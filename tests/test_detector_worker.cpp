#include "detector/detector.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <sstream>
#include <boost/property_tree/json_parser.hpp>

int main(int argc, char** argv) {
    if (argc!=2) return 2;
    drone::Detector detector;
    detector.start();
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (!detector.snapshot().ready && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (!detector.snapshot().ready) throw std::runtime_error(detector.snapshot().status);
    std::ifstream file(argv[1],std::ios::binary);
    if (!file) return 3;
    std::vector<std::complex<float>> samples(1024*1024);
    file.read(reinterpret_cast<char*>(samples.data()),samples.size()*8);
    // Repeat the capture's real background into a complete worker batch.
    // Zero padding would change the packetizer's estimated noise floor.
    const size_t captured=static_cast<size_t>(file.gcount())/8;
    if (!captured) return 4;
    for (size_t i=captured;i<samples.size();++i) samples[i]=samples[i%captured];
    for (size_t i=0;i<samples.size();i+=8192) {
        detector.submit(samples.data()+i,8192,50e6,2444.5e6,i/50e6,1);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (!detector.snapshot().decoded && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (!detector.snapshot().decoded) {
        auto s=detector.snapshot();
        std::cerr << s.status << " samples=" << s.samples << " dropped=" << s.dropped << " candidates=" << s.candidates << '\n';
        detector.stop();
        throw std::runtime_error("No valid DJI result from subprocess worker");
    }
    auto health=detector.snapshot();
    if (health.end_to_end_ms<health.queue_age_ms || health.processing_ms<=0 || health.analysis_errors)
        throw std::runtime_error("Invalid processing health metrics");
    bool telemetry_received=false;
    for (const auto& o:health.observations) if (o.confirmed) {
        boost::property_tree::ptree packet;
        std::istringstream input(o.packet_json);
        boost::property_tree::read_json(input,packet);
        if (packet.get<int>("sequence_number")!=591 || packet.get<std::string>("raw_payload_hex").size()!=182)
            throw std::runtime_error("Packet export lost decoded data");
        for (const auto& f:o.telemetry)
            if (f.name=="sequence_number" && f.value=="591") telemetry_received=true;
        if (o.position_status=="Unavailable") throw std::runtime_error("Missing position status");
    }
    if (!telemetry_received) throw std::runtime_error("Telemetry did not cross Python/C++ boundary");
    for (int i=0;i<2000;++i) detector.submit(samples.data(),8192,50e6,2444.5e6,1,2);
    if (!detector.snapshot().dropped) throw std::runtime_error("Queue limit was not enforced");
    auto start=std::chrono::steady_clock::now();
    detector.stop();
    if (std::chrono::steady_clock::now()-start>std::chrono::seconds(2)) throw std::runtime_error("Slow cancellation");
    detector.start(true);
    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (!detector.snapshot().ready && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    if (!detector.snapshot().ready) throw std::runtime_error("DC worker startup failed");
    // A partial batch before a gap must not leak into the following epoch.
    detector.submit(samples.data(),8192,50e6,2444.5e6,0,10);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    for (size_t i=0;i<samples.size();i+=8192) {
        detector.submit(samples.data()+i,8192,50e6,2444.5e6,1+i/50e6,11);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (!detector.snapshot().decoded && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    health=detector.snapshot();
    if (!health.decoded || !health.discontinuities || health.partial_discarded<8192 || health.preprocessing_ms<=0)
        throw std::runtime_error("DC decode or discontinuity metrics failed");
    bool fresh_metadata=false;
    for (const auto& o:health.observations)
        if (o.confirmed && o.receiver_epoch==11 && o.acquired_at!=std::chrono::steady_clock::time_point{}) fresh_metadata=true;
    if (!fresh_metadata) throw std::runtime_error("Missing acquisition metadata for scan/hold");
    const auto analyzed=health.samples;
    std::vector<std::complex<float>> large(2007040);
    detector.submit(large.data(),large.size(),100e6,2444.5e6,2,12);
    deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while (detector.snapshot().samples<analyzed+2000000 && std::chrono::steady_clock::now()<deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    health=detector.snapshot();
    if (health.samples<analyzed+2000000 || health.analysis_errors)
        throw std::runtime_error("100 MS/s batch exceeded decoder sample limit");
    detector.stop();
    std::cout << "Subprocess decode, bounded queue, cancellation and restart passed\n";
}
