# Drone detector — plan, decisions and progress

Updated: 2026-09-23.
Task checklist: [DETECTOR_TODO.md](DETECTOR_TODO.md).
Current implementation guide: [DETECTOR.md](DETECTOR.md).

## Objective and boundaries

Improve the existing receive-only detector into a measured, extensible detector
for DJI and DIY drone links. Distinguish RF activity, modulation candidates,
validated protocols and aircraft identities. Keep Recorder UI, recording logic
and file format unchanged. Shared radio changes must be necessary and regression-tested.

The user has authorized incremental detector implementation and dataset integration,
with these documents maintained during work. Test-generator
firmware is a separate future component; it does not turn the detector into a transmitter.

## Current status

| Area | Status | Evidence / remaining limit |
| --- | --- | --- |
| DJI DroneID | Implemented; validated on two published captures | Mini 2: eight valid packets; Mavic Air 2: one. CRC24A and CRC16 required. Not universal DJI coverage. |
| User's Mini 2 | User reports successful detection on 2026-09-23 | Exact session, firmware and validated-ID evidence not yet archived; do not count as a measured performance benchmark. |
| DIY classification | Waveform candidates only | CSS/FSK/OFDM/FM-video tests; no validated ELRS/FrSky/FlySky packet decoder yet. |
| Hopping | Scan lists and seeded ELRS order reconstruction | Upstream sequence comparison passed; no synchronized RF hop following. |
| X310 | Live receive/retune demonstrated | Earlier optimized smoke run analyzed about 66.2M samples, dropped about 12.0M detector-queue samples; no live DJI ID in that short run. |
| Automated tests | Seven suites pass (2026-09-23, 8.17 s) | Adds preprocessing, real channelized Air 2, DC worker, gap accounting and 100 MS/s batch-limit checks. All 12 raw/DC replay cases pass. |
| DroneDetect_V2 | Read-only audit and bounded pilot implemented | 390 files audited; 84 windows from 28 ON recordings replayed, zero validated IDs. Acquisition metadata remains provisional; no accuracy claim or classifier training. |
| UI/radio fixes | Implemented and rebuilt | Stop retains detector RF settings; detector connect uses selected profile; observations default to 420 px and resize by divider. |
| Heltec / Nucleo / LR2021 | User has hardware; integration pending | No board flashed, wired or validated by this project. Exact Heltec RF variant and LR2021 module pinout required. |

Existing implementation lives in `src/detector/`, detector UI in
`src/ui/droneid_panel.cpp`, and validation in `tests/`. The Recorder source and
control/plot source checksums were unchanged in prior verification.

## Execution order and acceptance criteria

### Phase 1 — reproducible software benchmark (S01–S03)

Create a capture manifest recording source, model, firmware, band, center
frequency, actual sample rate, IQ format, gain, antenna, operating state,
capture time, labels, expected packets and provenance/licensing. Unknown fields
remain explicitly unknown. Track capture hashes and software/decoder versions.

Build a batch runner around the same analyzer used for live reception. Emit
JSON plus a readable report with per-capture results and aggregate metrics.
Separate development captures from held-out sessions; do not split adjacent
chunks of one capture across both sets. Include ordinary Wi-Fi/BLE, non-drone
LoRa/FSK, noise and tones as negatives. Synthetic and real captures stay labeled.

Acceptance: reproducible results for existing DJI fixtures, no confirmed drone
on the defined negative set, deterministic reports except measured timing, and
an explicit support matrix. General LoRa must not become confirmed ELRS.
Hardware: none required for the runner or existing fixtures.

### Phase 2 — early real Mini 2 checkpoint (H01–H04)

Do this after the basic runner works, before extensive classifier tuning.
Use X310 plus Mini 2/controller; no Nucleo or Heltec is needed. Log aircraft and
controller states independently, video activity, orientations, receiver settings
and firmware. Use repeated sessions at fixed channels before testing scanning.
Stationary bench tests do not require flight; use props removed where appropriate.

Capture via the existing Recorder or a new detector-only evidence capture facility;
do not change Recorder behavior. Archive the user's validated-ID example, if
available, separately from candidate-only observations.

Acceptance: labeled repeatable positive/negative sessions, a held-out set, and a
baseline report that distinguishes scanning misses from decoding failures.
Drone support is specific to model, firmware, band, mode and tested conditions.

### Phase 3 — reception and extensibility (S04–S07)

Instrument UHD discontinuities, host queue loss, analyzed samples, queue age,
processing latency, actual receiver tuning and time spent away from each channel.
Do not treat sample power in dBFS as calibrated antenna RSSI or range.
Profile before optimizing; consider bounded parallel processing/native DSP only
where measurements justify it. Preserve timing and reset partial frames at gaps.

Introduce protocol modules with declared bands, sample-rate requirements,
detection/validation methods and supported result fields. Add validated editable
profiles with import/export, stale-result aging, evidence details and detector
session export. Reject invalid configurations visibly.

Acceptance: no regression on frozen captures; benchmark before/after loss and
latency under the same conditions. Report remaining loss instead of promising
zero loss. Radio ownership tests and unchanged Recorder checks must pass.

### Phase 4 — hardware test bench (S08, G01–G06)

Define host commands and logs before board-specific firmware. Proposed commands:
identify capabilities, configure one supported waveform, select frequency/power,
set packet count/interval, select deterministic channel order, start and stop.
Every run gets an ID and a configuration snapshot. Number packets; log scheduled
and observed TX completion times, errors and hop indices. USB host timestamps
alone are not accurate RF timing ground truth; use hardware events where possible.

Bring up one fixed-channel mode first, then parameter sweeps and hopping. Keep
waveform tests distinct from real protocol emulation. Correct PHY modulation
alone does not reproduce packet framing, whitening, CRC, timing or telemetry.

Acceptance: independent reception of known test payloads, reproducible logs and
measured frequency/timing behavior. A generator and decoder agreeing does not
independently prove protocol compatibility.

### Phase 5 — protocol validation and broader identification (D01–D05, R01–R03)

Start with one explicitly selected ELRS version/band/mode. Establish external
reference vectors or captures; implement synchronization, demodulation, framing
and integrity validation. Cross-check on an independent genuine transmitter/
receiver pair or trusted independent captures before claiming ELRS support.
Then expand DJI coverage and FrSky/FlySky one protocol/model at a time.

Add standardized Remote ID as a separate receive backend, initially one supported
BLE or Wi-Fi transport. Use an established ESP32-S3 implementation for a test
endpoint with clearly identified test data. Confirm decoding independently before
integrating it. One board cannot be assumed to provide an independent simultaneous
transmitter and receiver test; use a second compatible receiver or separate runs.

Acceptance: independent positive evidence, malformed/integrity-failure rejection,
negative traffic tests and measured support limits. A valid broadcast ID is not
authenticated proof of physical aircraft identity. Protocol detection does not
necessarily reveal the aircraft manufacturer or model.

### Phase 6 — adaptive scanning and link tracking (A01–A05, V01)

Prioritize occupied channels without hiding blind time. Display measured activity,
predicted hop order and actual receiver tuning separately. Test synchronization
and reacquisition against known generator logs before attempting supported live
protocol tracking. Matching a seed is not time synchronization. Account for the
X310's instantaneous bandwidth, RF retune/settle latency and host scheduling.

Associate ID/control/video/telemetry only with identifying evidence; proximity in
frequency or time alone must not merge independent emitters. Publish a release
matrix by model, firmware, protocol, transport, band and tested operating state.

Acceptance: measured acquisition/reacquisition and hop coverage, documented false
associations and explicit unsupported cases. No universal DJI hop sequence claim.

## Hardware connections by stage

| Stage | What to connect | Dependencies / limits |
| --- | --- | --- |
| Software benchmark and architecture | Nothing new | Existing IQ fixtures suffice. |
| Mini 2 live benchmark | X310 powered, Ethernet to host at configured 192.168.10.2, correct-band antenna on configured RX2; Mini 2 and paired controller | Confirm receive daughterboard/band and aircraft/controller firmware. Only one program owns the USRP. |
| Heltec bring-up | Exact Heltec board by USB data cable | Identify board/revision and RF band before choosing firmware. No transmission needed for identification. |
| Nucleo/LR2021 bring-up | NUCLEO-L476RG by its ST-LINK USB; LR2021 shield/module via its verified adapter/pinout | Confirm SPI, chip select, BUSY, IRQ, reset, power/logic levels, oscillator and RF switch. Do not infer connections from photo alone. |
| RF generator tests | Verified board/radio, suitable antenna or conducted/shielded RF fixture, X310 receiving | Calculate attenuation against transmitter output and receiver input limits. Do not connect TX directly to RX without suitable attenuation. |
| Independent ELRS validation | Supported genuine ELRS TX/RX pair or independent labeled captures | Version, region/band and mode must match the decoder target. |
| Remote ID validation | ESP32-S3 endpoint plus independent compatible receiver | Verify the particular BLE/Wi-Fi transport; a generic phone/adapter may not support every transport. |

The plan is software-first with an early hardware checkpoint, not building the
entire detector and postponing all RF testing until the end. Board firmware stays
gated on exact hardware identification. No immediate LR2021-to-Nucleo wiring is required.

## Hardware capabilities and evidence

- The standard Heltec WiFi LoRa 32 V3 combines ESP32-S3 and SX1262; its LoRa RF
  path is sub-GHz and variant-dependent. ESP32 Wi-Fi/BLE is a separate 2.4 GHz
  path. A Heltec-style clone must be identified before assuming the same pinout.
  Source: https://heltec.org/project/wifi-lora-32-v3/
- LR2021 supports sub-GHz and 2.4 GHz with LoRa/FSK/FLRC and other PHY modes.
  The actual module's matching, antennas and driver support constrain usable modes.
  It is not a DJI OFDM waveform generator.
  Source: https://www.semtech.com/products/wireless-rf/end-nodes-ics
- Semtech documents NUCLEO-L476RG support with specified radio/expansion platforms;
  this does not verify the user's unidentified shield wiring.
  Sources: https://github.com/Lora-net/usp and https://github.com/Lora-net/usp_zephyr
- NUCLEO-L476RG provides ST-LINK and expansion connectors; it needs an external
  radio for these RF tests. Source: https://www.st.com/en/evaluation-tools/nucleo-l476rg
- ArduRemoteID lists ESP32-S3 support, useful as a reference for standardized
  Remote ID test endpoints. Source: https://github.com/ArduPilot/ArduRemoteID

These sources were checked during the preceding planning discussion on 2026-09-23.
Firmware targets and compatibility must be checked again at implementation time.

## Metrics and pass/fail policy

Record detection probability per defined observation interval, packet recovery
when transmitted packet counts are known, false positives per hour, confusion
between protocol labels, time to first valid result, identity stability, hop
coverage/reacquisition, CPU/memory and latency distributions. Track fixed-channel
and scanning results separately. With passive drone captures and no known TX
count, do not present decoded counts as an absolute packet-recovery percentage.

Use ground-truth generator sequence numbers for controlled tests and independent
captures/receivers for compatibility. Agree numerical performance targets after
the baseline identifies feasible rates and RF conditions; no unmeasured accuracy
or range claim is an acceptance result. Retain failed examples as regressions.

## Open inputs and blockers

1. Exact Heltec model/revision, RF chip and band.
2. LR2021 module/shield part number and its adapter/schematic.
3. Mini 2 and controller firmware and evidence of validated ID versus candidate detection.
4. Availability of a genuine ELRS pair or independent reference captures.
5. Available RF attenuators/cables/shielding and X310 daughterboard details for conducted tests.

These block specific hardware stages, not the offline runner, schemas, reporting,
software profiling or protocol-interface design.

## Decisions and change history

| Date | Decision / evidence | Consequence |
| --- | --- | --- |
| 2026-09-22 | Working detector implemented and validated with published DJI captures and X310 smoke checks | Preserve as baseline; do not claim all DJI models. |
| 2026-09-22 | Stop originally restored earlier 433 MHz settings; changed after user feedback | Detector now retains its tuning and RX settings. |
| 2026-09-22 | Observation list increased and made resizable | Maintain detector UI improvement; Recorder remains unchanged. |
| 2026-09-23 | User reports Mini 2 detection and lists Heltec, Nucleo and LR2021 hardware | Add real Mini 2 benchmark and controlled generator work. |
| 2026-09-23 | Separate waveform tests, protocol conformance and real-device validation | Simulator-only success is not drone support. |
| 2026-09-23 | User requests persistent to-do and detailed progress files | These two documents become the working checklist and progress record. |

Dates summarize conversation sessions, not independently recovered build timestamps.

## Work log

### 2026-09-23 — P00: documentation setup

- Status: complete.
- Added `docs/DETECTOR_TODO.md` and this detailed plan/progress log.
- Consolidated the earlier roadmap, hardware test strategy, completed implementation,
  known performance limits, dependencies, evidence and acceptance criteria.
- Read existing `docs/DETECTOR.md` to reconcile current behavior.
- No application code, board firmware, radio configuration or Recorder files changed.
- Validation: document readback and task-reference check; no code tests required for documentation-only work.
- Next proposed implementation: S01/S02 capture manifests and batch regression runner;
  follow with the H01/H02 Mini 2 checkpoint rather than waiting until all features exist.

### Template for subsequent sessions

For every meaningful session append: date; task IDs; planned/in-progress/blocked/
complete status; files changed; design decisions and reason; exact verification
commands and results; artifact locations; known limitations; hardware and firmware
used; blockers; next step. Update the checklist and current-status table together.
Keep previous failures and measurements; append corrections instead of silently
rewriting history. Never mark a hardware validation complete from a synthetic test.


### 2026-09-23 — S01/S02: capture manifests and replay benchmark

- Status: S01/S02 complete; S03 partially prepared, independent field negatives and held-out sessions still pending.
- Added `tests/data/captures.json`: two hash-pinned real DJI development captures and ten deterministic synthetic negative/control cases. Unknown original firmware, gain and RF metadata remain explicitly unknown. Replay center is not claimed as original RF center.
- Added `src/detector/python/benchmark.py`: schema/metadata validation, session split checks, input integrity, bounded replay using the live Analyzer, explicit expected results, separate failure/error outcomes, source/library/environment hashes, and JSON/Markdown reports. Prior report directories cannot be overwritten.
- Added `tests/test_benchmark.py` and CTest registration: six checks covering corrupt evidence, incomplete IQ samples, invalid metadata, session leakage, sequential chunk timing, deterministic generation and missing expected labels.
- Added `docs/DETECTOR_BENCHMARK.md` for commands, manifest contract and interpretation.
- Baseline artifacts: `reports/benchmark-2026-09-23-baseline/report.json` and `report.md`.
- Baseline result: all 12 cases passed. Mini 2: eight confirmed packets; Mavic Air 2: one. Nine synthetic noise/tone/silence cases and generic CSS produced zero confirmed aircraft packets. Generic CSS remained a candidate.
- Performance finding: dense DJI fixtures required about 604 ms (Mini 2) and 180 ms (Air 2), approximately 41.5x and 39.9x their IQ durations. Quiet 25 MS/s noise took about 11 ms per 20 ms window; a 25 MS/s tone took about 43 ms. Correct decoding is not equivalent to real-time throughput.
- Validation command: `cmake -S . -B build && cmake --build build -j4 && ctest --test-dir build --output-on-failure`.
- Verification result: all five CTest suites passed (4.87 seconds). Build succeeded.
- No RF hardware was opened, board firmware changed or Recorder source edited. The previous /tmp checksum inventory was unavailable in this session, so a fresh comparison against that inventory could not be performed.
- Limitation: no held-out captures yet; no measured field false-positive rate or packet recovery percentage. No new real-model coverage claimed.
- Next: S03 real-negative/held-out dataset and H01/H02 early Mini 2 checkpoint; software work can continue with S04 loss/latency instrumentation while hardware metadata is gathered.

## Support matrix after S01/S02

| Target / mode | Firmware | Evidence | Current claim |
| --- | --- | --- | --- |
| DJI Mini 2 DroneID, published IQ at 50 MS/s | Unknown | Existing development capture; eight valid packets | Decodes this capture; user's live report not yet archived as benchmark |
| DJI Mavic Air 2 DroneID, published IQ at 50 MS/s | Unknown | Existing development capture; one valid packet | Decodes this capture |
| Generic CSS, synthetic 500 kHz SF7 at 2 MS/s | Not applicable | Known generated chirps | Candidate only; not ELRS protocol validation |
| Noise/CW/silence at 2, 4, 25 MS/s | Not applicable | Deterministic synthetic controls | No confirmed drone under these test cases |
| ELRS/FrSky/FlySky packets | Unselected | No independent packet fixtures | Unsupported packet-level classification |
| Additional DJI models / legacy / O3/O4 | Unknown | No model-specific independent fixtures | Unvalidated |
| Standard BLE/Wi-Fi Remote ID | Unselected | No receiver backend yet | Planned |

Future sessions must extend this matrix by actual model, firmware, transport and operating state, not by modulation resemblance.


### 2026-09-23 — DroneDetect_V2 integration started (DD01–DD03)

- User approved dataset work and continuing Markdown updates.
- Dataset source: `/home/sudeep/Pictures/DroneDetect_V2`; read-only external input, not copied into the project.
- Initial inventory: 390 DAT files, approximately 349 GiB; CLEAN/BLUE have 95 files each, WIFI/BOTH 100 each. No local readme or license accompanies the files.
- Plan: finish source/format audit, add exact-window integrity and exploratory (unscored) replay, run a bounded 28-recording ON pilot, then choose channelization improvements from observed evidence.
- Full dataset training, claimed protocol labels, and treating interference groups as drone-free negatives are out of scope for this initial audit.
- Initial byte probes support little-endian interleaved float32: plausible finite quantized amplitudes; opposite-endian interpretation produces tiny subnormal numbers. Sample rate and RF center still require source evidence; filename labels do not encode these.
- Status: in progress. No RF hardware connected and no Recorder changes planned.


### 2026-09-23 — DD02/DD03 complete; DD01 source verification pending

- Implemented `src/detector/python/dronedetect.py`: strict filename/folder labels, file-size inventory, three bounded quality probes per file, coverage gaps, deterministic ON repeat-00 pilot and exact-window hashes. Raw dataset remains external and read-only.
- Extended `benchmark.py` with bounded memmaps, selected-byte SHA256, source-size checks, source-relative timestamps, source-path split checks and explicit unscored exploratory observations. Known-packet regression behavior is preserved. Window hashes do not attest whole-file integrity.
- Dataset inventory: 390 recordings, 374,203,574,280 bytes; CLEAN 95, BLUE 95, WIFI 100, BOTH 100. All filenames passed consistency checks. Missing combinations: DIS_HO in all conditions and PHA_FY in CLEAN/BLUE. These are coverage gaps, not proof of a corrupt download.
- Two files are shorter than 1.99 s at the provisional rate: `BOTH/INS_FY/INS_1110_00.dat` (873,922,520 bytes) and `CLEAN/INS_FY/INS_0010_00.dat` (842,727,408 bytes). Duration differences alone do not prove corruption.
- Format probes: 1,170 bounded probes, all finite under little-endian complex-float32 interpretation. Median DC power fraction 0.9656; 893 probes exceed 0.5. This is a measured preprocessing concern, not evidence that the entire recordings contain only DC. No DC removal has yet been applied.
- Metadata limitation: local files contain no README/acquisition metadata/license. IEEE source access was unavailable during verification. CLI values 60 MS/s and 2.4375 GHz are explicitly provisional in artifacts; source/model/protocol claims are not inferred from folder names. Redistribution/license and true acquisition-session grouping remain unresolved.
- Pilot: 28 recordings (seven aircraft codes × four conditions), three independent 20 ms windows per recording at 0.25/0.9/1.5 s. Total 84 windows, 1.68 seconds of assumed-rate IQ, 806.4 MB selected raw data. Decoder state resets between separated windows.
- Pilot outcome: all 84 windows completed as `observed`, zero processing errors, zero validated DJI IDs and zero DJI packet candidates. Event labels: 60 Unknown RF, nine Wi-Fi-like OFDM, four DJI-like OFDM. These are waveform observations, not identified aircraft or protocol validation. Some windows produced no event.
- Timing: 4.8125 s aggregate Analyzer wall time for 1.68 s IQ (~2.86×). Excludes IO/startup and was collected alongside other verification work; not a controlled throughput benchmark.
- No negative/positive scoring was assigned to the dataset. Short ON-only windows, unknown packet presence, provisional metadata, front-end bandwidth, DC offset and channel alignment prevent interpreting absent IDs as missed drones.
- Artifacts: `reports/dronedetect-2026-09-23-audit/{audit.json,audit.md,pilot.json}` and `reports/dronedetect-2026-09-23-pilot/{report.json,report.md}`. Instructions in `docs/DETECTOR_BENCHMARK.md`.
- Validation: five new dataset tests cover label mismatches, exact-window bytes/timestamps, mutated data, changed source size, truncated windows, split leakage, DC and nonfinite probes. Build succeeded; all six CTest suites passed (5.79 s). All 12 existing benchmark cases passed in `reports/benchmark-2026-09-23-dataset-regression/`; Mini 2 eight valid packets, Air 2 one.
- No Recorder UI/logic, live detector algorithm, board firmware or RF configuration changed in this milestone. No hardware needed or opened.
- Next DD04: verify acquisition metadata; implement explicit, optional DC handling and frequency translation/anti-alias filtering/resampling with injected-tone and known-DJI regressions. Compare matched raw/processed windows before changing live preprocessing. Do not invent protocol labels to train against.
- Later DD05/DD06: expand modes/repeats, establish independent sessions, add genuine drone-free negatives, then evaluate a lightweight model with unknown rejection against independent X310 captures. Hardware checkpoint remains X310 + Mini 2/controller; Heltec/Nucleo/LR2021 are not required for dataset work.

### 2026-09-23 — Requested GUI sample-rate option

- Added `Custom sample rate` and an MS/s input to Drone ID Detector → Receiver. Enabling it starts from the current profile rate; disabling it restores automatic profile selection.
- Range: 0.1–100 MS/s, matching the analyzer input range. Hardware may coerce the request; the existing Actual readout displays the receiver rate. Changes apply on Connect or Start, and controls are disabled during detection/recording.
- Both detector connection and start now use the requested override. Invalid requests are rejected; the existing actual-rate minimum for wideband/DJI profiles remains enforced, with an inline message for custom requests below 15.26 MS/s.
- Validation: build succeeded; all six CTest suites passed. Controller checks cover custom rates on connect/start, rejection of a too-low DJI rate and rejection of an out-of-range request. No physical receiver-rate test or interactive GUI inspection performed.
- Updated only detector UI/controller, detector fields in shared AppState, controller tests and tracking documents. Recorder tab and recording logic were not edited.


## Updated broader-detection roadmap — 2026-09-23

This is the current execution order and supersedes the earlier phase ordering.
This update changes plans only. Recorder behavior remains out of scope.

### Intended scope and honest result semantics

Target a multi-protocol, multi-band RF detector with explicit tested coverage,
not a promise to detect every drone. Receive-only RF cannot detect an aircraft
that emits no observable RF in the monitored bands. If eventual requirements
include those aircraft, independent optical/thermal/acoustic/radar sensing is a
separate future project; it is not solved by training on more RF recordings.

Maintain separate fields for RF presence, modulation hypothesis, probable link
family, validated protocol and claimed broadcast identity. CRC validates packet
integrity, not authenticity of an aircraft's identity. A control protocol can
be used by non-aircraft equipment; packet classification alone does not prove
a drone. Uncertainty must remain visible. Count signals/links separately from
aircraft; associate links only using supporting identity evidence.

### 1. Reliable signal intake and measurements (DD01/DD04, S04)

- Verify dataset acquisition rate, center, front-end bandwidth and format from
  authoritative metadata; leave assumptions visible until verified.
- Add optional DC correction, frequency translation, anti-alias filtering and
  resampling. Test amplitude/frequency/timestamp handling, edge effects and
  decoder continuity. Compare identical raw/processed windows; do not blindly
  remove center-frequency signal content or normalize away useful evidence.
- Channelize captured bandwidth into analysis windows suitable for each module.
  Channelization cannot recover spectrum absent from the original recording.
- Record receiver discontinuities, queue drops/age, tune settling, scan blind
  time and per-stage latency. Honor actual hardware sample rates; expose which
  decoders are eligible at the requested/actual rate.
- Acceptance: existing known DJI packets remain decodable; injected in-band and
  out-of-band fixtures verify filtering, and gaps reset partial decoding state.
  Re-run the dataset pilot with a raw/processed comparison and preserve errors.

### 2. Early independent field evidence (H01–H04, S03/DD05)

Connect X310 and the real Mini 2/controller after software preprocessing tests.
Capture drone/controller off, controller-only, aircraft-only where practical,
paired idle, active video and controlled operation. Include ordinary Wi-Fi/BLE,
non-drone LoRa/FSK and quiet background. Record actual configuration and firmware.
Split by independent sessions/day/location/device where available, not adjacent
windows or repeat number. Unknown DroneDetect sessions remain development data.
Acceptance: freeze an independent positive/negative evaluation set before tuning
thresholds; distinguish ID-packet truth from aircraft-present truth.

### 3. General discovery architecture and GUI (S06/S07, U01/U03)

Introduce a receiver capability description and protocol modules declaring bands,
rate/bandwidth requirements, detection cost, packet validators and result fields.
Add Discover mode that schedules compatible modules and band profiles, alongside
the existing targeted mode. Stage processing: inexpensive activity/burst checks,
waveform features, then relevant packet decoders. Import/export validated profiles.
Show signal/link tracks, evidence state, candidate family, last seen, occupancy,
actual frequency/rate, coverage and loss; show claimed IDs in a separate view.
Acceptance: unsupported rates/bands/decoders cannot silently appear covered;
multiple emitters cannot inflate a confirmed-aircraft counter from waveform rows.

### 4. Broader identity coverage (R01–R03, D04)

Prioritize a separate standardized Remote ID receiver backend, initially one
verified BLE or Wi-Fi transport, followed by additional transports. This adds
cross-manufacturer identity coverage when broadcasts are present. Use established
message definitions and independent receiver fixtures; deduplicate messages and
preserve source, timestamps and missing fields. Continue DJI expansion by actual
model/firmware captures; O3/O4/legacy remain unvalidated until demonstrated.
Acceptance: independently captured valid and malformed messages, expiry/dedup
checks, and no promotion of a broadcast assertion into authenticated identity.
Reference: https://www.faa.gov/uas/getting_started/remote_id and
https://www.faa.gov/sites/faa.gov/files/uas/getting_started/remote_id/industry/Remote_ID_Standard.pdf
(technical broadcast context, not an assertion about local legal requirements).

### 5. DIY protocol support and controlled bench (S08, G*, D01–D03/D05)

Select one ELRS version/band/packet mode using available genuine hardware or
independent captures. Implement packet synchronization/framing/integrity checks,
then add modes and FrSky/FlySky separately. ELRS includes 2.4 GHz and 900 MHz
hardware; Gemini can require multi-channel or cross-band observation, so one
single-channel decoder does not establish all ELRS support.
References: https://www.expresslrs.org/hardware/hardware-selection/ and
https://www.expresslrs.org/software/gemini/.
Identify the exact Heltec variant and LR2021 shield before firmware/wiring work.
Use Nucleo/LR2021 for supported known-payload waveform/timing experiments, not
assumed DJI imitation. Independent real transmitter evidence remains required.
Acceptance: genuine packet captures plus malformed/non-drone negatives; generated
PHY resemblance alone cannot validate a protocol or aircraft model.

### 6. Unknown-aware classifier and throughput (DD06, S05)

Use DroneDetect for feature exploration and interference stress tests after the
metadata/preprocessing work. Expand beyond ON/repeat-00 windows. Start with a
small interpretable feature baseline (normalized spectral shape, bandwidth,
burst timing, cyclic-prefix/chirp features) before deep learning. Compare against
simple power-only baselines to expose gain/session confounds. Require unknown
rejection and independent X310 evaluation; do not force every signal into one
of the seven aircraft labels. A model-family prediction remains a hypothesis.
Profile actual bottlenecks, then optimize DSP/parallelism and bounded queues.
Acceptance: report per-class confusion, unknown false acceptance and performance
on independent sessions; preserve packet results and measure queue loss/latency.

### 7. Coverage, hopping and release gates (A*, V01, U02)

Scan only hardware-supported bands with suitable antennas; prioritize 2.4/5.8 GHz
and relevant regional sub-GHz DIY profiles. Confirm daughterboard/host bandwidth
before choosing simultaneous channels or another receiver. Sample rate alone
does not provide multi-band coverage. Optimize dwell using measured occupancy;
keep actual coverage/blind time visible. Test hop timing/reacquisition for known
supported protocols; do not infer a universal DJI hopping sequence.
Release reports must include detection probability by tested condition, false
alerts/hour on real negatives, time-to-detect distribution, validated-ID accuracy,
unknown rejection, gap/queue loss and multi-emitter false associations. Define
operating conditions and acceptable thresholds before tuning; do not invent a
universal accuracy or range figure. RF range requires measured field conditions.

### Immediate next implementation milestone

DD04 plus S04: tested preprocessing/channelizer and loss instrumentation, raw versus
processed pilot report, then the early real Mini 2 checkpoint. No new boards are
required for the software milestone. Remote ID and independent negatives move
ahead of speculative large-model training. Hardware identification can proceed
when needed; no immediate LR2021-to-Nucleo wiring is requested.


### 2026-09-23 — DD04/S04 software implementation and matched comparison

- User authorized preprocessing, reception-health metrics and matched replay. No new board connection or RF transmission occurred.
- Added `preprocessing.py`: optional streaming 1 kHz DC blocker initialized from a buffered 4096-sample mean; stateful frequency translation, Kaiser FIR and integer-ratio downsampling. Rejects invalid/out-of-capture bands and unsupported rate ratios. FIR startup is omitted and output timestamps compensate group delay; DC phase is not timestamp-corrected. No amplitude normalization. Configuration/epoch/timestamp gaps reset state.
- Replay flags: `--dc-block`, `--shift-hz`, `--output-rate-hz`, `--bandwidth-hz`. Reports retain raw evidence hashes and distinguish processed sample counts and preprocessing cost. Channelizer is offline/experimental, not an automatic multi-channel live backend.
- GUI Receiver section: optional `DC correction (experimental)`, default off, applied on Start. Live defaults remain unchanged. Python subprocess receives this setting through an explicit argument.
- Health UI: UHD overflow/timeout/other-error event counts, queue-dropped samples, continuity breaks, partial-batch/stop discards, settling-skipped samples and their nominal duration, queue depth/oldest age, last batch wait, batch age at result, preprocessing cost and decoder errors. Exact lost RF samples are unknown from UHD event counts. Queue age includes batching delay. Tune-away time is estimated by UI-tick sampling for the current target and excludes settling/queue loss; it is not exact packet coverage.
- Receiver changes add detector telemetry only around existing metadata handling/settling. Existing Recorder ring behavior, Recorder tab and recording logic were not edited. Worker now propagates timestamp-only discontinuities and caps IPC batches at two million samples, avoiding over-limit messages near 100 MS/s.
- Source verification advanced: the author's thesis Figure 19/acquisition text corroborates a 60M sample setting, 2.4375 GHz center and interleaved float IQ; 120 million complex samples over two seconds supports 60 MS/s despite Mbits/s prose. About 28 MHz bandwidth is described, while the flowgraph control says 30M. This is dataset-family evidence, not verification of each local V2 file. License, archive provenance, independent sessions and per-file metadata remain unresolved. Source: https://repository.essex.ac.uk/34531/1/1900395_SWINNEY_Thesis_Submission.pdf . Historical artifacts are not rewritten.
- Preserved failure: initial DC warmup-discard strategy reduced Mini 2 from eight to seven packets (`reports/benchmark-2026-09-23-dc`). Replaced with buffered initialization retaining initial samples; chunk-size invariance and packet tests now pass.
- Raw and DC regressions: all 12 cases pass in each mode, eight Mini 2 and one Mavic Air 2 validated packets. No new model support is claimed.
- Downsampling regression exposed off-center fixture signals: unshifted 50→25 MS/s tests failed. Air 2 aligned at -12.5 MHz recovers its one packet; Mini 2 aligned at +9.6 MHz recovers four of eight. A polyphase packet-resampling experiment also recovered four and was reverted. All failed runs remain in the comparison report. Mini 2 reduced-rate compatibility remains an open acceptance failure; it is not hidden by lowering expected counts.
- Matched dataset pilot, 84 identical hashed windows / 28 recordings / 1.68 s input: raw 0 validated IDs, 0 packet candidates, 4.682 s processing; DC 0 IDs, 2 packet candidates, 7.821 s; DC + channelization 0 IDs, 3 packet candidates, 9.059 s. Channelized configuration: -3 MHz shift, 20 MS/s output, 16 MHz passband (2434.5 MHz assumed center). More candidates do not establish increased accuracy. Processing includes preprocessing but excludes IO/startup; early runs overlapped verification activity and are not controlled throughput measurements.
- Artifacts: `reports/preprocessing-2026-09-23/comparison.md` and `.json`, individual reports/manifests/logs beneath that directory. Usage details in `docs/DETECTOR_BENCHMARK.md`.
- Validation: project builds; seven CTest suites pass (8.17 s). New tests cover DC rejection, in-band preservation/out-of-band alias suppression, mixing direction, streaming chunk equivalence, output timing, gap/retune/epoch reset, invalid configurations, known channelized Air 2 packet, DC subprocess decoding, continuity discards, receiver-counter deltas and 100 MS/s batch bounds. No interactive GUI or physical RF test was performed.
- Status: DD04 partially complete, pending reduced-rate Mini 2 acceptance; S04 implemented in software, pending live counter/coverage validation. DD01 corroborated but not fully closed. No classifier training.
- Next: diagnose Mini 2 channelized packet failures before enabling live channelization. The independent Mini 2 checkpoint can now use raw/DC modes: connect X310 and Mini 2/controller, archive firmware/settings and collect labeled positive plus drone-free sessions. Heltec/Nucleo/LR2021 are still not needed for this checkpoint.
