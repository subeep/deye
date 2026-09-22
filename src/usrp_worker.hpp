#pragma once
#include <uhd/usrp/multi_usrp.hpp>
#include <complex>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <array>
#include <string>
#include <functional>

// ─── Ring Buffer ─────────────────────────────────────────────────────────────
// Lock-free single-producer / single-consumer ring buffer of IQ sample chunks.
static constexpr int RING_SLOTS = 64;
static constexpr size_t CHUNK_SAMPLES = 8192;

struct RingBuffer {
    struct Slot {
        std::vector<std::complex<float>> data;
        std::atomic<bool> ready{false};
    };
    std::array<Slot, RING_SLOTS> slots;
    std::atomic<int> write_idx{0};
    std::atomic<int> read_idx{0};

    // Producer: returns slot to write into, or nullptr if full
    Slot* claim_write_slot() {
        int w = write_idx.load(std::memory_order_relaxed);
        int next = (w + 1) % RING_SLOTS;
        if (next == read_idx.load(std::memory_order_acquire)) return nullptr; // full
        return &slots[w];
    }
    void commit_write() {
        int w = write_idx.load(std::memory_order_relaxed);
        slots[w].ready.store(true, std::memory_order_release);
        write_idx.store((w + 1) % RING_SLOTS, std::memory_order_release);
    }

    // Consumer: returns slot to read, or nullptr if empty
    Slot* peek_read_slot() {
        int r = read_idx.load(std::memory_order_relaxed);
        if (r == write_idx.load(std::memory_order_acquire)) return nullptr; // empty
        if (!slots[r].ready.load(std::memory_order_acquire)) return nullptr;
        return &slots[r];
    }
    void consume() {
        int r = read_idx.load(std::memory_order_relaxed);
        slots[r].ready.store(false, std::memory_order_release);
        read_idx.store((r + 1) % RING_SLOTS, std::memory_order_release);
    }
};

// ─── USRP Worker ─────────────────────────────────────────────────────────────
class UsrpWorker {
public:
    UsrpWorker();
    ~UsrpWorker();

    // Connect to device and start streaming
    bool connect(const std::string& device_args = "addr=192.168.10.2");
    void disconnect();
    bool is_connected() const { return connected_.load(); }

    // Safe mid-stream setters — applied inside RX thread on next iteration
    void set_freq(double freq_hz);
    void set_gain(double gain_db);

    // Unsafe mid-stream setters — STOP streaming, reconfigure, RESTART
    // Must be called from the UI/main thread (NOT the RX thread).
    void set_sample_rate(double rate_hz);
    void set_channel(int ch); // 0 = Radio#0 (A), 1 = Radio#1 (B)

    // Getters
    double get_freq() const { return current_freq_.load(); }
    double get_gain() const { return current_gain_.load(); }
    double get_sample_rate() const { return current_rate_.load(); }
    int    get_channel() const { return current_channel_.load(); }

    std::string get_last_error() const { return last_error_; }

    // The ring buffer — UI thread reads from this
    RingBuffer ring;

    // Overflow counter (samples dropped due to full ring)
    std::atomic<uint64_t> overflow_count{0};

    // Independent, nonblocking detector tap. Recorder remains the ring consumer.
    using DetectorSink = std::function<void(const std::complex<float>*, size_t,
                                            double, double, double, uint64_t)>;
    void set_detector_sink(DetectorSink sink);
    std::atomic<bool> detector_active{false};

private:
    void rx_thread_func();

    // ONLY freq/gain — safe to call while streaming in UHD
    void apply_safe_changes();

    // Rebuild streamer. Must be called with rx_thread_ NOT running.
    bool rebuild_stream();
    // Same but assumes config_mutex_ already held by caller.
    bool rebuild_stream_locked();

    // Stop streaming thread (blocks until joined). Does NOT reset connected_.
    void stop_stream();

    // Start streaming thread. Assumes usrp_ and rx_stream_ are valid.
    void start_stream();

    uhd::usrp::multi_usrp::sptr usrp_;
    uhd::rx_streamer::sptr       rx_stream_;

    std::thread rx_thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connected_{false};

    // Protects usrp_ / rx_stream_ during stop/reconfigure/start
    std::mutex config_mutex_;

    // Desired settings (written from UI thread, read in RX thread)
    std::atomic<double> desired_freq_{433e6};
    std::atomic<double> desired_gain_{20.0};
    std::atomic<double> desired_rate_{1e6};
    std::atomic<int>    desired_channel_{0};

    // Current applied settings
    std::atomic<double> current_freq_{433e6};
    std::atomic<double> current_gain_{20.0};
    std::atomic<double> current_rate_{1e6};
    std::atomic<int>    current_channel_{0};

    // Flags to trigger in-flight retune (safe changes only)
    std::atomic<bool> freq_changed_{false};
    std::atomic<bool> gain_changed_{false};

    std::string last_error_;
    std::mutex detector_mutex_;
    DetectorSink detector_sink_;
    uint64_t detector_epoch_{0}; // receive thread / stopped-stream updates only
    size_t detector_settle_samples_{0};
};
