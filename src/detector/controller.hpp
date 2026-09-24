#pragma once
#include "detector.hpp"
#include "app_state.hpp"
#include <chrono>
#include <cmath>

namespace drone {
// Only newly acquired, unexpired evidence from this tuning epoch may hold.
inline std::chrono::steady_clock::time_point fresh_hold_evidence(
    const std::vector<Observation>& observations, double target, uint64_t epoch,
    std::chrono::steady_clock::time_point tune_started,
    std::chrono::steady_clock::time_point consumed,
    std::chrono::steady_clock::time_point now, std::chrono::milliseconds hold) {
    auto latest=consumed;
    for (const auto& o:observations)
        if (o.confirmed && o.receiver_epoch==epoch && std::abs(o.frequency-target)<5000 &&
            o.acquired_at>=tune_started && o.acquired_at>latest && o.acquired_at<=now &&
            o.acquired_at+hold>now) latest=o.acquired_at;
    return latest;
}
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
    std::chrono::steady_clock::time_point tune_started_{}, consumed_evidence_{};
    uint64_t overflow_base_{}, timeout_base_{}, error_base_{}, settle_base_{}, settle_ns_base_{};
    std::vector<double> away_seconds_;
    double previous_frequency_{};
    std::chrono::steady_clock::time_point coverage_tick_;
    std::chrono::steady_clock::time_point deadline_, tune_deadline_;
};
}
