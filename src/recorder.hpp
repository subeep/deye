#pragma once
#include <string>
#include <fstream>
#include <complex>
#include <vector>
#include <cstdint>
#include <atomic>

struct RecordingMeta {
    double center_freq_hz{0};
    double sample_rate_hz{0};
    double gain_db{0};
    int    channel{0};          // 0=A, 1=B
    std::string drone_name;     // Drone identifier
    std::string timestamp_utc;
};

class Recorder {
public:
    Recorder() = default;
    ~Recorder() { stop(); }

    // Open a new recording file in dir. Returns false if dir can't be opened.
    bool start(const std::string& dir, const RecordingMeta& meta);

    // Append IQ samples (interleaved float32 I,Q,I,Q,...)
    void write(const std::vector<std::complex<float>>& samples);

    // Flush + close files, write JSON sidecar
    void stop();

    bool is_recording() const { return recording_.load(); }
    uint64_t bytes_written() const { return bytes_written_.load(); }
    uint64_t samples_written() const { return samples_written_.load(); }
    std::string current_filename() const { return filename_; }

private:
    std::ofstream   bin_file_;
    std::string     filename_;
    std::string     json_path_;
    RecordingMeta   meta_;

    std::atomic<bool>     recording_{false};
    std::atomic<uint64_t> bytes_written_{0};
    std::atomic<uint64_t> samples_written_{0};
};
