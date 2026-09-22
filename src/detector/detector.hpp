#pragma once
#include <atomic>
#include <complex>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace drone {
struct Observation {
    std::string protocol, link, evidence, serial, model;
    double frequency{}, power{}, timestamp{}, latitude{}, longitude{}, altitude{};
    bool confirmed{};
    uint64_t count{1};
};
struct Snapshot {
    std::string status{"Idle"};
    bool ready{};
    uint64_t samples{}, dropped{}, candidates{}, rejected{}, decoded{};
    double processing_ms{};
    std::vector<Observation> observations;
};
class Detector {
public:
    Detector() = default;
    ~Detector();
    void start();
    void stop();
    bool running() const { return running_; }
    void submit(const std::complex<float>* data, size_t count, double rate,
                double frequency, double timestamp, uint64_t epoch);
    Snapshot snapshot();
private:
    struct Chunk {
        std::vector<std::complex<float>> data;
        double rate{}, frequency{}, timestamp{};
        uint64_t epoch{};
    };
    void run();
    void status(const std::string& message, bool ready=false);
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> dropped_{0}, gap_{0};
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<Chunk> queue_;
    Snapshot snapshot_;
};
}
