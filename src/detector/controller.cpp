#include "controller.hpp"
#include "hopping.hpp"
#include <algorithm>
#include <cmath>

namespace drone {
void Controller::shutdown(UsrpWorker& radio) {
    radio.detector_active=false;
    detector_.stop();
    active_=false;
}
void Controller::tick(AppState& s, UsrpWorker& radio, bool recording) {
    using Clock=std::chrono::steady_clock;
    auto now=Clock::now();
    s.dd_recording_busy=recording;
    if (s.dd_connect_requested) {
        s.dd_connect_requested=false;
        if (!recording && !radio.is_connected()) {
            const auto& profile=profiles().at(s.dd_profile);
            s.dd_target_hz=s.dd_use_custom_freq ? s.dd_custom_freq_mhz*1e6
                : profile.channels.at(s.dd_profile_channel);
            radio.set_sample_rate(profile.rate);
            radio.set_gain(s.dd_gain_db);
            radio.set_freq(s.dd_target_hz);
            if (!radio.connect("addr=192.168.10.2")) s.dd_status_msg=radio.get_last_error();
            else s.dd_status_msg="Connected. Select a profile and start detection.";
        }
    }
    if (s.dd_start_requested) {
        s.dd_start_requested=false;
        if (recording || !radio.is_connected()) {
            s.dd_status_msg=recording ? "Radio is recording; stop recording before detector tuning." : "Connect the USRP first.";
        } else if (!active_) {
            const auto& profile=profiles().at(s.dd_profile);
            sequence_=profile.channels;
            if (s.dd_mode==3 && profile.elrs) {
                sequence_.clear();
                for (unsigned channel:elrs_sequence(s.dd_seed,profile.channels.size())) sequence_.push_back(profile.channels[channel]);
            }
            position_=s.dd_mode==0 ? static_cast<size_t>(s.dd_profile_channel) : 0;
            if (s.dd_use_custom_freq) { sequence_={s.dd_custom_freq_mhz*1e6}; position_=0; }
            radio.set_sample_rate(profile.rate);
            radio.set_gain(s.dd_gain_db);
            s.dd_target_hz=sequence_.at(position_);
            radio.set_freq(s.dd_target_hz);
            s.dd_actual_rate=radio.get_sample_rate();
            // DJI needs sufficient captured bandwidth even if UHD coerces the requested rate.
            if (profile.rate>=15e6 && s.dd_actual_rate<15.26e6) {
                s.dd_status_msg="USRP sample rate is too low for this profile.";
                s.dd_stop_requested=true;
            } else {
                detector_.start(); radio.detector_active=true; active_=true;
                last_decoded_=0; tuned_=false;
                tune_deadline_=now+std::chrono::seconds(3);
                s.dd_status_msg="Starting receiver...";
            }
        }
    }
    if (active_ && (!radio.is_connected() || recording || !detector_.running())) {
        s.dd_stop_requested=true;
        s.dd_status_msg=recording ? "Detection stopped: radio is recording." :
            (!radio.is_connected() ? "Detection stopped: radio disconnected." : detector_.snapshot().status);
    }
    if (s.dd_stop_requested) {
        s.dd_stop_requested=false;
        shutdown(radio);
        s.dd_snapshot=detector_.snapshot();
        if (s.dd_status_msg=="Stop requested") s.dd_status_msg="Stopped; detector frequency and RX settings retained.";
    }
    if (active_) {
        s.dd_snapshot=detector_.snapshot();
        s.dd_status_msg=s.dd_snapshot.status;
        if (std::abs(radio.get_sample_rate()-s.dd_actual_rate)>1) {
            s.dd_status_msg="Detection stopped: receiver sample rate changed.";
            s.dd_stop_requested=true;
        }
        if (!tuned_ && std::abs(radio.get_freq()-s.dd_target_hz)<5000) {
            tuned_=true;
            deadline_=now+std::chrono::milliseconds(std::max(100,s.dd_dwell_ms));
        }
        if (!tuned_ && now>tune_deadline_) {
            s.dd_status_msg="Tune failed: requested frequency was not applied.";
            s.dd_stop_requested=true;
        }
        if (s.dd_snapshot.decoded>last_decoded_) {
            last_decoded_=s.dd_snapshot.decoded;
            // Queued results from a previous channel must not hold the new channel.
            bool current=false;
            for (const auto& o:s.dd_snapshot.observations)
                if (o.confirmed && std::abs(o.frequency-s.dd_target_hz)<5000) current=true;
            if (s.dd_mode==2 && current) deadline_=now+std::chrono::milliseconds(s.dd_hold_ms);
        }
        if (tuned_ && s.dd_snapshot.ready && s.dd_mode!=0 && !s.dd_use_custom_freq && now>=deadline_) {
            position_=(position_+1)%sequence_.size();
            s.dd_target_hz=sequence_[position_]; radio.set_freq(s.dd_target_hz);
            tuned_=false; tune_deadline_=now+std::chrono::seconds(3);
        }
        s.dd_scan_position=position_;
    }
    s.dd_detection_running=active_;
}
}
