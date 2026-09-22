#include "usrp_worker.hpp"
#include <uhd/exception.hpp>
#include <uhd/types/tune_request.hpp>
#include <uhd/stream.hpp>
#include <iostream>
#include <chrono>
#include <thread>

UsrpWorker::UsrpWorker() {
    // Pre-allocate ring slots
    for (auto& slot : ring.slots) {
        slot.data.resize(CHUNK_SAMPLES);
        slot.ready.store(false);
    }
}

UsrpWorker::~UsrpWorker() {
    disconnect();
}

void UsrpWorker::set_detector_sink(DetectorSink sink) {
    std::lock_guard<std::mutex> lock(detector_mutex_);
    detector_sink_ = std::move(sink);
}

// ─── Connect ─────────────────────────────────────────────────────────────────
bool UsrpWorker::connect(const std::string& device_args) {
    try {
        std::cout << "[USRP] Connecting: " << device_args << "\n";
        usrp_ = uhd::usrp::multi_usrp::make(device_args);

        // Apply initial settings to the desired channel
        int ch = desired_channel_.load();
        usrp_->set_rx_subdev_spec(
            ch == 0 ? uhd::usrp::subdev_spec_t("A:0") : uhd::usrp::subdev_spec_t("B:0"));
        usrp_->set_rx_antenna("RX2", 0);
        usrp_->set_rx_rate(desired_rate_.load(), 0);
        usrp_->set_rx_freq(uhd::tune_request_t(desired_freq_.load()), 0);
        usrp_->set_rx_gain(desired_gain_.load(), 0);

        current_freq_.store(usrp_->get_rx_freq(0));
        current_gain_.store(usrp_->get_rx_gain(0));
        current_rate_.store(usrp_->get_rx_rate(0));
        current_channel_.store(ch);

        std::cout << "[USRP] Freq: " << current_freq_.load() / 1e6 << " MHz\n";
        std::cout << "[USRP] Rate: " << current_rate_.load() / 1e6 << " Msps\n";
        std::cout << "[USRP] Gain: " << current_gain_.load() << " dB\n";

        // Wait for LO lock
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Build the RX streamer (once, from the main thread)
        if (!rebuild_stream()) return false;

        connected_.store(true);
        start_stream();
        return true;

    } catch (const uhd::exception& e) {
        last_error_ = std::string("UHD: ") + e.what();
        std::cerr << "[USRP] Error: " << last_error_ << "\n";
        return false;
    } catch (const std::exception& e) {
        last_error_ = e.what();
        std::cerr << "[USRP] Error: " << last_error_ << "\n";
        return false;
    }
}

// ─── Disconnect ───────────────────────────────────────────────────────────────
void UsrpWorker::disconnect() {
    stop_stream();
    connected_.store(false);
    {
        std::lock_guard<std::mutex> lk(config_mutex_);
        rx_stream_.reset();
        usrp_.reset();
    }
}

// ─── Safe setters (freq/gain) — applied mid-stream in RX thread ───────────────
void UsrpWorker::set_freq(double freq_hz) {
    desired_freq_.store(freq_hz);
    freq_changed_.store(true, std::memory_order_release);
}

void UsrpWorker::set_gain(double gain_db) {
    desired_gain_.store(gain_db);
    gain_changed_.store(true, std::memory_order_release);
}

// ─── Unsafe setters — stop streaming, reconfigure from main thread, restart ───
void UsrpWorker::set_sample_rate(double rate_hz) {
    if (!connected_.load()) {
        desired_rate_.store(rate_hz);
        return;
    }
    try {
        bool was_running = running_.load();
        if (was_running) stop_stream();

        {
            std::lock_guard<std::mutex> lk(config_mutex_);
            usrp_->set_rx_rate(rate_hz, 0);
            current_rate_.store(usrp_->get_rx_rate(0));
            desired_rate_.store(current_rate_.load());
            std::cout << "[USRP] Rate changed: " << current_rate_.load() / 1e6 << " Msps\n";
            if (!rebuild_stream_locked()) return;
        }

        if (was_running) start_stream();
    } catch (const uhd::exception& e) {
        last_error_ = std::string("UHD rate: ") + e.what();
        std::cerr << "[USRP] " << last_error_ << "\n";
    }
}

void UsrpWorker::set_channel(int ch) {
    if (!connected_.load()) {
        desired_channel_.store(ch);
        return;
    }
    try {
        bool was_running = running_.load();
        if (was_running) stop_stream();

        {
            std::lock_guard<std::mutex> lk(config_mutex_);
            usrp_->set_rx_subdev_spec(
                ch == 0 ? uhd::usrp::subdev_spec_t("A:0")
                        : uhd::usrp::subdev_spec_t("B:0"));
            usrp_->set_rx_antenna("RX2", 0);
            // Re-apply current freq/gain/rate on new channel
            usrp_->set_rx_rate(current_rate_.load(), 0);
            usrp_->set_rx_freq(uhd::tune_request_t(current_freq_.load()), 0);
            usrp_->set_rx_gain(current_gain_.load(), 0);
            current_channel_.store(ch);
            desired_channel_.store(ch);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::cout << "[USRP] Channel changed: " << (ch == 0 ? "A" : "B") << "\n";
            if (!rebuild_stream_locked()) return;
        }

        if (was_running) start_stream();
    } catch (const uhd::exception& e) {
        last_error_ = std::string("UHD channel: ") + e.what();
        std::cerr << "[USRP] " << last_error_ << "\n";
    }
}

// ─── Internal helpers ────────────────────────────────────────────────────────

bool UsrpWorker::rebuild_stream() {
    std::lock_guard<std::mutex> lk(config_mutex_);
    return rebuild_stream_locked();
}

// Must be called with config_mutex_ held
bool UsrpWorker::rebuild_stream_locked() {
    try {
        // Fully release any existing stream before creating a new one
        if (rx_stream_) {
            rx_stream_.reset();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        uhd::stream_args_t stream_args("fc32", "sc16");
        stream_args.channels = {0};
        rx_stream_ = usrp_->get_rx_stream(stream_args);
        return true;
    } catch (const uhd::exception& e) {
        last_error_ = std::string("UHD stream: ") + e.what();
        std::cerr << "[USRP] " << last_error_ << "\n";
        return false;
    }
}

void UsrpWorker::stop_stream() {
    if (running_.exchange(false)) {
        if (rx_thread_.joinable()) rx_thread_.join();
    }
}

void UsrpWorker::start_stream() {
    ++detector_epoch_;
    detector_settle_samples_ = static_cast<size_t>(current_rate_.load() * .05);
    running_.store(true);
    rx_thread_ = std::thread(&UsrpWorker::rx_thread_func, this);
}

// ─── Applied only inside the RX thread — SAFE mid-stream operations ──────────
void UsrpWorker::apply_safe_changes() {
    // These UHD calls are documented as safe to invoke during streaming
    try {
        if (freq_changed_.exchange(false, std::memory_order_acq_rel)) {
            usrp_->set_rx_freq(uhd::tune_request_t(desired_freq_.load()), 0);
            current_freq_.store(usrp_->get_rx_freq(0));
            ++detector_epoch_;
            detector_settle_samples_ = static_cast<size_t>(current_rate_.load() * .05);
        }
        if (gain_changed_.exchange(false, std::memory_order_acq_rel)) {
            usrp_->set_rx_gain(desired_gain_.load(), 0);
            current_gain_.store(usrp_->get_rx_gain(0));
        }
    } catch (const uhd::exception& e) {
        last_error_ = std::string("UHD tune: ") + e.what();
    }
}

// ─── RX thread ───────────────────────────────────────────────────────────────
void UsrpWorker::rx_thread_func() {
    // Issue continuous stream command
    {
        uhd::stream_cmd_t start_cmd(uhd::stream_cmd_t::STREAM_MODE_START_CONTINUOUS);
        start_cmd.stream_now = true;
        rx_stream_->issue_stream_cmd(start_cmd);
    }

    uhd::rx_metadata_t md;
    std::vector<std::complex<float>> detector_scratch(CHUNK_SAMPLES);

    while (running_.load(std::memory_order_relaxed)) {
        // Apply freq/gain changes (safe mid-stream)
        apply_safe_changes();

        // Get a ring buffer slot
        RingBuffer::Slot* slot = ring.claim_write_slot();
        if (!slot) {
            overflow_count.fetch_add(CHUNK_SAMPLES, std::memory_order_relaxed);
            if (!detector_active.load()) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
        }

        if (slot) slot->data.resize(CHUNK_SAMPLES);
        auto* received = slot ? slot->data.data() : detector_scratch.data();
        size_t num_rx = rx_stream_->recv(received, CHUNK_SAMPLES, md, 2.0, false);

        if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_TIMEOUT) {
            ++detector_epoch_;
            continue;
        }
        if (md.error_code == uhd::rx_metadata_t::ERROR_CODE_OVERFLOW) {
            overflow_count.fetch_add(CHUNK_SAMPLES, std::memory_order_relaxed);
            ++detector_epoch_;
        }
        if (md.error_code != uhd::rx_metadata_t::ERROR_CODE_NONE &&
            md.error_code != uhd::rx_metadata_t::ERROR_CODE_OVERFLOW) {
            last_error_ = "RX error: " + md.strerror();
            ++detector_epoch_;
            continue;
        }

        if (num_rx > 0) {
            if (detector_settle_samples_) {
                detector_settle_samples_ -= std::min(detector_settle_samples_, num_rx);
            } else if (detector_active.load()) {
                std::lock_guard<std::mutex> lock(detector_mutex_);
                if (detector_sink_) detector_sink_(received, num_rx, current_rate_.load(),
                    current_freq_.load(), md.has_time_spec ? md.time_spec.get_real_secs() : 0.0,
                    detector_epoch_);
            }
            if (slot) {
                slot->data.resize(num_rx);
                ring.commit_write();
            }
        }
    }

    // Stop streaming
    try {
        uhd::stream_cmd_t stop_cmd(uhd::stream_cmd_t::STREAM_MODE_STOP_CONTINUOUS);
        rx_stream_->issue_stream_cmd(stop_cmd);
        // Drain any remaining data
        uhd::rx_metadata_t drain_md;
        std::vector<std::complex<float>> drain_buf(4096);
        for (int i = 0; i < 10; ++i) {
            size_t n = rx_stream_->recv(drain_buf.data(), drain_buf.size(), drain_md, 0.1, false);
            if (n == 0) break;
        }
    } catch (...) {}
}
