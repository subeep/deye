# deye — Drone Recorder & RF Protocol Detector

A high-performance C++17 and Python SDR application for real-time IQ signal reception, zero-loss RF recording, spectral visualization, and drone RF detection with **DJI DroneID** demodulation, **ExpressLRS (ELRS)** candidate detection, and DIY drone telemetry analysis using Ettus USRP Software Defined Radios (e.g., USRP X310, B210).

---

## Overview

**deye** (Drone Recorder) provides a unified tactical interface for RF monitoring and drone protocol analysis:
- **RF Spectrum & Time-Domain Visualization**: Hardware-accelerated real-time plotting powered by Dear ImGui, ImPlot, and FFTW3.
- **Continuous IQ Recording**: High-throughput lock-free ring buffer architecture streaming complex float32 samples directly to disk with JSON metadata sidecars.
- **Live Drone ID & Protocol Detector**: Integrated receive-only DSP engine detecting and decoding DJI DroneID telemetry bursts, LoRa/ELRS modulation candidates, 2-FSK control links, and analog FPV video transmissions.
- **Offline Batch Processing**: Command-line interface for analyzing stored IQ recordings without hardware attached.

---

## Key Features

### 1. Dual-Channel USRP Streaming & Recording
- **Hardware Integration**: Native Ettus UHD 4.0+ driver integration supporting USRP X310, B210, and compatible frontends.
- **Zero-Loss Ring Buffer**: Single-producer single-consumer (SPSC) lock-free ring buffer decouples high-speed UHD network RX from GUI rendering and disk I/O.
- **Standardized Recording Format**:
  - Binary IQ: Interleaved 32-bit floating point complex samples (`.bin` / `.fc32`).
  - JSON Sidecar: Captures UTC timestamp, center frequency, sample rate, analog gain, channel ID, antenna port, and total sample count.

### 2. Live Drone Protocol & Emitter Detection

| Protocol / Target | Detection & Demodulation Method | Decoded Output & Certainty |
| :--- | :--- | :--- |
| **DJI DroneID** | Coarse/fine burst extraction, CFO correction, OFDM/QPSK demodulation, Gold sequence descrambling, SIMD-accelerated LTE turbo decoding, CRC24A & CRC16 validation | **Validated Identity**: Aircraft Serial Number, Drone Model, Latitude, Longitude, Altitude, Velocity, Home Location |
| **LoRa CSS / ExpressLRS** | Chirp Spread Spectrum (CSS) dechirped preamble detection across SF5–SF9, 500 / 812.5 / 1625 kHz bandwidths | **Modulation Candidate**: Peak SNR, center frequency, bandwidth, spreading factor |
| **ExpressLRS FHSS Sequence** | Upstream-verified FHSS permutation reconstruction from user-provided 32-bit firmware seeds across 2.4 GHz, FCC915, EU868, and IN866 domains | **Reconstructed Schedule**: Frequency sequence generation and channel hopping |
| **DIY FSK Control / Telemetry** | 2-state instantaneous frequency clustering | **Waveform Candidate**: Center frequency, deviation, symbol rate estimate (FrSky / FlySky / FLRC) |
| **Wi-Fi / Drone OFDM** | Cyclic-prefix correlation and autocorrelation | **Waveform Candidate**: OFDM burst timing and frame detection |
| **Analog FPV Video** | FM demodulation and PAL/NTSC horizontal sync periodicity detection | **Video Candidate**: Video carrier and frame line timing |

### 3. Flexible Acquisition Modes
- **Fixed Frequency**: Continuously monitor a single channel or custom RF center frequency.
- **Channel Scan**: Sweeps predefined channel tables with configurable dwell times (100 ms to 3000 ms).
- **Scan & Hold on Valid DJI ID**: Automatically pauses channel scanning when a valid CRC24A/CRC16 DJI packet is decoded, preserving lock on active drones before resuming scan.

### 4. Modern Tactical GUI
- Custom dark tactical styling with adjustable split-pane layouts.
- Time-domain waveform oscilloscope and FFT power spectrum analyzer.
- Real-time diagnostic metrics: analyzed samples, queue drops, batch processing latency, and buffer health.
- Interactive observation table with persistent column headers, sortable fields, and payload inspection.

---

## Architecture

```
                      ┌──────────────────────┐
                      │   Ettus USRP SDR     │ (X310 / B210)
                      └──────────┬───────────┘
                                 │ UHD 10GbE / USB3
                                 ▼
                      ┌──────────────────────┐
                      │   UHD RX Worker      │
                      │  (High-Priority Thd) │
                      └──────────┬───────────┘
                                 │
           ┌─────────────────────┼─────────────────────┐
           │                     │                     │
           ▼                     ▼                     ▼
┌────────────────────┐ ┌────────────────────┐ ┌────────────────────┐
│ Lock-Free RingBuf  │ │   Disk Recorder    │ │  Detector Worker   │
│  (SPSC, 64 slots)  │ │ (Binary + JSON)    │ │  (IPC Controller)  │
└──────────┬─────────┘ └────────────────────┘ └──────────┬─────────┘
           │                                             │ Bounded Pipe
           ▼                                             ▼
┌────────────────────┐                        ┌────────────────────┐
│   Dear ImGui UI    │                        │  Python DSP Engine │
│ Time & FFT Scopes  │                        │ (Burst, Gold, QPSK)│
└────────────────────┘                        └──────────┬─────────┘
                                                         │
                                                         ▼
                                              ┌────────────────────┐
                                              │ SIMD libdrone_turbo│
                                              │ (SSSE3 Turbo FEC)  │
                                              └────────────────────┘
```

---

## System Requirements & Prerequisites

### Platform
- **OS**: Linux (tested on Ubuntu 20.04 / 22.04 / 24.04 LTS)
- **CPU**: x86_64 processor supporting **SSSE3** (required by SIMD Turbo decoder)
- **SDR**: Ettus USRP (X300/X310 with TwinRX/UBX, B200/B210, etc.)

### System Packages
Install required build tools, UHD drivers, and GUI libraries:

```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    pkg-config \
    libuhd-dev \
    uhd-host \
    libglfw3-dev \
    libgl1-mesa-dev \
    libfftw3-dev \
    python3 \
    python3-pip \
    python3-dev
```

### Python Dependencies
Install required Python packages for the detector DSP engine:

```bash
pip install -r src/detector/python/requirements.txt
```

*(Requirements: `numpy>=1.24,<2`, `scipy>=1.10,<2`, `matplotlib>=3.5,<4`, `bitarray>=2.4,<4`, `crcmod==1.7`)*

---

## Building from Source

1. **Clone the repository:**
   ```bash
   git clone https://github.com/subeep/deye.git
   cd deye
   ```

2. **Configure with CMake:**
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   ```

3. **Compile:**
   ```bash
   cmake --build build -j$(nproc)
   ```

   This produces:
   - `build/drone_recorder`: Main GUI application
   - `build/libdrone_turbo.so`: SSSE3 SIMD Turbo FEC shared library
   - Test binaries: `test_hopping`, `test_detector_worker`, `test_controller`

---

## Running Automated Tests

Run the full automated test suite (including validation on real DJI Mavic Air 2 and Mini 2 IQ captures, ELRS FHSS sequences, and DSP pipelines):

```bash
ctest --test-dir build --output-on-failure
```

Expected output:
```
1/4 Test #1: hopping ..........................   Passed
2/4 Test #2: detector_dsp .....................   Passed
3/4 Test #3: detector_worker ..................   Passed
4/4 Test #4: detector_controller ..............   Passed

100% tests passed, 0 tests failed out of 4
```

---

## Usage

### 1. Launching the GUI
Use the provided launcher script (which ensures clean library and linker paths):

```bash
./run.sh
```

Or run the binary directly:
```bash
./build/drone_recorder
```

### 2. Operating the Application

#### **Recorder Tab**
1. Select USRP channel (**Channel A** or **Channel B**), center frequency, sample rate, and gain.
2. Click **Connect USRP** to establish UHD stream.
3. Specify output directory and drone label.
4. Click **Start Recording** to stream raw IQ to `.bin` with a `.json` sidecar.
5. Inspect real-time RF power and spectrum via the interactive plot.

#### **Drone ID Detector Tab**
1. Open the **Drone ID Detector** tab.
2. Select target profile (e.g. *DJI 2.4 GHz*, *DJI 5.8 GHz*, *ELRS 2.4 GHz*, *ELRS 915 MHz*, or *Custom*).
3. Choose acquisition mode:
   - **Fixed**: Lock to a designated center frequency.
   - **Scan Channels**: Step through channels at configured dwell times (100–3000 ms).
   - **Scan / Hold on Valid DJI ID**: Retain dwell when a valid packet is captured.
4. Click **Start Detector**. Validated DroneID packets (serial, GPS, altitude, velocity) and detected candidate waveforms will populate the observation list in real time.

### 3. Headless Offline IQ Analysis
Analyze pre-recorded `.fc32` files using the standalone DSP engine:

```bash
python3 src/detector/python/backend.py \
  --turbo build/libdrone_turbo.so \
  --input tests/data/dji_mavic_air_2.fc32 \
  --rate 50000000 \
  --frequency 2444500000
```

Outputs structured JSON containing decoded DroneID packets, candidate waveforms, SNR, and processing metrics.

---

## Repository Structure

```
├── CMakeLists.txt                # Root CMake build configuration
├── run.sh                        # Application launch script
├── docs/
│   └── DETECTOR.md               # Detailed detector implementation & validation guide
├── src/
│   ├── main.cpp                  # Entry point and Dear ImGui render loop
│   ├── app_state.hpp             # Shared application state and thread synchronization
│   ├── usrp_worker.hpp/.cpp      # UHD RX thread and lock-free SPSC ring buffer
│   ├── signal_proc.hpp/.cpp      # FFTW3 spectrum analysis and peak detection
│   ├── recorder.hpp/.cpp         # Raw IQ binary stream & JSON sidecar writer
│   ├── detector/                 # Drone ID & RF detection subsystem
│   │   ├── controller.hpp/.cpp   # Frequency hopping, dwell timers, and channel profiles
│   │   ├── detector.hpp/.cpp     # C++ subprocess IPC worker for Python DSP
│   │   ├── hopping.hpp/.cpp      # ExpressLRS FHSS sequence permutation generator
│   │   ├── turbo_bridge.c        # C bridge to SSSE3 Turbo FEC decoder
│   │   └── python/
│   │       ├── backend.py        # Core DSP analyzer (burst, gold, QPSK, framing)
│   │       ├── requirements.txt  # Python package dependencies
│   │       └── reference/        # Reverse-engineering reference code and fixtures
│   └── ui/                       # ImGui / ImPlot user interface components
│       ├── control_panel.hpp/.cpp# USRP controls, tuner, and recording management
│       ├── plot_panel.hpp/.cpp   # Time-domain and FFT power spectrum displays
│       ├── droneid_panel.hpp/.cpp# DroneID observation table, stats, and hopper UI
│       └── splitter.hpp          # Dynamic resizable panel splitters
├── tests/                        # Automated unit and integration tests
│   ├── data/                     # Published real-world test IQ captures (.fc32)
│   ├── test_hopping.cpp          # ELRS hopping permutation golden vector tests
│   ├── test_detector.py          # Python DSP pipeline tests
│   ├── test_detector_worker.cpp  # C++ IPC detector worker test
│   └── test_controller.cpp       # Frequency hopper and state machine test
└── vendor/                       # Vendored third-party dependencies
    ├── imgui/                    # Dear ImGui (v1.90+ Docking / Viewports)
    ├── implot/                   # ImPlot plotting library
    └── turbofec/                 # SSSE3 SIMD LTE Turbo decoder
```

---

## Hardware Configuration Notes

- **Network Buffer Optimization (USRP X310)**:
  To avoid UDP packet drops at sample rates above 20 MS/s, increase your Linux network socket buffers:
  ```bash
  sudo sysctl -w net.core.rmem_max=67108864
  sudo sysctl -w net.core.wmem_max=67108864
  ```
- **Clock Rates**: USRP X310 uses a 200 MHz master clock. Requesting 25 MS/s or 50 MS/s yields exact integer decimation ratios for optimal filter performance.

---

## Provenance and Licensing

This project incorporates and adapts specialized open-source works:
- **DroneID Analysis & Protocol Logic**: Adapted from [RUB-SysSec/DroneSecurity](https://github.com/RUB-SysSec/DroneSecurity) (commit `9ff8198`), licensed under **AGPL-3.0**.
- **Turbo FEC Decoder**: `vendor/turbofec` derived from [ttsou/turbofec](https://github.com/ttsou/turbofec), licensed under **GPL-3.0-or-later**.
- **ExpressLRS FHSS Algorithms**: Based on [ExpressLRS](https://github.com/ExpressLRS/ExpressLRS) (commit `a60b68a`), licensed under **GPL-3.0**.
- **UI & Plotting**: [Dear ImGui](https://github.com/ocornut/imgui) and [ImPlot](https://github.com/epezent/implot), licensed under the **MIT License**.

See [docs/DETECTOR.md](docs/DETECTOR.md) and individual source directories for full license texts and author credits.
