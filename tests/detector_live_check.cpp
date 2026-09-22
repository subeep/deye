// Manual receive-only hardware check; intentionally not registered with CTest.
#include "usrp_worker.hpp"
#include "detector/detector.hpp"
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    UsrpWorker radio;
    drone::Detector detector;
    radio.set_detector_sink([&](const std::complex<float>* data,size_t n,double rate,double freq,double time,uint64_t epoch) {
        detector.submit(data,n,rate,freq,time,epoch);
    });
    detector.start();
    for (int i=0;i<100 && !detector.snapshot().ready;++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (!detector.snapshot().ready) { std::cerr << detector.snapshot().status << '\n'; return 1; }
    radio.set_sample_rate(2e6); radio.set_freq(915e6); radio.set_gain(20);
    if (!radio.connect()) { std::cerr << radio.get_last_error() << '\n'; return 2; }
    radio.detector_active=true;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    radio.set_freq(916e6);
    std::this_thread::sleep_for(std::chrono::seconds(2));
    radio.set_sample_rate(25e6); radio.set_freq(2444.5e6);
    std::this_thread::sleep_for(std::chrono::seconds(3));
    radio.detector_active=false;
    radio.disconnect();
    auto s=detector.snapshot();
    detector.stop(); radio.set_detector_sink({});
    std::cout << "Analyzed=" << s.samples << " dropped=" << s.dropped
              << " observations=" << s.observations.size() << " valid_DJI=" << s.decoded
              << " status=" << s.status << '\n';
    return s.samples>100000 ? 0 : 3;
}
