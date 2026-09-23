#pragma once
#include "detector.hpp"
#include "app_state.hpp"
#include <chrono>

namespace drone {
class Controller {
public:
    void tick(AppState& state, UsrpWorker& radio, bool recording);
    void submit(const std::complex<float>* data, size_t count, double rate,
                double frequency, double timestamp, uint64_t epoch) {
        detector_.submit(data,count,rate,frequency,timestamp,epoch);
    }
    void shutdown(UsrpWorker& radio);
private:
    Detector detector_;
    bool active_{false}, tuned_{false};
    std::vector<double> sequence_;
    size_t position_{};
    uint64_t last_decoded_{};
    uint64_t overflow_base_{}, timeout_base_{}, error_base_{}, settle_base_{}, settle_ns_base_{};
    std::vector<double> away_seconds_;
    double previous_frequency_{};
    std::chrono::steady_clock::time_point coverage_tick_;
    std::chrono::steady_clock::time_point deadline_, tune_deadline_;
};
}
