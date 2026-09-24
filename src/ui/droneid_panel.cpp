#include "droneid_panel.hpp"
#include "splitter.hpp"
#include "detector/hopping.hpp"
#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <unistd.h>

static void Section(const char* text) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(.49f,.83f,.99f,1), "%s", text);
    ImGui::Separator();
}
static void TelemetryDetails(const drone::Observation& o) {
    static std::string export_message;
    ImGui::PushID(o.serial.c_str());
    const std::string label="Telemetry: "+o.serial+" ("+o.model+")";
    if (ImGui::TreeNodeEx("##telemetry",ImGuiTreeNodeFlags_None,"%s",label.c_str())) {
        const double age=std::chrono::duration<double>(std::chrono::steady_clock::now()-o.acquired_at).count();
        ImGui::Text("Latest packet received %.1f s ago%s",age,age>10 ? " - STALE (over 10 s)" : "");
        ImGui::TextWrapped("All fields below belong to this packet. Age measures receiver acquisition time, not GPS-fix freshness. CRC checks packet integrity; telemetry validity and state-bit meanings may be unverified.");
        if (o.telemetry.empty()) ImGui::TextUnformatted("Telemetry unavailable for this observation.");
        else if (ImGui::BeginTable("##fields",3,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Field"); ImGui::TableSetupColumn("Reported value");
            ImGui::TableSetupColumn("Validity / interpretation"); ImGui::TableHeadersRow();
            std::string group;
            for (const auto& f:o.telemetry) {
                if (group!=f.group) {
                    group=f.group; ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(ImVec4(.49f,.83f,.99f,1),"%s",group.c_str());
                }
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextWrapped("%s",f.name.c_str());
                ImGui::TableSetColumnIndex(1); ImGui::TextWrapped("%s",f.value.c_str());
                ImGui::TableSetColumnIndex(2); ImGui::TextWrapped("%s",f.status.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::BeginDisabled(o.packet_json.empty());
        if (ImGui::Button("Copy packet JSON")) ImGui::SetClipboardText(o.packet_json.c_str());
        ImGui::SameLine();
        if (ImGui::Button("Save packet JSON")) {
            std::error_code error;
            std::filesystem::create_directories("reports/telemetry",error);
            std::string message;
            if (error) message="Cannot create export directory: "+error.message();
            else {
                // Unique, exclusively created files never overwrite previous evidence.
                std::string path="reports/telemetry/dji-packet-XXXXXX.json";
                int fd=mkstemps(path.data(),5);
                FILE* file=fd<0 ? nullptr : fdopen(fd,"w");
                if (!file) { if (fd>=0) { close(fd); std::filesystem::remove(path,error); } message="Cannot open packet export file."; }
                else {
                    const bool written=fwrite(o.packet_json.data(),1,o.packet_json.size(),file)==o.packet_json.size();
                    const bool closed=fclose(file)==0;
                    if (written && closed) message="Saved: "+std::filesystem::absolute(path).string();
                    else { std::filesystem::remove(path,error); message="Packet export failed."; }
                }
            }
            export_message=message;
            ImGui::OpenPopup("Export result");
        }
        ImGui::EndDisabled();
        if (ImGui::BeginPopup("Export result")) {
            ImGui::TextWrapped("%s",export_message.c_str()); ImGui::EndPopup();
        }
        if (ImGui::TreeNode("Decoded packet JSON / raw payload")) {
            ImGui::TextWrapped("%s",o.packet_json.c_str()); ImGui::TreePop();
        }
        ImGui::TreePop();
    }
    ImGui::PopID();
}
static void Config(AppState& s, const UsrpWorker* radio) {
    bool connected=radio && radio->is_connected();
    Section("RECEIVE-ONLY DRONE / LINK DETECTOR");
    ImGui::TextUnformatted(connected ? "USRP connected" : "USRP disconnected");
    if (!connected && ImGui::Button("Connect USRP X310",ImVec2(-1,28))) s.dd_connect_requested=true;
    if (s.dd_recording_busy) ImGui::TextWrapped("The radio is recording. Stop recording before starting the detector.");
    ImGui::BeginDisabled(s.dd_detection_running || s.dd_recording_busy);
    const auto& profiles=drone::profiles();
    Section("TARGET LINK FAMILY");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##profile",profiles[s.dd_profile].name)) {
        for (size_t i=0;i<profiles.size();++i) {
            if (ImGui::Selectable(profiles[i].name,s.dd_profile==static_cast<int>(i))) {
                s.dd_profile=static_cast<int>(i); s.dd_profile_channel=0;
                if (!profiles[i].elrs && s.dd_mode==3) s.dd_mode=0;
            }
        }
        ImGui::EndCombo();
    }
    const auto& profile=profiles[s.dd_profile];
    ImGui::TextWrapped("%s",profile.description);
    Section("ACQUISITION MODE");
    const char* modes[]={"Fixed channel","Scan channels","Scan / hold on valid DJI ID","ELRS reconstructed order (slow scan)"};
    ImGui::SetNextItemWidth(-1);
    ImGui::Combo("##mode",&s.dd_mode,modes,profile.elrs ? 4 : 3);
    if (s.dd_mode==0) {
        char label[64]; snprintf(label,sizeof(label),"%.4f MHz",profile.channels[s.dd_profile_channel]/1e6);
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##channel",label)) {
            for (size_t i=0;i<profile.channels.size();++i) {
                snprintf(label,sizeof(label),"%zu  |  %.4f MHz",i,profile.channels[i]/1e6);
                if (ImGui::Selectable(label,s.dd_profile_channel==static_cast<int>(i))) s.dd_profile_channel=i;
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##dwell",&s.dd_dwell_ms,100,3000,"Dwell: %d ms");
        if (s.dd_mode==2) {
            ImGui::SetNextItemWidth(-1);
            ImGui::SliderInt("##hold",&s.dd_hold_ms,1000,10000,"Hold: %d ms");
        }
    }
    if (profile.elrs) {
        ImGui::TextUnformatted("ELRS firmware FHSS seed (hex)");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputScalar("##seed",ImGuiDataType_U32,&s.dd_seed,nullptr,nullptr,"%08X",ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::TextWrapped("Reconstructs channel order from the firmware seed. Does not infer the seed or synchronize to a transmitter.");
    }
    ImGui::Checkbox("Custom fixed frequency",&s.dd_use_custom_freq);
    if (s.dd_use_custom_freq) {
        ImGui::SetNextItemWidth(-1);
        ImGui::InputFloat("##frequency",&s.dd_custom_freq_mhz,.1f,1,"%.4f MHz");
        s.dd_custom_freq_mhz=std::clamp(s.dd_custom_freq_mhz,10.f,6000.f);
    }
    Section("RECEIVER");
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##gain",&s.dd_gain_db,0,31.5f,"Gain: %.1f dB");
    if (ImGui::Checkbox("Custom sample rate", &s.dd_use_custom_rate) && s.dd_use_custom_rate)
        s.dd_sample_rate_msps=static_cast<float>(profile.rate/1e6);
    if (s.dd_use_custom_rate) {
        ImGui::TextUnformatted("Sample rate (MS/s)");
        ImGui::SetNextItemWidth(-1);
        ImGui::InputFloat("##detector_sample_rate", &s.dd_sample_rate_msps, 1.0f, 5.0f, "%.3f");
        if (!std::isfinite(s.dd_sample_rate_msps)) s.dd_sample_rate_msps=static_cast<float>(profile.rate/1e6);
        s.dd_sample_rate_msps=std::clamp(s.dd_sample_rate_msps, .1f, 100.f);
        ImGui::TextWrapped("Applied on Connect or Start. Hardware may adjust the rate.");
        if (profile.rate>=15e6 && s.dd_sample_rate_msps<15.26f)
            ImGui::TextWrapped("This profile requires at least 15.26 MS/s to start detection.");
    } else {
        ImGui::Text("Sample rate: %.2f MS/s (profile)",profile.rate/1e6);
    }
    if (s.dd_profile<=1 && ImGui::Button("Use 20 MS/s (lower data rate)",ImVec2(-1,0))) {
        s.dd_use_custom_rate=true; s.dd_sample_rate_msps=20.f;
    }
    ImGui::Checkbox("DC correction (experimental)", &s.dd_dc_block);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Streaming 1 kHz DC blocker; initializes from 4096 samples after gaps. Applied on Start.");
    ImGui::EndDisabled();
    if (connected) {
        ImGui::Text("Actual: %.4f MHz / %.3f MS/s",radio->get_freq()/1e6,radio->get_sample_rate()/1e6);
    }
    if (s.dd_detection_running && s.dd_mode!=0 && !s.dd_use_custom_freq)
        ImGui::Text("%s: %.1f s", s.dd_hold_active ? "Holding validated ID" : "Channel dwell remaining", s.dd_dwell_remaining_s);
    ImGui::Spacing();
    if (s.dd_detection_running) {
        if (ImGui::Button("STOP DETECTION",ImVec2(-1,36))) {
            s.dd_stop_requested=true; s.dd_status_msg="Stop requested";
        }
    } else {
        ImGui::BeginDisabled(!connected || s.dd_recording_busy);
        if (ImGui::Button("START DETECTION",ImVec2(-1,36))) s.dd_start_requested=true;
        ImGui::EndDisabled();
    }
    ImGui::TextWrapped("%s",s.dd_status_msg.c_str());
    ImGui::TextWrapped("Scanning observes one RF window at a time and can miss short bursts. No RF transmission is performed.");
}
static void Results(AppState& s) {
    const auto& stats=s.dd_snapshot;
    Section("LIVE ANALYSIS");
    ImGui::Text("Analyzed %.2f M samples  |  Queue drops %.2f M  |  Last batch %.1f ms",
                stats.samples/1e6,stats.dropped/1e6,stats.processing_ms);
    ImGui::Text("Queue: %zu blocks | Oldest: %.1f ms | Last batch wait: %.1f ms",
        stats.queue_depth,stats.oldest_queue_ms,stats.queue_age_ms);
    ImGui::Text("Last batch age at result: %.1f ms | Preprocessing: %.1f ms",
        stats.end_to_end_ms,stats.preprocessing_ms);
    ImGui::Text("RX events: %llu overflow / %llu timeout / %llu other error",
        (unsigned long long)s.dd_rx_overflows,(unsigned long long)s.dd_rx_timeouts,(unsigned long long)s.dd_rx_errors);
    ImGui::Text("Continuity breaks: %llu | Partial batches discarded: %.3f M samples | Analysis errors: %llu",
        (unsigned long long)stats.discontinuities,stats.partial_discarded/1e6,(unsigned long long)stats.analysis_errors);
    ImGui::Text("Tuning settle skips: %.3f M samples (%.3f s) | Stop discards: %.3f M",
        s.dd_settle_skipped/1e6,s.dd_settle_seconds,stats.stop_discarded/1e6);
    ImGui::Text("Current scan target: away %.2f / %.2f s (UI-sampled tune coverage)",
        s.dd_channel_away_s,s.dd_scan_elapsed_s);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cumulative time tuned away from the current target, sampled each UI tick. Excludes settling and queue loss; not packet coverage. RX events do not measure exact missing samples.");
    ImGui::Text("DJI candidates: %llu  |  Rejected: %llu  |  Valid packets: %llu",
                (unsigned long long)stats.candidates,(unsigned long long)stats.rejected,(unsigned long long)stats.decoded);
    if (stats.processing_ms>20) ImGui::TextColored(ImVec4(1,.75f,.35f,1),"Decoder is slower than the 20 ms input window; monitor dropped samples.");
    Section("DRONES WITH VALIDATED DJI ID");
    bool any=false;
    for (const auto& o:stats.observations) if (o.confirmed) any=true;
    if (!any) ImGui::TextWrapped("No checksum-validated DJI IDs received. Waveform candidates below do not establish a drone identity.");
    if (any) ImGui::TextWrapped("Detected frequency and last seen describe received evidence; the receiver may now be scanning another channel.");
    if (any && ImGui::BeginTable("##dji",8,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollX)) {
        for (const char* name:{"Serial / broadcast ID","Model","Protocol / link","Detected MHz","dBFS","Packets","Coordinates","Last seen"}) ImGui::TableSetupColumn(name);
        ImGui::TableHeadersRow();
        for (const auto& o:stats.observations) {
            if (!o.confirmed) continue;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(o.serial.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(o.model.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted("DJI DroneID / ID broadcast");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s\nRX time: %.3f s",o.evidence.c_str(),o.timestamp);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%.4f",o.frequency/1e6);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%.1f",o.power);
            ImGui::TableSetColumnIndex(5); ImGui::Text("%llu",(unsigned long long)o.count);
            ImGui::TableSetColumnIndex(6);
            if (o.position_status!="Reported; validity unverified") ImGui::TextWrapped("%s",o.position_status.c_str());
            else ImGui::Text("%.6f, %.6f (reported)",o.latitude,o.longitude);
            ImGui::TableSetColumnIndex(7);
            ImGui::Text("%.1f s ago",std::chrono::duration<double>(std::chrono::steady_clock::now()-o.acquired_at).count());
        }
        ImGui::EndTable();
    }
    for (const auto& o:stats.observations) if (o.confirmed) TelemetryDetails(o);
    Section("LINK OBSERVATIONS / PROTOCOL CANDIDATES");
    ImGui::TextWrapped("These rows are signal observations, not separate drones. A control link and video link cannot be assigned to the same aircraft without identifying packets.");
    ImGui::TextDisabled("Drag the divider below the list to resize; double-click to reset.");
    if (ImGui::BeginTable("##links",5,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollY,ImVec2(0,s.dd_observations_height))) {
        for (const char* name:{"Protocol candidate","Link role","MHz / dBFS","Evidence","Hits"}) ImGui::TableSetupColumn(name);
        ImGui::TableSetupScrollFreeze(0,1);
        ImGui::TableHeadersRow();
        for (auto it=stats.observations.rbegin();it!=stats.observations.rend();++it) {
            const auto& o=*it; if (o.confirmed) continue;
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextWrapped("%s",o.protocol.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextWrapped("%s",o.link.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.4f / %.1f",o.frequency/1e6,o.power);
            ImGui::TableSetColumnIndex(3); ImGui::TextWrapped("%s",o.evidence.c_str());
            ImGui::TableSetColumnIndex(4); ImGui::Text("%llu",(unsigned long long)o.count);
        }
        ImGui::EndTable();
    }
    DrawHorizontalSplitter("##detector_list_height",s.dd_observations_height,420.0f,140.0f,2000.0f);
    Section("HOPPING / CHANNEL PLAN");
    const auto& p=drone::profiles()[s.dd_profile];
    std::vector<double> frequencies;
    if (p.elrs) for (auto channel:drone::elrs_sequence(s.dd_seed,p.channels.size())) frequencies.push_back(p.channels[channel]/1e6);
    else for (auto frequency:p.channels) frequencies.push_back(frequency/1e6);
    ImGui::TextWrapped("%s",p.elrs ? "Reconstructed ExpressLRS channel order. The live scanner follows this order only in reconstructed-order mode, at the selected slow dwell. It is not synchronized FHSS tracking." : "Acquisition scan list. DJI adaptive hopping is not reconstructed from this list.");
    if (ImPlot::BeginPlot("##hops",ImVec2(-1,180))) {
        ImPlot::SetupAxes("Sequence index","MHz",ImPlotAxisFlags_AutoFit,ImPlotAxisFlags_AutoFit);
        ImPlot::PlotLine("Channel order",frequencies.data(),frequencies.size());
        ImPlot::EndPlot();
    }
    if (s.dd_detection_running) ImGui::Text("Receive scan position: %zu  |  Target: %.4f MHz",s.dd_scan_position,s.dd_target_hz/1e6);
    if (ImGui::TreeNode("Support and validation limits")) {
        ImGui::BulletText("DJI: IQ -> OFDM -> turbo FEC -> CRC24A + CRC16 -> ID.");
        ImGui::BulletText("DIY: CSS, FSK, OFDM and FM video waveform candidates.");
        ImGui::BulletText("ELRS FLRC, FrSky/FlySky packet decoders and DJI O3/O4 are not validated.");
        ImGui::BulletText("Cross-link aircraft association and synchronized RF hop tracking are not implemented.");
        ImGui::TreePop();
    }
}
void DrawDroneIdPanel(AppState& s, const UsrpWorker* radio) {
    float width=std::clamp(ImGui::GetContentRegionAvail().x*.3f,280.f,370.f);
    ImGui::BeginChild("##detector_config",ImVec2(width,0),ImGuiChildFlags_Borders);
    Config(s,radio); ImGui::EndChild(); ImGui::SameLine();
    ImGui::BeginChild("##detector_results",ImVec2(0,0)); Results(s); ImGui::EndChild();
}
