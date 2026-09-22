#include "recorder.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

static std::string make_timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    gmtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%dT%H%M%SZ");
    return oss.str();
}

static std::string make_filename(const std::string& dir, const RecordingMeta& meta,
                                  const std::string& ext) {
    std::ostringstream oss;
    oss << dir;
    if (!dir.empty() && dir.back() != '/') oss << '/';
    oss << "iq_" << meta.timestamp_utc
        << "_" << static_cast<long long>(meta.center_freq_hz / 1e6) << "MHz"
        << "_" << static_cast<long long>(meta.sample_rate_hz / 1e6) << "Msps"
        << "_ch" << (meta.channel == 0 ? "A" : "B")
        << ext;
    return oss.str();
}

bool Recorder::start(const std::string& dir, const RecordingMeta& meta) {
    if (recording_.load()) stop();

    meta_ = meta;
    meta_.timestamp_utc = make_timestamp();

    try {
        fs::create_directories(dir);
    } catch (...) {}

    filename_ = make_filename(dir, meta_, ".bin");
    json_path_ = make_filename(dir, meta_, ".json");

    bin_file_.open(filename_, std::ios::binary | std::ios::out | std::ios::trunc);
    if (!bin_file_.is_open()) {
        std::cerr << "[Recorder] Failed to open: " << filename_ << "\n";
        return false;
    }

    bytes_written_.store(0);
    samples_written_.store(0);
    recording_.store(true);
    std::cout << "[Recorder] Started: " << filename_ << "\n";
    return true;
}

void Recorder::write(const std::vector<std::complex<float>>& samples) {
    if (!recording_.load() || !bin_file_.is_open()) return;

    // Write interleaved float32: I0 Q0 I1 Q1 ...
    static_assert(sizeof(float) == 4, "float must be 32-bit");
    const size_t n_floats = samples.size() * 2;
    bin_file_.write(reinterpret_cast<const char*>(samples.data()), n_floats * sizeof(float));

    bytes_written_.fetch_add(n_floats * sizeof(float));
    samples_written_.fetch_add(samples.size());
}

void Recorder::stop() {
    if (!recording_.load()) return;
    recording_.store(false);

    if (bin_file_.is_open()) {
        bin_file_.flush();
        bin_file_.close();
    }

    // Write JSON sidecar
    std::ofstream json(json_path_);
    if (json.is_open()) {
        json << "{\n"
             << "  \"drone_name\": \"" << meta_.drone_name << "\",\n"
             << "  \"center_freq_hz\": " << static_cast<long long>(meta_.center_freq_hz) << ",\n"
             << "  \"sample_rate_hz\": " << static_cast<long long>(meta_.sample_rate_hz) << ",\n"
             << "  \"gain_db\": " << meta_.gain_db << ",\n"
             << "  \"channel\": \"" << (meta_.channel == 0 ? "A" : "B") << "\",\n"
             << "  \"antenna\": \"RX2\",\n"
             << "  \"timestamp_utc\": \"" << meta_.timestamp_utc << "\",\n"
             << "  \"samples_written\": " << samples_written_.load() << ",\n"
             << "  \"bytes_written\": " << bytes_written_.load() << ",\n"
             << "  \"format\": \"complex_float32_interleaved\"\n"
             << "}\n";
        json.close();
    }

    std::cout << "[Recorder] Stopped. Samples: " << samples_written_.load()
              << ", Bytes: " << bytes_written_.load() << "\n";
    std::cout << "[Recorder] JSON: " << json_path_ << "\n";
}
