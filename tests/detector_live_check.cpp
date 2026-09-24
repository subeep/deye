// Manual receive-only hardware check; intentionally not registered with CTest.
#include "usrp_worker.hpp"
#include "detector/detector.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    if (argc<2 || std::filesystem::exists(argv[1])) {
        std::cerr << "Usage: detector_live_check NEW_REPORT.json [--dc-block] [--rate 20000000|25000000] [--fixed]\n"; return 1;
    }
    bool dc=false, fixed=false, capture=false; double requested_rate=25e6;
    for (int i=2;i<argc;++i) {
        const std::string arg=argv[i];
        if (arg=="--capture") capture=true;
        else if (arg=="--dc-block") dc=true;
        else if (arg=="--fixed") fixed=true;
        else if (arg=="--rate" && i+1<argc) {
            const std::string rate=argv[++i];
            if (rate=="20000000") requested_rate=20e6;
            else if (rate=="25000000") requested_rate=25e6;
            else return 1;
        } else return 1;
    }
    const std::string iq_path=std::string(argv[1])+".cf32";
    if (capture && (!fixed || std::filesystem::exists(iq_path))) return 1;
    // Bounded two-second evidence buffer. Disk writes happen only after RX stops.
    std::vector<std::complex<float>> evidence(capture ? static_cast<size_t>(requested_rate*2) : 0);
    size_t captured=0; uint64_t evidence_epoch=0, capture_resets=0;
    double evidence_time=0, expected_time=0, evidence_rate=0, evidence_frequency=0;
    using Clock=std::chrono::steady_clock;
    UsrpWorker radio;
    drone::Detector detector;
    radio.set_detector_sink([&](const std::complex<float>* data,size_t n,double rate,double freq,double time,uint64_t epoch) {
        if (capture && captured<evidence.size()) {
            if (captured && (epoch!=evidence_epoch || rate!=evidence_rate || freq!=evidence_frequency ||
                std::abs(time-expected_time)>std::max(2/rate,1e-9))) { captured=0; ++capture_resets; }
            if (!captured) { evidence_epoch=epoch; evidence_time=time; evidence_rate=rate; evidence_frequency=freq; }
            const auto take=std::min(n,evidence.size()-captured);
            std::memcpy(evidence.data()+captured,data,take*sizeof(*data)); captured+=take;
            expected_time=time+n/rate;
        }
        detector.submit(data,n,rate,freq,time,epoch);
    });
    detector.start(dc);
    for (int i=0;i<100 && !detector.snapshot().ready;++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    if (!detector.snapshot().ready) { std::cerr << detector.snapshot().status << '\n'; return 1; }
    radio.set_sample_rate(requested_rate); radio.set_freq(fixed ? 2444.5e6 : 2434.5e6); radio.set_gain(20);
    if (!radio.connect()) { std::cerr << radio.get_last_error() << '\n'; return 2; }
    radio.detector_active=true;
    boost::property_tree::ptree report, phases;
    report.put("fixed_channel",fixed); report.put("requested_rate_hz",requested_rate);
    report.put("dc_block",dc); report.put("device","addr=192.168.10.2");
    report.put("antenna","RX2"); report.put("gain_db",radio.get_gain());
    report.put("sample_rate_hz",radio.get_sample_rate());
    report.put("ground_truth","Aircraft state/firmware not independently verified; not an accuracy test");
    double max_queue_age=0, max_result_age=0;
    bool tuned_ok=true;
    const std::vector<double> targets=fixed ? std::vector<double>{2444.5e6} :
        std::vector<double>{2434.5e6,2444.5e6,2459.5e6,2474.5e6,2414.5e6,2429.502441e6,2399.5e6};
    for (double target:targets) {
        auto start=Clock::now(); radio.set_freq(target);
        while (std::abs(radio.get_freq()-target)>5000 && Clock::now()-start<std::chrono::seconds(2))
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        const bool tuned=std::abs(radio.get_freq()-target)<=5000;
        tuned_ok &= tuned;
        auto deadline=Clock::now()+std::chrono::seconds(fixed ? 10 : 3);
        while (Clock::now()<deadline) {
            auto s=detector.snapshot();
            max_queue_age=std::max(max_queue_age,s.oldest_queue_ms);
            max_result_age=std::max(max_result_age,s.end_to_end_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        auto s=detector.snapshot(); boost::property_tree::ptree phase;
        phase.put("target_hz",target); phase.put("actual_hz",radio.get_freq()); phase.put("tuned",tuned);
        phase.put("elapsed_s",std::chrono::duration<double>(Clock::now()-start).count());
        phase.put("analyzed_cumulative",s.samples); phase.put("queue_dropped_cumulative",s.dropped);
        phase.put("valid_cumulative",s.decoded); phase.put("queue_depth",s.queue_depth);
        phase.put("oldest_queue_ms",s.oldest_queue_ms); phase.put("processing_ms",s.processing_ms);
        phases.push_back({"",phase});
        std::cout << "RX " << radio.get_freq()/1e6 << " MHz: analyzed=" << s.samples << " dropped=" << s.dropped << " valid=" << s.decoded << std::endl;
    }
    radio.detector_active=false;
    const bool worker_alive=detector.running();
    radio.disconnect(); detector.stop();
    auto s=detector.snapshot(); radio.set_detector_sink({});
    if (capture) {
        std::ofstream file(iq_path,std::ios::binary);
        file.write(reinterpret_cast<const char*>(evidence.data()),captured*sizeof(evidence[0]));
        if (!file) { std::cerr << "Evidence write failed\n"; return 4; }
        report.put("capture_path",iq_path); report.put("capture_samples",captured);
        report.put("capture_complete",captured==evidence.size()); report.put("capture_resets",capture_resets);
        report.put("capture_rate_hz",evidence_rate); report.put("capture_frequency_hz",evidence_frequency);
        report.put("capture_timestamp",evidence_time); report.put("capture_format","cf32_le");
    }
    report.add_child("phases",phases);
    report.put("analyzed_samples",s.samples); report.put("queue_dropped_samples",s.dropped);
    report.put("partial_discarded_samples",s.partial_discarded); report.put("stop_discarded_samples",s.stop_discarded);
    report.put("continuity_breaks",s.discontinuities); report.put("analysis_errors",s.analysis_errors);
    report.put("valid_packets",s.decoded); report.put("candidates",s.candidates); report.put("rejected",s.rejected);
    report.put("rx_overflow_events",radio.detector_rx_overflows.load());
    report.put("rx_timeout_events",radio.detector_rx_timeouts.load()); report.put("rx_error_events",radio.detector_rx_errors.load());
    report.put("settle_skipped_samples",radio.detector_settle_skipped.load());
    report.put("settle_seconds",radio.detector_settle_ns.load()/1e9);
    report.put("max_queue_age_ms",max_queue_age); report.put("max_result_age_ms",max_result_age);
    report.put("status",s.status);
    const bool pass=(!capture || captured==evidence.size()) && worker_alive && tuned_ok && s.samples>100000 && s.analysis_errors==0;
    report.put("health_check_pass",pass);
    report.put("interpretation","Cumulative decoder results may lag tuning; no per-channel packet recovery or exact RF loss inferred");
    boost::property_tree::write_json(argv[1],report);
    return pass ? 0 : 3;
}
