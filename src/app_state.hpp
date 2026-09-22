#pragma once
#include "usrp_worker.hpp"
#include "signal_proc.hpp"
#include "recorder.hpp"
#include "detector/detector.hpp"
#include <string>
#include <vector>
#include <atomic>

// ─── Application State ───────────────────────────────────────────────────────
// All state shared between control panel UI and main loop.
struct AppState {
    // ── USRP Connection ──────────────────────────────────────────────────────
    bool     connect_requested{false};
    bool     disconnect_requested{false};

    // ── RF Settings (UI values, written by control panel) ────────────────────
    float    ui_freq_mhz{433.0f};     // Center frequency in MHz
    float    ui_gain_db{20.0f};       // RX Gain dB
    int      ui_channel{0};           // 0 = Ch A, 1 = Ch B
    int      ui_rate_idx{0};          // Index into sample_rates[]
    int      ui_fft_size_idx{1};      // Index into fft_sizes[]

    bool     apply_freq{false};
    bool     apply_gain{false};
    bool     apply_channel{false};
    bool     apply_rate{false};
    bool     apply_fft_size{false};

    // ── Recording ────────────────────────────────────────────────────────────
    bool     record_requested{false};  // Toggle
    char     drone_name[128];          // Drone identifier → saved as subdirectory
    char     record_dir[512];          // Base output directory path

    // ── Plot Visibility (collapsible toggles) ─────────────────────────────────
    bool     show_fft{true};
    bool     show_stft{true};
    bool     show_spectrogram{true};

    // ── Plot Sizing & Layout (user-adjustable) ─────────────────────────────────
    float    ctrl_width{280.0f};           // Width of left control panel
    float    iq_height{220.0f};            // Height of IQ constellation & time plots
    float    iq_split_ratio{0.42f};        // Width ratio for Constellation vs Time Domain
    float    fft_height{220.0f};           // Height of FFT plot
    float    stft_height{220.0f};          // Height of STFT plot
    float    spectrogram_height{280.0f};   // Height of Spectrogram waterfall

    // ── Status ────────────────────────────────────────────────────────────────
    std::string status_msg;
    bool     error_popup{false};
    std::string error_msg;

    // ── DroneID Detector tab ───────────────────────────────────────────────────
    bool dd_use_custom_freq{false};
    float dd_custom_freq_mhz{2414.5f};
    float dd_gain_db{30.0f};
    bool dd_detection_running{false};
    std::string dd_status_msg{"Idle. Select a link profile to begin."};
    int dd_profile{0};
    int dd_mode{0}; // fixed, channel scan, hold on validated DJI, ELRS reconstructed order
    int dd_profile_channel{0};
    int dd_dwell_ms{800};
    int dd_hold_ms{3000};
    unsigned dd_seed{1};
    bool dd_start_requested{false};
    bool dd_stop_requested{false};
    bool dd_recording_busy{false};
    bool dd_connect_requested{false};
    drone::Snapshot dd_snapshot;
    size_t dd_scan_position{0};
    double dd_target_hz{0};
    double dd_actual_rate{0};
    float dd_observations_height{420.0f};

    AppState() {
        snprintf(drone_name, sizeof(drone_name), "drone");
        std::string home = getenv("HOME") ? getenv("HOME") : "/tmp";
        std::string def  = home + "/iq_recordings";
        snprintf(record_dir, sizeof(record_dir), "%s", def.c_str());
    }
};

// ─── Available options ────────────────────────────────────────────────────────
static constexpr double SAMPLE_RATES[]  = {1e6, 2e6, 5e6, 10e6, 20e6, 25e6, 50e6};
static constexpr const char* RATE_LABELS[] = {"1 Msps","2 Msps","5 Msps","10 Msps","20 Msps","25 Msps","50 Msps"};
static constexpr int RATE_COUNT = 7;

static constexpr int FFT_SIZES[]  = {512, 1024, 2048, 4096};
static constexpr const char* FFT_LABELS[] = {"512","1024","2048","4096"};
static constexpr int FFT_SIZE_COUNT = 4;
