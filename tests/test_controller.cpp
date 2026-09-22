// Deterministic radio double: exercises detector ownership without RF hardware.
#include "detector/controller.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

UsrpWorker::UsrpWorker() = default;
UsrpWorker::~UsrpWorker() = default;
bool UsrpWorker::connect(const std::string&) { connected_=true; return true; }
void UsrpWorker::set_sample_rate(double value) { current_rate_=value; }
void UsrpWorker::set_freq(double value) { current_freq_=value; }
void UsrpWorker::set_gain(double value) { current_gain_=value; }

static void check(bool condition) { if (!condition) throw std::runtime_error("Controller ownership check failed"); }
int main() {
    AppState state;
    UsrpWorker radio;
    drone::Controller controller;
    radio.connect();
    radio.set_freq(433e6); radio.set_sample_rate(1e6); radio.set_gain(12);
    state.dd_start_requested=true;
    controller.tick(state,radio,true);
    check(!state.dd_detection_running && radio.get_freq()==433e6);
    state.dd_start_requested=true;
    controller.tick(state,radio,false);
    check(state.dd_detection_running && radio.get_sample_rate()==25e6);
    check(radio.detector_active && state.dd_target_hz==radio.get_freq());
    const double detector_frequency=radio.get_freq();
    state.dd_stop_requested=true; state.dd_status_msg="Stop requested";
    controller.tick(state,radio,false);
    check(!state.dd_detection_running && !radio.detector_active);
    check(radio.get_freq()==detector_frequency && radio.get_sample_rate()==25e6 && radio.get_gain()==30);
    check(state.dd_status_msg=="Stopped; detector frequency and RX settings retained.");
    // Recording takes ownership: no restore/tune is allowed once it is active.
    state.dd_start_requested=true;
    controller.tick(state,radio,false);
    auto recording_frequency=radio.get_freq();
    controller.tick(state,radio,true);
    check(!state.dd_detection_running && radio.get_freq()==recording_frequency);
    check(radio.get_sample_rate()==25e6 && !radio.detector_active);
    // Connecting from the detector also uses its selected profile, not 433 MHz.
    AppState fresh;
    UsrpWorker disconnected;
    fresh.dd_profile_channel=1;
    fresh.dd_connect_requested=true;
    controller.tick(fresh,disconnected,false);
    check(disconnected.get_freq()==2414.5e6 && disconnected.get_sample_rate()==25e6);
    std::cout << "Recording exclusion, retained detector tuning and profile connection passed\n";
}
