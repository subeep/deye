#include "detector.hpp"
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <sstream>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
namespace drone {
Detector::~Detector() { stop(); }
void Detector::start(bool dc_block) {
    stop();
    { std::lock_guard<std::mutex> lock(mutex_); snapshot_ = {}; queue_.clear(); }
    dc_block_=dc_block;
    dropped_=0; gap_=0; running_=true;
    thread_=std::thread(&Detector::run, this);
}
void Detector::stop() {
    running_=false; wake_.notify_all();
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& chunk:queue_) snapshot_.stop_discarded+=chunk.data.size();
    queue_.clear(); snapshot_.ready=false;
}
void Detector::status(const std::string& text, bool ready) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshot_.status=text; snapshot_.ready=ready;
}
Snapshot Detector::snapshot() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto out=snapshot_; out.dropped=dropped_.load();
    out.queue_depth=queue_.size();
    if (!queue_.empty()) out.oldest_queue_ms=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-queue_.front().enqueued).count();
    return out;
}
void Detector::submit(const std::complex<float>* data, size_t count, double rate,
                      double frequency, double timestamp, uint64_t epoch) {
    if (!running_) return;
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock || queue_.size() >= 256) {
        dropped_+=count; ++gap_; return;
    }
    // Each RX gap/reconfiguration and local queue loss starts a new continuity epoch.
    queue_.push_back({std::vector<std::complex<float>>(data, data+count),rate,frequency,timestamp,
                      (epoch<<32) ^ gap_.load(), epoch, std::chrono::steady_clock::now()});
    wake_.notify_one();
}
static bool transfer(int fd, void* data, size_t bytes, bool write, const std::atomic<bool>& running) {
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);
    auto* p=static_cast<char*>(data);
    while (bytes && running) {
        pollfd item{fd, static_cast<short>(write ? POLLOUT : POLLIN), 0};
        int rc=poll(&item,1,50);
        if (std::chrono::steady_clock::now()>deadline) return false;
        if (rc<0) { if (errno==EINTR) continue; return false; }
        if (!rc) continue;
        if (!(item.revents & item.events)) return false;
        ssize_t n=write ? ::write(fd,p,bytes) : ::read(fd,p,bytes);
        if (n<0 && (errno==EINTR || errno==EAGAIN)) continue;
        if (n<=0) return false;
        p+=n; bytes-=n;
    }
    return bytes==0;
}
static bool line(int fd, std::string& text, const std::atomic<bool>& running) {
    text.clear();
    char ch;
    while (text.size()<131072 && transfer(fd,&ch,1,false,running)) {
        if (ch=='\n') return true;
        text+=ch;
    }
    return false;
}
void Detector::run() {
    int sockets[2]={-1,-1}; pid_t child=-1;
    try {
        status("Starting IQ decoder...");
        if (socketpair(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0,sockets)) throw std::runtime_error("Cannot create decoder socket");
        posix_spawn_file_actions_t actions;
        posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_adddup2(&actions,sockets[1],STDIN_FILENO);
        posix_spawn_file_actions_adddup2(&actions,sockets[1],STDOUT_FILENO);
        posix_spawn_file_actions_addclose(&actions,sockets[0]);
        posix_spawn_file_actions_addclose(&actions,sockets[1]);
        const char* args[]={DETECTOR_PYTHON,"-u",DETECTOR_SCRIPT,"--turbo",DETECTOR_TURBO,dc_block_ ? "--dc-block" : nullptr,nullptr};
        int rc=posix_spawn(&child,DETECTOR_PYTHON,&actions,nullptr,const_cast<char* const*>(args),environ);
        posix_spawn_file_actions_destroy(&actions);
        close(sockets[1]); sockets[1]=-1;
        if (rc) throw std::runtime_error("Cannot launch Python decoder: "+std::string(strerror(rc)));
        // Nonblocking local IPC with SIGPIPE blocked only in this worker thread.
        sigset_t blocked;
        sigemptyset(&blocked); sigaddset(&blocked,SIGPIPE);
        pthread_sigmask(SIG_BLOCK,&blocked,nullptr);
        int flags=fcntl(sockets[0],F_GETFL);
        if (flags<0 || fcntl(sockets[0],F_SETFL,flags|O_NONBLOCK)<0)
            throw std::runtime_error("Cannot configure decoder IPC");
        std::string response;
        if (!line(sockets[0],response,running_)) throw std::runtime_error("Decoder startup failed; check Python requirements and terminal");
        boost::property_tree::ptree hello;
        std::istringstream handshake(response); boost::property_tree::read_json(handshake,hello);
        if (!hello.get<bool>("ready",false)) throw std::runtime_error("Invalid decoder handshake");
        status("Listening: protocol analysis and DJI CRC-validated decoding",true);
        Chunk batch;
        uint64_t continuity=0;
        bool have_previous=false;
        double expected_time=0, previous_rate=0, previous_frequency=0;
        uint64_t previous_epoch=0;
        while (running_) {
            Chunk part;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                wake_.wait_for(lock,std::chrono::milliseconds(100),[&]{return !queue_.empty() || !running_;});
                if (!running_) break;
                if (queue_.empty()) continue;
                part=std::move(queue_.front()); queue_.pop_front();
            }
            const bool discontinuity=have_previous && (previous_rate!=part.rate ||
                previous_frequency!=part.frequency || previous_epoch!=part.epoch ||
                std::abs(part.timestamp-expected_time)>std::max(2/part.rate,1e-9));
            if (discontinuity) {
                std::lock_guard<std::mutex> lock(mutex_);
                ++snapshot_.discontinuities;
                snapshot_.partial_discarded+=batch.data.size();
                batch.data.clear();
            }
            have_previous=true; previous_rate=part.rate; previous_frequency=part.frequency;
            previous_epoch=part.epoch; expected_time=part.timestamp+part.data.size()/part.rate;
            // Also propagate timestamp-only gaps to the Python overlap/filter state.
            if (discontinuity) ++continuity;
            part.epoch=continuity;
            if (batch.data.empty()) batch=std::move(part);
            else batch.data.insert(batch.data.end(),part.data.begin(),part.data.end());
            const size_t target=std::min<size_t>(2000000, static_cast<size_t>(batch.rate*.02));
            if (batch.data.size()<target) continue;
            const auto processing_start=std::chrono::steady_clock::now();
            uint32_t count=static_cast<uint32_t>(std::min<size_t>(batch.data.size(),2000000));
            char header[40]; memcpy(header,"DDIQ",4); memcpy(header+4,&count,4);
            memcpy(header+8,&batch.rate,8); memcpy(header+16,&batch.frequency,8);
            memcpy(header+24,&batch.timestamp,8); memcpy(header+32,&batch.epoch,8);
            if (!transfer(sockets[0],header,sizeof(header),true,running_)) throw std::runtime_error("Decoder header send failed");
            if (!transfer(sockets[0],batch.data.data(),count*sizeof(std::complex<float>),true,running_))
                throw std::runtime_error("Decoder IQ send failed: "+std::string(strerror(errno)));
            if (!line(sockets[0],response,running_)) throw std::runtime_error("Decoder response read failed");
            boost::property_tree::ptree result;
            std::istringstream input(response); boost::property_tree::read_json(input,result);
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot_.samples+=count;
            snapshot_.queue_age_ms=std::chrono::duration<double,std::milli>(processing_start-batch.enqueued).count();
            snapshot_.end_to_end_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-batch.enqueued).count();
            snapshot_.preprocessing_ms=result.get<double>("preprocessing_ms",0);
            snapshot_.processing_ms=result.get<double>("processing_ms",0);
            snapshot_.candidates+=result.get<uint64_t>("candidates",0);
            snapshot_.rejected+=result.get<uint64_t>("rejected",0);
            if (auto error=result.get_optional<std::string>("error")) { ++snapshot_.analysis_errors; snapshot_.status="Analysis error: "+*error; }
            for (auto& item:result.get_child("events")) {
                auto& v=item.second;
                Observation o;
                o.receiver_epoch=batch.receiver_epoch; o.acquired_at=batch.enqueued;
                o.protocol=v.get<std::string>("protocol"); o.link=v.get<std::string>("link");
                o.evidence=v.get<std::string>("evidence"); o.serial=v.get<std::string>("serial","");
                o.model=v.get<std::string>("model",""); o.confirmed=v.get<bool>("confirmed",false);
                o.frequency=v.get<double>("frequency"); o.power=v.get<double>("power");
                o.timestamp=v.get<double>("timestamp"); o.latitude=v.get<double>("latitude",0);
                o.longitude=v.get<double>("longitude",0); o.altitude=v.get<double>("altitude",0);
                if (auto telemetry=v.get_child_optional("telemetry")) {
                    o.packet_json=telemetry->get<std::string>("packet_json", "");
                    o.position_status=telemetry->get<std::string>("aircraft_position_status", "Unavailable");
                    if (auto fields=telemetry->get_child_optional("fields"))
                        for (const auto& field:*fields) {
                            const auto& f=field.second;
                            o.telemetry.push_back({f.get<std::string>("group"), f.get<std::string>("name"),
                                                   f.get<std::string>("value"), f.get<std::string>("status")});
                        }
                }
                if (o.confirmed) ++snapshot_.decoded;
                auto found=std::find_if(snapshot_.observations.begin(),snapshot_.observations.end(),[&](const Observation& old){
                    if (o.confirmed) return old.confirmed && old.serial==o.serial;
                    return !old.confirmed && old.protocol==o.protocol && std::abs(old.frequency-o.frequency)<1000;
                });
                if (found!=snapshot_.observations.end()) { o.count=found->count+1; *found=std::move(o); }
                else {
                    if (snapshot_.observations.size()>=128) {
                        auto candidate=std::find_if(snapshot_.observations.begin(),snapshot_.observations.end(),
                            [](const Observation& entry){return !entry.confirmed;});
                        if (candidate!=snapshot_.observations.end()) snapshot_.observations.erase(candidate);
                        else if (o.confirmed) snapshot_.observations.erase(snapshot_.observations.begin());
                        else continue;
                    }
                    snapshot_.observations.push_back(std::move(o));
                }
            }
            batch.data.erase(batch.data.begin(),batch.data.begin()+count);
            batch.timestamp+=count/batch.rate;
        }
        { std::lock_guard<std::mutex> lock(mutex_); snapshot_.stop_discarded+=batch.data.size(); }
    } catch (const std::exception& error) {
        if (running_) status(error.what());
    }
    for (int fd:sockets) if (fd>=0) close(fd);
    if (child>0) {
        kill(child,SIGTERM);
        int rc=0;
        for (int i=0;i<20;++i) {
            rc=waitpid(child,nullptr,WNOHANG);
            if (rc!=0) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!rc) { kill(child,SIGKILL); while (waitpid(child,nullptr,0)<0 && errno==EINTR) {} }
    }
    running_=false;
}
}
