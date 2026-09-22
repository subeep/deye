#include "signal_proc.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cassert>

static constexpr float PI = 3.14159265358979f;
static constexpr float TWO_PI = 2.0f * PI;

SignalProcessor::SignalProcessor() {
    stft_history_.resize(STFT_HISTORY);
    rebuild_plan();
}

SignalProcessor::~SignalProcessor() {
    if (plan_)    fftwf_destroy_plan(plan_);
    if (fft_in_)  fftwf_free(fft_in_);
    if (fft_out_) fftwf_free(fft_out_);
}

void SignalProcessor::set_fft_size(int n) {
    if (n == fft_size_) return;
    fft_size_ = n;
    rebuild_plan();
}

void SignalProcessor::rebuild_plan() {
    if (plan_)    { fftwf_destroy_plan(plan_); plan_ = nullptr; }
    if (fft_in_)  { fftwf_free(fft_in_);  fft_in_  = nullptr; }
    if (fft_out_) { fftwf_free(fft_out_); fft_out_ = nullptr; }

    fft_in_  = fftwf_alloc_complex(fft_size_);
    fft_out_ = fftwf_alloc_complex(fft_size_);
    plan_    = fftwf_plan_dft_1d(fft_size_, fft_in_, fft_out_, FFTW_FORWARD, FFTW_ESTIMATE);

    rebuild_hann_window(fft_size_);

    fft_freqs_.resize(fft_size_);
    fft_power_.resize(fft_size_, -120.0f);
    fft_avg_.resize(fft_size_, -120.0f);

    for (auto& row : stft_history_)
        row.assign(fft_size_, -120.0f);
    spectrogram_flat_.assign(STFT_HISTORY * fft_size_, -120.0f);

    stft_hop_size_ = fft_size_ / 2;
    stft_hop_counter_ = 0;

    accum_.clear();
    accum_.reserve(fft_size_ * 2);

    iq_i_.assign(IQ_DISPLAY_LEN, 0.0f);
    iq_q_.assign(IQ_DISPLAY_LEN, 0.0f);
    iq_t_.resize(IQ_DISPLAY_LEN);
}

void SignalProcessor::rebuild_hann_window(int n) {
    hann_window_.resize(n);
    for (int i = 0; i < n; ++i)
        hann_window_[i] = 0.5f * (1.0f - std::cos(TWO_PI * i / (n - 1)));
}

void SignalProcessor::apply_window(const std::complex<float>* in, int n) {
    for (int i = 0; i < n; ++i) {
        float w = hann_window_[i];
        fft_in_[i][0] = in[i].real() * w;
        fft_in_[i][1] = in[i].imag() * w;
    }
}

void SignalProcessor::compute_fft_power() {
    fftwf_execute(plan_);

    // Frequency axis — FFT-shifted: negative freqs first (bins N/2..N-1), then positive
    double bin_hz = sample_rate_ / fft_size_;
    for (int k = 0; k < fft_size_; ++k) {
        int shifted_k = (k + fft_size_ / 2) % fft_size_;
        double freq = (shifted_k - fft_size_ / 2) * bin_hz;
        fft_freqs_[k] = static_cast<float>(freq);

        float re = fft_out_[shifted_k][0];
        float im = fft_out_[shifted_k][1];
        float mag2 = re * re + im * im;
        float power_db = 10.0f * std::log10(mag2 / (fft_size_ * fft_size_) + 1e-20f);
        fft_power_[k] = power_db;
    }

    // EMA rolling average
    for (int k = 0; k < fft_size_; ++k)
        fft_avg_[k] = avg_alpha_ * fft_power_[k] + (1.0f - avg_alpha_) * fft_avg_[k];
}

void SignalProcessor::push_stft_row() {
    // Shift history down (row 0 = newest)
    for (int r = STFT_HISTORY - 1; r > 0; --r)
        stft_history_[r] = stft_history_[r - 1];

    stft_history_[0] = fft_power_;

    // Rebuild flat spectrogram (row-major: row 0 = newest = bottom of heatmap)
    for (int r = 0; r < STFT_HISTORY; ++r)
        for (int k = 0; k < fft_size_; ++k)
            spectrogram_flat_[r * fft_size_ + k] = stft_history_[r][k];
}

void SignalProcessor::process(const std::vector<std::complex<float>>& samples, double sample_rate_hz) {
    if (samples.empty()) return;
    sample_rate_ = sample_rate_hz;

    // ─── IQ time-domain display buffer (newest samples, ring)
    int n = static_cast<int>(samples.size());
    if (n >= IQ_DISPLAY_LEN) {
        // Take last IQ_DISPLAY_LEN samples
        int start = n - IQ_DISPLAY_LEN;
        for (int i = 0; i < IQ_DISPLAY_LEN; ++i) {
            iq_i_[i] = samples[start + i].real();
            iq_q_[i] = samples[start + i].imag();
        }
    } else {
        // Shift existing data left, append new samples
        int keep = IQ_DISPLAY_LEN - n;
        std::move(iq_i_.begin() + n, iq_i_.end(), iq_i_.begin());
        std::move(iq_q_.begin() + n, iq_q_.end(), iq_q_.begin());
        for (int i = 0; i < n; ++i) {
            iq_i_[keep + i] = samples[i].real();
            iq_q_[keep + i] = samples[i].imag();
        }
    }

    // Time axis in microseconds
    double us_per_sample = 1e6 / sample_rate_;
    for (int i = 0; i < IQ_DISPLAY_LEN; ++i)
        iq_t_[i] = static_cast<float>((i - IQ_DISPLAY_LEN + 1) * us_per_sample);

    // ─── Signal power (RMS over batch)
    float sum2 = 0.0f;
    for (auto& s : samples) {
        float re = s.real(), im = s.imag();
        sum2 += re * re + im * im;
    }
    float rms = std::sqrt(sum2 / samples.size());
    signal_power_dbfs_ = 20.0f * std::log10(rms + 1e-12f);

    // ─── Accumulate samples for FFT/STFT
    for (auto& s : samples)
        accum_.push_back(s);

    // Process complete FFT frames
    while (static_cast<int>(accum_.size()) >= fft_size_) {
        apply_window(accum_.data(), fft_size_);
        compute_fft_power();

        // STFT: push a new row every hop_size samples
        stft_hop_counter_ += stft_hop_size_;
        if (stft_hop_counter_ >= stft_hop_size_) {
            push_stft_row();
            stft_hop_counter_ = 0;
        }

        // Slide by hop size (50% overlap)
        int advance = stft_hop_size_;
        accum_.erase(accum_.begin(), accum_.begin() + advance);
    }
}
