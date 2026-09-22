#include "plot_panel.hpp"
#include "splitter.hpp"
#include "imgui.h"
#include "implot.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <vector>

static void PlotSectionHeader(const char* title, bool& visible, ImVec4 accent) {
    ImGui::PushStyleColor(ImGuiCol_Header,        ImVec4(accent.x * 0.3f, accent.y * 0.3f, accent.z * 0.3f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(accent.x * 0.5f, accent.y * 0.5f, accent.z * 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,  ImVec4(accent.x * 0.4f, accent.y * 0.4f, accent.z * 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(accent.x, accent.y, accent.z, 1.0f));
    visible = ImGui::CollapsingHeader(title, visible ? ImGuiTreeNodeFlags_DefaultOpen : 0);
    ImGui::PopStyleColor(4);
}

void DrawPlotPanel(AppState& state, const SignalProcessor& proc,
                   double center_freq_hz, double sample_rate_hz) {

    // ── Derived constants ────────────────────────────────────────────────────
    double cf_mhz    = center_freq_hz * 1e-6;
    double half_bw   = sample_rate_hz * 0.5e-6;          // MHz
    double freq_lo   = cf_mhz - half_bw;
    double freq_hi   = cf_mhz + half_bw;

    // ─── IQ CONSTELLATION + TIME DOMAIN ──────────────────────────────────────
    {
        float avail_w = ImGui::GetContentRegionAvail().x;
        float iq_h    = state.iq_height;

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 1.0f, 1.0f));
        ImGui::TextUnformatted("  IQ Plots");
        ImGui::PopStyleColor();

        float splitter_w = 8.0f;
        float total_w = avail_w - splitter_w - ImGui::GetStyle().ItemSpacing.x * 2.0f;
        if (total_w < 200.0f) total_w = 200.0f;
        float min_const_w = 80.0f;
        float max_const_w = total_w - 80.0f;
        float const_w = total_w * state.iq_split_ratio;
        if (const_w < min_const_w) const_w = min_const_w;
        if (const_w > max_const_w) const_w = max_const_w;

        // ── Constellation ──────────────────────────────────────────────────
        if (ImPlot::BeginPlot("##constellation", ImVec2(const_w, iq_h),
                              ImPlotFlags_Equal | ImPlotFlags_NoTitle)) {
            ImPlot::SetupAxes("I", "Q",
                ImPlotAxisFlags_None, ImPlotAxisFlags_None);
            ImPlot::SetupAxisLimits(ImAxis_X1, -1.2, 1.2, ImGuiCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1.2, 1.2, ImGuiCond_Once);

            const auto& qi = proc.iq_i();
            const auto& qq = proc.iq_q();
            int n = std::min((int)qi.size(), 2048);

            ImPlot::PlotScatter("##const",
                qi.data() + (int)qi.size() - n,
                qq.data() + (int)qq.size() - n, n,
                ImPlotSpec(
                    ImPlotProp_Marker,          (int)ImPlotMarker_Circle,
                    ImPlotProp_MarkerSize,       1.5f,
                    ImPlotProp_MarkerFillColor,  ImVec4(0.20f, 0.65f, 1.00f, 0.6f),
                    ImPlotProp_LineWeight,        0.0f
                ));

            ImPlot::EndPlot();
        }

        ImGui::SameLine(0, 4);

        // ── Divider between Constellation and Time Domain ──────────────────
        float cur_const_w = const_w;
        if (DrawVerticalSplitter("##iq_split", cur_const_w, total_w * 0.42f, min_const_w, max_const_w, iq_h)) {
            state.iq_split_ratio = cur_const_w / total_w;
        }

        ImGui::SameLine(0, 4);

        // ── I/Q Time Domain ────────────────────────────────────────────────
        if (ImPlot::BeginPlot("##timedomain", ImVec2(-1, iq_h), ImPlotFlags_NoTitle)) {
            double us_extent = (double)SignalProcessor::IQ_DISPLAY_LEN / sample_rate_hz * 1e6;
            ImPlot::SetupAxes("Time (µs)", "Amplitude",
                ImPlotAxisFlags_None, ImPlotAxisFlags_None);
            ImPlot::SetupAxisLimits(ImAxis_X1, -us_extent, 0, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -1.3, 1.3, ImGuiCond_Once);

            const auto& ti = proc.iq_t();
            const auto& qi = proc.iq_i();
            const auto& qq = proc.iq_q();
            int n = static_cast<int>(ti.size());

            ImPlot::PlotLine("I", ti.data(), qi.data(), n,
                ImPlotSpec(ImPlotProp_LineColor, ImVec4(0.20f, 0.65f, 1.00f, 1.0f),
                           ImPlotProp_LineWeight, 1.0f));

            ImPlot::PlotLine("Q", ti.data(), qq.data(), n,
                ImPlotSpec(ImPlotProp_LineColor, ImVec4(1.00f, 0.40f, 0.60f, 1.0f),
                           ImPlotProp_LineWeight, 1.0f));

            ImPlot::EndPlot();
        }

        // Resizable height handle for IQ section
        DrawHorizontalSplitter("##iq_h_split", state.iq_height, 220.0f, 100.0f, 1000.0f);
    }

    ImGui::Spacing();

    // ─── FFT (COLLAPSIBLE) ────────────────────────────────────────────────────
    PlotSectionHeader("  Power Spectrum (FFT)", state.show_fft,
                      ImVec4(0.20f, 0.80f, 0.55f, 1.0f));
    if (state.show_fft) {
        if (ImPlot::BeginPlot("##fft", ImVec2(-1, state.fft_height), ImPlotFlags_NoTitle)) {
            ImPlot::SetupAxes("Frequency (MHz)", "Power (dBFS)");
            ImPlot::SetupAxisLimits(ImAxis_X1, freq_lo, freq_hi, ImGuiCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -120, 0, ImGuiCond_Once);

            const auto& freqs = proc.fft_freqs();
            const auto& power = proc.fft_power();
            const auto& avg   = proc.fft_avg();
            int n = static_cast<int>(freqs.size());

            // Convert Hz offset → absolute MHz
            static std::vector<float> freq_abs_mhz;
            freq_abs_mhz.resize(n);
            for (int k = 0; k < n; ++k)
                freq_abs_mhz[k] = static_cast<float>(cf_mhz) + freqs[k] * 1e-6f;

            // Instantaneous — semi-transparent
            ImPlot::PlotLine("Instant", freq_abs_mhz.data(), power.data(), n,
                ImPlotSpec(ImPlotProp_LineColor, ImVec4(0.20f, 0.60f, 1.00f, 0.5f),
                           ImPlotProp_LineWeight, 1.0f));

            // Rolling average — bright green
            ImPlot::PlotLine("Avg", freq_abs_mhz.data(), avg.data(), n,
                ImPlotSpec(ImPlotProp_LineColor, ImVec4(0.10f, 0.95f, 0.55f, 1.0f),
                           ImPlotProp_LineWeight, 1.5f));

            ImPlot::EndPlot();
        }

        // Resizable height handle for FFT
        DrawHorizontalSplitter("##fft_h_split", state.fft_height, 220.0f, 90.0f, 1200.0f);
    }

    ImGui::Spacing();

    // ─── STFT (COLLAPSIBLE) ───────────────────────────────────────────────────
    PlotSectionHeader("  Short-Time FFT (STFT)", state.show_stft,
                      ImVec4(0.10f, 0.75f, 0.90f, 1.0f));
    if (state.show_stft) {
        const auto& hist  = proc.stft_history();
        const auto& freqs = proc.fft_freqs();
        int n_freq = static_cast<int>(freqs.size());

        static std::vector<float> freq_abs_mhz;
        freq_abs_mhz.resize(n_freq);
        for (int k = 0; k < n_freq; ++k)
            freq_abs_mhz[k] = static_cast<float>(cf_mhz) + freqs[k] * 1e-6f;

        if (ImPlot::BeginPlot("##stft", ImVec2(-1, state.stft_height), ImPlotFlags_NoTitle)) {
            ImPlot::SetupAxes("Frequency (MHz)", "Power (dBFS)");
            ImPlot::SetupAxisLimits(ImAxis_X1, freq_lo, freq_hi, ImGuiCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -120, 0, ImGuiCond_Once);

            int n_slices = std::min((int)hist.size(), 16);
            for (int r = n_slices - 1; r >= 0; --r) {
                float alpha = 1.0f - (float)r / (float)n_slices;
                float blue  = 0.3f + 0.7f * alpha;
                char lbl[24];
                snprintf(lbl, sizeof(lbl), "##stft_%d", r);
                if ((int)hist[r].size() == n_freq)
                    ImPlot::PlotLine(lbl, freq_abs_mhz.data(), hist[r].data(), n_freq,
                        ImPlotSpec(ImPlotProp_LineColor,
                                   ImVec4(0.05f, 0.5f * alpha, blue, alpha),
                                   ImPlotProp_LineWeight, 1.0f));
            }
            ImPlot::EndPlot();
        }

        // Resizable height handle for STFT
        DrawHorizontalSplitter("##stft_h_split", state.stft_height, 220.0f, 90.0f, 1200.0f);
    }

    ImGui::Spacing();

    // ─── SPECTROGRAM / WATERFALL (COLLAPSIBLE) ────────────────────────────────
    PlotSectionHeader("  Spectrogram (Waterfall)", state.show_spectrogram,
                      ImVec4(0.70f, 0.35f, 1.00f, 1.0f));
    if (state.show_spectrogram) {
        float colorbar_w = 60.0f;
        float plot_w = ImGui::GetContentRegionAvail().x - colorbar_w - ImGui::GetStyle().ItemSpacing.x;
        if (plot_w < 100.0f) plot_w = 100.0f;

        const auto& flat = proc.spectrogram_data();
        int fft_n  = proc.get_fft_size();

        if (!flat.empty() && ImPlot::BeginPlot("##spectrogram", ImVec2(plot_w, state.spectrogram_height),
                ImPlotFlags_NoTitle)) {
            ImPlot::SetupAxes("Frequency (MHz)", "Time (slices ago)",
                ImPlotAxisFlags_None, ImPlotAxisFlags_Invert);
            ImPlot::SetupAxisLimits(ImAxis_X1, freq_lo, freq_hi, ImGuiCond_Once);
            ImPlot::SetupAxisLimits(ImAxis_Y1, 0, STFT_HISTORY, ImGuiCond_Always);

            ImPlot::PushColormap(ImPlotColormap_Plasma);
            ImPlot::PlotHeatmap("##heatmap",
                flat.data(), STFT_HISTORY, fft_n,
                -110.0, -20.0,
                nullptr,
                ImPlotPoint(freq_lo, 0),
                ImPlotPoint(freq_hi, STFT_HISTORY));
            ImPlot::PopColormap();

            ImPlot::EndPlot();
        }

        // Colorbar outside the main plot
        if (!flat.empty()) {
            ImGui::SameLine();
            ImPlot::PushColormap(ImPlotColormap_Plasma);
            ImPlot::ColormapScale("dBFS", -110.0, -20.0, ImVec2(colorbar_w, state.spectrogram_height));
            ImPlot::PopColormap();
        }

        // Resizable height handle for Spectrogram
        DrawHorizontalSplitter("##sg_h_split", state.spectrogram_height, 280.0f, 100.0f, 1400.0f);
    }
}

