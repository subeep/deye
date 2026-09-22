#pragma once
#include <complex>
#include <vector>
#include <array>
#include <cstddef>
#include <fftw3.h>

// ─── Configuration ────────────────────────────────────────────────────────────
static constexpr int MAX_FFT_SIZE   = 4096;
static constexpr int STFT_HISTORY   = 256;   // Number of time slices in STFT/Spectrogram

// ─── Signal Processor ────────────────────────────────────────────────────────
// All methods called from the UI (main) thread only. Thread-safe via single-consumer design.
class SignalProcessor {
public:
    SignalProcessor();
    ~SignalProcessor();

    // Call once per frame with samples drained from the ring buffer
    void process(const std::vector<std::complex<float>>& samples, double sample_rate_hz);

    // Current FFT size setting
    void set_fft_size(int n);
    int  get_fft_size() const { return fft_size_; }

    // ─── FFT output ──────────────────────────────────────────────────────────
    // fft_freqs_[k] in Hz relative to center (negative to positive)
    // fft_power_[k] in dBFS
    const std::vector<float>& fft_freqs()  const { return fft_freqs_; }
    const std::vector<float>& fft_power()  const { return fft_power_; }
    const std::vector<float>& fft_avg()    const { return fft_avg_; }   // rolling average

    // ─── STFT output ─────────────────────────────────────────────────────────
    // stft_history_[row][bin] — row 0 is newest
    const std::vector<std::vector<float>>& stft_history() const { return stft_history_; }

    // ─── Spectrogram output ──────────────────────────────────────────────────
    // Flat row-major matrix [STFT_HISTORY × fft_size_], ready for ImPlot::PlotHeatmap
    const std::vector<float>& spectrogram_data() const { return spectrogram_flat_; }

    // ─── IQ time-domain ──────────────────────────────────────────────────────
    // Last N samples for waveform + constellation plots
    static constexpr int IQ_DISPLAY_LEN = 4096;
    const std::vector<float>& iq_i()  const { return iq_i_; }
    const std::vector<float>& iq_q()  const { return iq_q_; }
    const std::vector<float>& iq_t()  const { return iq_t_; }  // time axis (µs)

    // Overall stats
    float signal_power_dbfs() const { return signal_power_dbfs_; }
    double sample_rate()       const { return sample_rate_; }

private:
    void rebuild_plan();
    void apply_window(const std::complex<float>* in, int n);
    void compute_fft_power();
    void push_stft_row();
    void rebuild_hann_window(int n);

    int fft_size_{1024};

    fftwf_plan   plan_{nullptr};
    fftwf_complex* fft_in_{nullptr};
    fftwf_complex* fft_out_{nullptr};

    std::vector<float> hann_window_;

    // Accum buffer to gather enough samples for one FFT frame
    std::vector<std::complex<float>> accum_;

    // FFT output
    std::vector<float> fft_freqs_;
    std::vector<float> fft_power_;
    std::vector<float> fft_avg_;
    float avg_alpha_{0.15f}; // EMA smoothing factor

    // STFT history
    std::vector<std::vector<float>> stft_history_;
    std::vector<float> spectrogram_flat_;

    // IQ display buffers
    std::vector<float> iq_i_;
    std::vector<float> iq_q_;
    std::vector<float> iq_t_;

    float signal_power_dbfs_{-100.0f};
    double sample_rate_{1e6};

    int stft_hop_counter_{0};
    int stft_hop_size_;
};
