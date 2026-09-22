#include "control_panel.hpp"
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

// ─── Colour helpers ───────────────────────────────────────────────────────────
static ImVec4 ColFromHex(uint32_t hex) {
    float r = ((hex >> 16) & 0xFF) / 255.0f;
    float g = ((hex >> 8)  & 0xFF) / 255.0f;
    float b = ( hex        & 0xFF) / 255.0f;
    return ImVec4(r, g, b, 1.0f);
}

static void SectionHeader(const char* label) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ColFromHex(0x7DD3FC)); // sky-300
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();
    float x = ImGui::GetCursorPosX();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(ImGui::GetWindowPos().x + x, ImGui::GetCursorScreenPos().y),
        ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowWidth() - 8, ImGui::GetCursorScreenPos().y),
        IM_COL32(100, 140, 180, 80), 1.0f);
    ImGui::Spacing();
}

void DrawControlPanel(AppState& state, const UsrpWorker* worker, const Recorder* recorder) {
    bool connected = worker && worker->is_connected();
    bool recording = recorder && recorder->is_recording();

    // ─── CONNECTION ───────────────────────────────────────────────────────────
    SectionHeader("  DEVICE");

    ImGui::PushStyleColor(ImGuiCol_Text, connected
        ? ImVec4(0.4f, 1.0f, 0.5f, 1.0f)
        : ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
    ImGui::Bullet();
    ImGui::SameLine();
    ImGui::TextUnformatted(connected ? "USRP X310 Connected" : "Disconnected");
    ImGui::PopStyleColor();

    if (!connected) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.15f, 0.55f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.20f, 0.65f, 1.00f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.10f, 0.45f, 0.75f, 1.0f));
        if (ImGui::Button("  Connect  ", ImVec2(-1, 32)))
            state.connect_requested = true;
        ImGui::PopStyleColor(3);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.80f, 0.20f, 0.20f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.45f, 0.10f, 0.10f, 1.0f));
        if (ImGui::Button("  Disconnect  ", ImVec2(-1, 32)))
            state.disconnect_requested = true;
        ImGui::PopStyleColor(3);
    }

    // ─── RF SETTINGS ─────────────────────────────────────────────────────────
    SectionHeader("  RF SETTINGS");

    // Center Frequency
    ImGui::TextUnformatted("Center Frequency (MHz)");
    ImGui::SetNextItemWidth(-1);
    bool freq_edited = ImGui::InputFloat("##freq", &state.ui_freq_mhz, 0.1f, 1.0f, "%.4f MHz");
    // Clamp to UBX-160 range
    if (state.ui_freq_mhz < 10.0f)   state.ui_freq_mhz = 10.0f;
    if (state.ui_freq_mhz > 6000.0f) state.ui_freq_mhz = 6000.0f;
    if (freq_edited && connected) {
        state.apply_freq = true;
    }
    if (connected && ImGui::IsItemDeactivatedAfterEdit()) {
        state.apply_freq = true;
    }

    ImGui::Spacing();

    // Lock frequency button
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.12f, 0.42f, 0.65f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.15f, 0.55f, 0.85f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.08f, 0.32f, 0.50f, 1.0f));
    if (ImGui::Button(" Lock Frequency ", ImVec2(-1, 28)) && connected) {
        state.apply_freq = true;
    }
    ImGui::PopStyleColor(3);

    // Show actual tuned freq if connected
    if (connected && worker) {
        double actual = worker->get_freq();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
        ImGui::Text("  Actual: %.4f MHz", actual / 1e6);
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    // Channel chooser
    ImGui::TextUnformatted("RX Channel");
    ImGui::SetNextItemWidth(-1);
    const char* ch_items[] = {"Channel A  (RX2)", "Channel B  (RX2)"};
    if (ImGui::Combo("##channel", &state.ui_channel, ch_items, 2) && connected) {
        state.apply_channel = true;
    }

    ImGui::Spacing();

    // RX Gain slider
    ImGui::TextUnformatted("RX Gain (dB)");
    ImGui::SetNextItemWidth(-1);
    ImGui::PushStyleColor(ImGuiCol_SliderGrab,     ImVec4(0.20f, 0.65f, 1.00f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.10f, 0.45f, 0.75f, 1.0f));
    if (ImGui::SliderFloat("##gain", &state.ui_gain_db, 0.0f, 31.5f, "%.1f dB") && connected) {
        state.apply_gain = true;
    }
    ImGui::PopStyleColor(2);

    ImGui::Spacing();

    // Sample Rate
    ImGui::TextUnformatted("Sample Rate");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##rate", &state.ui_rate_idx, RATE_LABELS, RATE_COUNT) && connected) {
        state.apply_rate = true;
    }

    ImGui::Spacing();

    // FFT Size
    ImGui::TextUnformatted("FFT Size");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##fftsize", &state.ui_fft_size_idx, FFT_LABELS, FFT_SIZE_COUNT)) {
        state.apply_fft_size = true;
    }

    // ─── PLOT TOGGLES ────────────────────────────────────────────────────────
    SectionHeader("  PLOT VISIBILITY");

    auto ToggleButton = [](const char* label_on, const char* label_off,
                           bool& val, ImVec4 col_on) {
        if (val) {
            ImGui::PushStyleColor(ImGuiCol_Button,       col_on);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col_on.x*1.2f, col_on.y*1.2f, col_on.z*1.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(col_on.x*0.8f, col_on.y*0.8f, col_on.z*0.8f, 1.0f));
            if (ImGui::Button(label_on, ImVec2(-1, 26))) val = false;
            ImGui::PopStyleColor(3);
        } else {
            if (ImGui::Button(label_off, ImVec2(-1, 26))) val = true;
        }
    };

    ToggleButton("  FFT  [VISIBLE]", "  FFT  [HIDDEN]",
                 state.show_fft, ImVec4(0.15f, 0.45f, 0.20f, 1.0f));
    ImGui::Spacing();
    ToggleButton("  STFT  [VISIBLE]", "  STFT  [HIDDEN]",
                 state.show_stft, ImVec4(0.15f, 0.40f, 0.45f, 1.0f));
    ImGui::Spacing();
    ToggleButton("  Spectrogram  [VISIBLE]", "  Spectrogram  [HIDDEN]",
                 state.show_spectrogram, ImVec4(0.35f, 0.20f, 0.55f, 1.0f));

    ImGui::Spacing();
    if (ImGui::Button("  Reset Plot Sizes  ", ImVec2(-1, 24))) {
        state.ctrl_width         = 280.0f;
        state.iq_height          = 220.0f;
        state.iq_split_ratio     = 0.42f;
        state.fft_height         = 220.0f;
        state.stft_height        = 220.0f;
        state.spectrogram_height = 280.0f;
    }

    // ─── RECORDING ───────────────────────────────────────────────────────────
    SectionHeader("  RECORDING");

    ImGui::TextUnformatted("Drone Name:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##dronename", state.drone_name, sizeof(state.drone_name));

    ImGui::Spacing();

    ImGui::TextUnformatted("Output Directory:");
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##recdir", state.record_dir, sizeof(state.record_dir));

    ImGui::Spacing();

    if (!recording) {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.70f, 0.10f, 0.10f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  ImVec4(0.90f, 0.15f, 0.15f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,   ImVec4(0.55f, 0.08f, 0.08f, 1.0f));
        if (ImGui::Button("  ● START RECORDING  ", ImVec2(-1, 36)) && connected) {
            state.record_requested = true;
        }
        ImGui::PopStyleColor(3);
        if (!connected) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::TextUnformatted("  (connect USRP first)");
            ImGui::PopStyleColor();
        }
    } else {
        // Pulsing red button animation
        float t = static_cast<float>(ImGui::GetTime());
        float pulse = 0.5f + 0.5f * std::sin(t * 4.0f);
        ImVec4 col(0.80f + 0.15f * pulse, 0.10f, 0.10f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x * 1.1f, col.y, col.z, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(col.x * 0.9f, col.y, col.z, 1.0f));
        if (ImGui::Button("  ■ STOP RECORDING  ", ImVec2(-1, 36))) {
            state.record_requested = true;
        }
        ImGui::PopStyleColor(3);

        // Stats
        if (recorder) {
            uint64_t bytes = recorder->bytes_written();
            double mb = bytes / (1024.0 * 1024.0);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
            ImGui::Text("  %.1f MB  |  %llu samples",
                        mb, (unsigned long long)recorder->samples_written());
            ImGui::PopStyleColor();
        }
    }

    // ─── STATUS ───────────────────────────────────────────────────────────────
    if (!state.status_msg.empty()) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 0.7f, 1.0f));
        ImGui::TextWrapped("%s", state.status_msg.c_str());
        ImGui::PopStyleColor();
    }

    // Overflow indicator
    if (worker && worker->overflow_count.load() > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.2f, 1.0f));
        ImGui::Text("  ⚠ Overflow: %llu", (unsigned long long)worker->overflow_count.load());
        ImGui::PopStyleColor();
    }
}
