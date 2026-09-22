#include "droneid_panel.hpp"
#include "splitter.hpp"
#include "detector/hopping.hpp"
#include "imgui.h"
#include "implot.h"
#include <algorithm>
#include <cstdio>

static void Section(const char* text) {
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(.49f,.83f,.99f,1), "%s", text);
    ImGui::Separator();
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
    ImGui::Text("Requested sample rate: %.2f MS/s",profile.rate/1e6);
    ImGui::EndDisabled();
    if (connected) {
        ImGui::Text("Actual: %.4f MHz / %.3f MS/s",radio->get_freq()/1e6,radio->get_sample_rate()/1e6);
    }
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
    ImGui::Text("DJI candidates: %llu  |  Rejected: %llu  |  Valid packets: %llu",
                (unsigned long long)stats.candidates,(unsigned long long)stats.rejected,(unsigned long long)stats.decoded);
    if (stats.processing_ms>20) ImGui::TextColored(ImVec4(1,.75f,.35f,1),"Decoder is slower than the 20 ms input window; monitor dropped samples.");
    Section("DRONES WITH VALIDATED DJI ID");
    bool any=false;
    for (const auto& o:stats.observations) if (o.confirmed) any=true;
    if (!any) ImGui::TextWrapped("No checksum-validated DJI IDs received. Waveform candidates below do not establish a drone identity.");
    if (any && ImGui::BeginTable("##dji",7,ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_Resizable|ImGuiTableFlags_ScrollX)) {
        for (const char* name:{"Serial / broadcast ID","Model","Protocol / link","MHz","dBFS","Packets","Coordinates"}) ImGui::TableSetupColumn(name);
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
            if (o.latitude==0 && o.longitude==0) ImGui::TextUnformatted("Unavailable");
            else ImGui::Text("%.6f, %.6f",o.latitude,o.longitude);
        }
        ImGui::EndTable();
    }
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
