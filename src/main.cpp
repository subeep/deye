#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "implot.h"

#include "app_state.hpp"
#include "usrp_worker.hpp"
#include "signal_proc.hpp"
#include "recorder.hpp"
#include "ui/control_panel.hpp"
#include "ui/plot_panel.hpp"
#include "ui/droneid_panel.hpp"
#include "ui/splitter.hpp"
#include "detector/controller.hpp"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <string>
#include <vector>
#include <memory>

// ─── Custom Dark Theme ────────────────────────────────────────────────────────
static void ApplyDroneTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 8.0f;
    s.ChildRounding     = 6.0f;
    s.FrameRounding     = 5.0f;
    s.PopupRounding     = 6.0f;
    s.ScrollbarRounding = 5.0f;
    s.GrabRounding      = 4.0f;
    s.TabRounding       = 5.0f;
    s.FramePadding      = ImVec2(8, 5);
    s.ItemSpacing       = ImVec2(8, 6);
    s.WindowPadding     = ImVec2(12, 12);
    s.IndentSpacing     = 16.0f;
    s.ScrollbarSize     = 10.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;

    ImVec4* c = s.Colors;
    // Base palette: deep navy + electric blue accents
    c[ImGuiCol_WindowBg]            = ImVec4(0.07f, 0.08f, 0.12f, 1.00f);
    c[ImGuiCol_ChildBg]             = ImVec4(0.09f, 0.10f, 0.15f, 1.00f);
    c[ImGuiCol_PopupBg]             = ImVec4(0.08f, 0.09f, 0.13f, 0.98f);
    c[ImGuiCol_Border]              = ImVec4(0.18f, 0.22f, 0.32f, 1.00f);
    c[ImGuiCol_FrameBg]             = ImVec4(0.12f, 0.14f, 0.20f, 1.00f);
    c[ImGuiCol_FrameBgHovered]      = ImVec4(0.15f, 0.18f, 0.28f, 1.00f);
    c[ImGuiCol_FrameBgActive]       = ImVec4(0.18f, 0.22f, 0.34f, 1.00f);
    c[ImGuiCol_TitleBg]             = ImVec4(0.05f, 0.06f, 0.10f, 1.00f);
    c[ImGuiCol_TitleBgActive]       = ImVec4(0.07f, 0.10f, 0.18f, 1.00f);
    c[ImGuiCol_MenuBarBg]           = ImVec4(0.06f, 0.07f, 0.11f, 1.00f);
    c[ImGuiCol_ScrollbarBg]         = ImVec4(0.06f, 0.07f, 0.10f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]       = ImVec4(0.20f, 0.28f, 0.42f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered]= ImVec4(0.28f, 0.38f, 0.55f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.20f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_CheckMark]           = ImVec4(0.20f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_SliderGrab]          = ImVec4(0.20f, 0.55f, 0.90f, 1.00f);
    c[ImGuiCol_SliderGrabActive]    = ImVec4(0.20f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_Button]              = ImVec4(0.13f, 0.18f, 0.28f, 1.00f);
    c[ImGuiCol_ButtonHovered]       = ImVec4(0.18f, 0.28f, 0.45f, 1.00f);
    c[ImGuiCol_ButtonActive]        = ImVec4(0.12f, 0.40f, 0.70f, 1.00f);
    c[ImGuiCol_Header]              = ImVec4(0.13f, 0.20f, 0.32f, 1.00f);
    c[ImGuiCol_HeaderHovered]       = ImVec4(0.18f, 0.28f, 0.45f, 1.00f);
    c[ImGuiCol_HeaderActive]        = ImVec4(0.20f, 0.35f, 0.55f, 1.00f);
    c[ImGuiCol_Separator]           = ImVec4(0.18f, 0.22f, 0.32f, 1.00f);
    c[ImGuiCol_Tab]                 = ImVec4(0.09f, 0.12f, 0.18f, 1.00f);
    c[ImGuiCol_TabHovered]          = ImVec4(0.20f, 0.35f, 0.55f, 1.00f);
    c[ImGuiCol_TabActive]           = ImVec4(0.15f, 0.28f, 0.48f, 1.00f);
    c[ImGuiCol_Text]                = ImVec4(0.88f, 0.90f, 0.95f, 1.00f);
    c[ImGuiCol_TextDisabled]        = ImVec4(0.40f, 0.45f, 0.55f, 1.00f);
    c[ImGuiCol_PlotLines]           = ImVec4(0.20f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_PlotLinesHovered]    = ImVec4(0.10f, 0.90f, 0.60f, 1.00f);
    c[ImGuiCol_PlotHistogram]       = ImVec4(0.20f, 0.65f, 1.00f, 1.00f);
    c[ImGuiCol_TableHeaderBg]       = ImVec4(0.09f, 0.12f, 0.18f, 1.00f);
    c[ImGuiCol_TableBorderStrong]   = ImVec4(0.18f, 0.22f, 0.32f, 1.00f);
    c[ImGuiCol_TableBorderLight]    = ImVec4(0.12f, 0.15f, 0.22f, 1.00f);

    // ImPlot style
    ImPlot::GetStyle().PlotPadding    = ImVec2(6, 6);
    ImPlot::GetStyle().LabelPadding   = ImVec2(4, 4);
    ImPlot::GetStyle().LegendPadding  = ImVec2(6, 4);
    ImPlot::GetStyle().PlotBorderSize = 1.0f;
}

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main() {
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    const char* glsl_version = "#version 330";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // MSAA

    GLFWwindow* window = glfwCreateWindow(1600, 960,
        "Drone Recorder  —  USRP X310 IQ Visualizer", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // VSync

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Don't save layout to file

    // Load Inter font (or fallback)
    ImFontConfig font_cfg;
    font_cfg.OversampleH = 3;
    font_cfg.OversampleV = 3;
    io.Fonts->AddFontDefault(); // fallback, will use built-in
    io.FontGlobalScale = 1.0f;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    ApplyDroneTheme();

    // ─── Application Objects ──────────────────────────────────────────────────
    AppState      state;
    UsrpWorker    worker;
    SignalProcessor proc;
    Recorder      recorder;
    drone::Controller detector;
    worker.set_detector_sink([&](const std::complex<float>* data, size_t count,
                                double rate, double frequency, double timestamp, uint64_t epoch) {
        detector.submit(data,count,rate,frequency,timestamp,epoch);
    });

    // Staging buffer (samples drained per frame from ring buffer)
    std::vector<std::complex<float>> frame_samples;
    frame_samples.reserve(CHUNK_SAMPLES * 8);

    // ─── Main Loop ────────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // ── Handle connection requests ─────────────────────────────────────
        if (state.connect_requested) {
            state.connect_requested = false;
            state.status_msg = "Connecting to USRP X310…";
            // Set initial parameters before connect
            worker.set_freq(state.ui_freq_mhz * 1e6);
            worker.set_gain(state.ui_gain_db);
            worker.set_sample_rate(SAMPLE_RATES[state.ui_rate_idx]);
            worker.set_channel(state.ui_channel);
            if (!worker.connect("addr=192.168.10.2")) {
                state.error_msg  = worker.get_last_error();
                state.error_popup = true;
                state.status_msg  = "Connection failed.";
            } else {
                state.status_msg = "Connected to USRP X310 (Avgarde)";
            }
        }

        if (state.disconnect_requested) {
            state.disconnect_requested = false;
            recorder.stop();
            worker.disconnect();
            state.status_msg = "Disconnected.";
        }

        // ── Apply RF settings ──────────────────────────────────────────────
        if (state.dd_detection_running &&
            (state.apply_freq || state.apply_gain || state.apply_channel || state.apply_rate)) {
            state.dd_stop_requested = true;
            state.dd_status_msg = "Detection stopped: Recorder radio settings changed.";
            detector.tick(state, worker, recorder.is_recording());
        }
        if (state.apply_freq && worker.is_connected()) {
            worker.set_freq(state.ui_freq_mhz * 1e6);
            state.apply_freq = false;
        }
        if (state.apply_gain && worker.is_connected()) {
            worker.set_gain(state.ui_gain_db);
            state.apply_gain = false;
        }
        if (state.apply_channel && worker.is_connected()) {
            worker.set_channel(state.ui_channel);
            state.apply_channel = false;
        }
        if (state.apply_rate && worker.is_connected()) {
            worker.set_sample_rate(SAMPLE_RATES[state.ui_rate_idx]);
            state.apply_rate = false;
        }
        if (state.apply_fft_size) {
            proc.set_fft_size(FFT_SIZES[state.ui_fft_size_idx]);
            state.apply_fft_size = false;
        }

        // ── Handle record toggle ───────────────────────────────────────────
        if (state.record_requested) {
            state.record_requested = false;
            if (!recorder.is_recording()) {
                // Determine save directory: <record_dir>/<drone_name>
                std::string dname = state.drone_name;
                size_t first = dname.find_first_not_of(" \t\r\n/");
                size_t last  = dname.find_last_not_of(" \t\r\n/");
                if (first == std::string::npos) {
                    dname = "drone";
                } else {
                    dname = dname.substr(first, (last - first + 1));
                }

                std::string save_dir = state.record_dir;
                if (!save_dir.empty() && save_dir.back() != '/') {
                    save_dir += '/';
                }
                save_dir += dname;

                RecordingMeta meta;
                meta.drone_name     = dname;
                meta.center_freq_hz = worker.get_freq();
                meta.sample_rate_hz = worker.get_sample_rate();
                meta.gain_db        = worker.get_gain();
                meta.channel        = worker.get_channel();
                if (recorder.start(save_dir, meta)) {
                    state.status_msg = "Recording started: " + recorder.current_filename();
                } else {
                    state.error_msg   = "Failed to open recording directory: " + save_dir;
                    state.error_popup = true;
                }
            } else {
                recorder.stop();
                state.status_msg = "Recording stopped.";
            }
        }

        detector.tick(state, worker, recorder.is_recording());

        // ── Drain ring buffer → signal processing → recording ─────────────
        frame_samples.clear();
        while (auto* slot = worker.ring.peek_read_slot()) {
            for (auto& s : slot->data) frame_samples.push_back(s);
            worker.ring.consume();
            if (frame_samples.size() > CHUNK_SAMPLES * 16) break; // cap per frame
        }

        if (!frame_samples.empty()) {
            double rate = worker.is_connected()
                ? worker.get_sample_rate()
                : SAMPLE_RATES[state.ui_rate_idx];
            proc.process(frame_samples, rate);

            if (recorder.is_recording())
                recorder.write(frame_samples);
        }

        // ── Begin rendering ────────────────────────────────────────────────
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Full-screen dockspace
        int fb_w, fb_h;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2((float)fb_w, (float)fb_h));
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGuiWindowFlags root_flags =
            ImGuiWindowFlags_NoTitleBar   |
            ImGuiWindowFlags_NoCollapse   |
            ImGuiWindowFlags_NoResize     |
            ImGuiWindowFlags_NoMove       |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus   |
            ImGuiWindowFlags_NoScrollbar  |
            ImGuiWindowFlags_NoScrollWithMouse;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("##root", nullptr, root_flags);
        ImGui::PopStyleVar(2);

        // ── Title bar (custom) ─────────────────────────────────────────────
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.20f, 0.65f, 1.00f, 1.0f));
            ImGui::Text("  🛸  Drone Recorder");
            ImGui::PopStyleColor();
            ImGui::SameLine();

            // Status / freq readout on title line
            if (worker.is_connected()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.5f, 1.0f));
                ImGui::Text("  ●  %.4f MHz  |  %.0f Msps  |  %.1f dB  |  Ch %s",
                    worker.get_freq() / 1e6,
                    worker.get_sample_rate() / 1e6,
                    worker.get_gain(),
                    worker.get_channel() == 0 ? "A" : "B");
                ImGui::PopStyleColor();
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.60f, 1.0f));
                ImGui::TextUnformatted("  ○  Not Connected");
                ImGui::PopStyleColor();
            }

            if (recorder.is_recording()) {
                float t = static_cast<float>(ImGui::GetTime());
                float pulse = 0.5f + 0.5f * std::sin(t * 5.0f);
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text,
                    ImVec4(0.9f + 0.1f * pulse, 0.2f, 0.2f, 1.0f));
                ImGui::Text("  ● REC  %.1f MB",
                    recorder.bytes_written() / (1024.0 * 1024.0));
                ImGui::PopStyleColor();
            }

            ImGui::Separator();
        }

        // ── Top-level tabs: Recorder | Drone ID Detector ───────────────────
        if (ImGui::BeginTabBar("##main_tabs", ImGuiTabBarFlags_None)) {

            if (ImGui::BeginTabItem("  Recorder  ")) {
                // Two-pane layout: Control (left) | Splitter | Plots (right)
                float panel_h = ImGui::GetContentRegionAvail().y;
                float avail_total_w = ImGui::GetContentRegionAvail().x;
                float splitter_w = 8.0f;

                float min_ctrl = 200.0f;
                float max_ctrl = std::max(min_ctrl, avail_total_w - 300.0f);
                if (state.ctrl_width < min_ctrl) state.ctrl_width = min_ctrl;
                if (state.ctrl_width > max_ctrl) state.ctrl_width = max_ctrl;

                float ctrl_w = state.ctrl_width;
                float plot_w = avail_total_w - ctrl_w - splitter_w;

                // Left pane — controls
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.08f, 0.13f, 1.0f));
                ImGui::BeginChild("##control_pane", ImVec2(ctrl_w, panel_h),
                                  ImGuiChildFlags_Borders, ImGuiWindowFlags_None);
                DrawControlPanel(state, &worker, &recorder);
                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::SameLine(0, 0);

                // Divider between Control Panel and Plot Panel
                DrawVerticalSplitter("##main_pane_split", state.ctrl_width, 280.0f, min_ctrl, max_ctrl, panel_h);

                ImGui::SameLine(0, 0);

                // Right pane — plots (allows vertical scrolling when plots are enlarged)
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.06f, 0.07f, 0.11f, 1.0f));
                ImGui::BeginChild("##plot_pane", ImVec2(plot_w, panel_h),
                                  ImGuiChildFlags_None, ImGuiWindowFlags_None);
                DrawPlotPanel(state, proc,
                              worker.is_connected() ? worker.get_freq() : state.ui_freq_mhz * 1e6,
                              worker.is_connected() ? worker.get_sample_rate() : SAMPLE_RATES[state.ui_rate_idx]);
                ImGui::EndChild();
                ImGui::PopStyleColor();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("  Drone ID Detector  ")) {
                DrawDroneIdPanel(state, &worker);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        // ── Error popup ────────────────────────────────────────────────────
        if (state.error_popup) {
            ImGui::OpenPopup("Error##errpop");
            state.error_popup = false;
        }
        ImVec2 center = ImVec2((float)fb_w * 0.5f, (float)fb_h * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Error##errpop", nullptr,
                ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::TextWrapped("%s", state.error_msg.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(120, 0)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::End(); // root

        // ── Render ──────────────────────────────────────────────────────────
        ImGui::Render();
        glViewport(0, 0, fb_w, fb_h);
        glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // ─── Cleanup ──────────────────────────────────────────────────────────────
    detector.shutdown(worker);
    worker.set_detector_sink({});
    recorder.stop();
    worker.disconnect();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
