# Drone detector — work queue

Updated: 2026-09-24. Detailed plan, evidence and decisions: [DETECTOR_PROGRESS.md](DETECTOR_PROGRESS.md).
Existing implementation and usage: [DETECTOR.md](DETECTOR.md).

Check a task only when its acceptance criteria in the progress file are met.
Hardware-dependent work remains pending until tested; simulator results do not
establish real-drone support. Preserve the Recorder tab and recording format.

## Current priority order — broader drone detection (2026-09-23)

This order supersedes the older section order; existing task IDs remain valid.
Planning update only: no new detector implementation in this update.

1. DD01/DD04 + S04: verify dataset metadata, test preprocessing/channelization and measure reception losses.
2. H01–H04 + S03/DD05: establish early independent Mini 2 positives and real drone-free negatives.
3. S06/S07: common protocol modules, explicit evidence states and multi-profile discovery; preserve targeted mode.
4. R01–R03 + D04: standardized Remote ID receiver and measured DJI expansion.
5. S08/G01–G06 + D01–D03/D05: hardware-grounded DIY packet support, starting with one ELRS mode.
6. DD06 + S05: unknown-aware RF classification and measured performance optimization.
7. A01–A05 + V01: adaptive multi-band scanning, validated link association and release acceptance.

- [ ] U01 — Define and enforce capability-aware discovery plans (band/rate/antenna/backend), including explicit unobserved bands and scan blind time.
- [ ] U02 — Define held-out false-alerts/hour, detection probability, time-to-detect, identity accuracy, unknown rejection and multi-emitter acceptance scenarios before tuning.
- [ ] U03 — Add separate signal/link/aircraft views with evidence-backed association, stale-state handling and provenance; never count candidate rows as drones.

## Completed baseline

- [x] B01 — Independent live detector sample path and bounded processing queue.
- [x] B02 — DJI DroneID demodulation, turbo FEC, CRC24A/CRC16 and identity parsing.
- [x] B03 — Published Mini 2 and Mavic Air 2 capture validation.
- [x] B04 — CSS, FSK, OFDM and analog-video waveform candidates.
- [x] B05 — Fixed/scan/hold modes and seeded ELRS channel-order reconstruction.
- [x] B06 — Automated DSP, worker, hopping and controller checks.
- [x] B07 — X310 receive/retune smoke test; sample loss measured, not eliminated.
- [x] B08 — Stop retains detector tuning; detector connection applies its profile.
- [x] B09 — Taller, resizable observation list with fixed column headers.
- [x] P00 — Separate work queue and detailed plan/progress files.

## Next: build and test without new hardware

- [x] S01 — Capture manifest and protocol support matrix with provenance and expected results.
- [x] S02 — Batch replay runner using the live analyzer; machine-readable and readable reports.
- [ ] S03 — Independent development/held-out capture split; non-drone negative corpus.
- [ ] S04 — Software telemetry implemented/tested: separate RX events, queue loss/age, partial/stop discards, settling skips and processing latency. Tune-away time is UI-sampled; live hardware validation and exact coverage remain pending.
- [ ] S05 — NEXT: profile/reduce live host queue loss (raw ~125M, DC ~225M samples in short scans); preserve raw/DC/channelized packet regressions.
- [ ] S06 — Protocol-module interface and validated, editable/importable/exportable profiles.
- [ ] S07 — Explicit RF/candidate/validated-protocol/identity states, aging and detector session export.
- [ ] S08 — Define USB test-generator commands, acknowledgements and ground-truth log format.

## Early hardware checkpoint: real Mini 2 baseline

- [ ] H01 — Record exact Mini 2/controller firmware, RF setup and whether the observed result included a validated ID.
- [ ] H02 — Collect labeled aircraft/controller off/on and video-active sessions through a detector-only capture facility or existing recorder.
- [ ] H03 — Compare fixed-channel and scan reception across repeated signal-strength/orientation conditions.
- [ ] H04 — Freeze a held-out Mini 2 benchmark and document measured limits.

## Board identification and test generator

- [ ] G01 — Confirm Heltec model/revision, radio chip and RF band; identify LR2021 module/shield and connector pinout.
- [ ] G02 — Confirm Nucleo revision, supported driver target, power, SPI, BUSY/IRQ/reset and RF-switch wiring.
- [ ] G03 — Bring up each board over USB and verify radio identity before transmission.
- [ ] G04 — Fixed-channel LoRa/FSK/FLRC test modes where hardware supports them; numbered packets and logs.
- [ ] G05 — Deterministic hop-order/timing modes; measure actual timing rather than assuming scheduled timing.
- [ ] G06 — Attenuated/shielded RF tests against X310 with documented settings and ground truth.

## Validated protocol expansion

- [ ] D01 — Choose one ExpressLRS version, band and packet mode; establish reference vectors/captures.
- [ ] D02 — Implement synchronization, packet parsing and integrity checks for that mode.
- [ ] D03 — Cross-check with an independent genuine ELRS transmitter/receiver pair or independent captures.
- [ ] D04 — Add DJI model/firmware coverage using real captures; treat O3/O4 and legacy support as unverified until tested.
- [ ] D05 — Add FrSky/FlySky individually when reference hardware/captures are available.
- [ ] R01 — Specify a separate Remote ID receive backend; start with one supported transport.
- [ ] R02 — ESP32-S3 test endpoint using an established Remote ID implementation and clearly labeled test data.
- [ ] R03 — Verify with an independent receiver; then integrate identity/location/time into detector results.

## Scanning, hopping and multi-link tracking

- [ ] A01 — Occupancy-based scan scheduling with measured dwell/retune loss.
- [ ] A02 — Plot observed activity, predicted sequence and actual receiver tuning separately.
- [ ] A03 — Synchronization/reacquisition experiments on known generated sequences.
- [ ] A04 — Supported-protocol hop tracking; document timing/bandwidth limitations.
- [ ] A05 — Aircraft/link association only with identifying evidence; expose uncertainty and avoid duplicate drone counts.
- [ ] V01 — Run release acceptance matrix; publish model/firmware/mode-specific support and limitations.

## Working discipline

- [ ] For each work session: update status, changes, verification, artifacts, blockers and next step in DETECTOR_PROGRESS.md.
- [ ] Keep this checklist synchronized; never mark a hardware task complete from software-only tests.

The last two items are recurring process reminders rather than one-time completion tasks.


## DroneDetect_V2 integration (approved 2026-09-23)

- [ ] DD01 — Local audit complete (390 files); author thesis corroborates dataset-family acquisition settings. Exact local V2 provenance/per-file metadata, license and session details remain unresolved.
- [x] DD02 — Add hash-checked, bounded window replay without copying the dataset; distinguish exploratory results from scored regressions.
- [x] DD03 — Run a 28-recording ON-mode pilot across seven aircraft codes and four interference conditions; preserve errors and timing.
- [x] DD04 — Tested DC correction, experimental offline channelizer and matched pilot complete. Adjacent-carrier correction restores all eight Mini 2 packets at 25 MS/s; known Air 2 preserved. Automatic/live channelizer integration remains future work.
  - [x] Streaming DC initialization/state/gap tests and optional live detector control (default off).
  - [x] Frequency translation, anti-alias FIR and integer downsampling; amplitude/alias/timestamp tests.
  - [x] Matched raw/DC/channelized 84-window pilot; preserve failures and metadata assumptions.
  - [x] Resolve reduced-rate Mini 2 regression: all eight original packets recovered; no expectations relaxed.
  - [x] Run X310 receive-only raw/DC channel scans; tuning and health telemetry exercised. Significant host queue loss remains; later fixed-channel run recorded two validated packets.
- [ ] DD05 — Design leakage-resistant splits; preserve unknown acquisition sessions and gather real drone-free negatives.
- [ ] DD06 — Evaluate lightweight features/classifiers only after the pilot and split audit; test against independent X310 captures.

## Detector sample-rate control (2026-09-23)

- [x] Add optional sample-rate input in the detector Receiver section; preserve profile defaults, apply on Connect/Start, retain actual-rate display and DJI bandwidth checks.


## Revised next steps after user's validated Mini 2 screenshot (2026-09-24)

Initial plan below was implemented and measured on 2026-09-24; see the latest progress log and throughput report.

- [x] N01 — Separate link and decoder throughput: record negotiated NIC speed/MTU/error counters; run a bounded UHD-only baseline when the GUI releases the radio, then equivalent detector runs.
- [x] N02 — Validate 20 MS/s as a lower-load 1 GbE preset against known shifted DJI fixtures and matched fixed-channel Mini 2 sessions; compare actual rate, packet counts, latency and queue loss with 25 MS/s. Do not assume rate alone solves host overload.
- [x] N03 — Profile preprocessing, activity checks, OFDM synchronization/FEC and IPC/copies; optimize measured bottlenecks. Keep queues bounded and preserve continuity and all strict packet regressions.
- [x] N04 — Make scan/hold evidence explicit: current tuned frequency versus last detection, holding countdown, age/last seen, and acquisition-epoch/timestamp-aware hold decisions with stale-result tests.
- [ ] N05 — Initial linked-state and target-off IQ captures complete, hashed and replayed (two seconds each). Repeated independent sessions and held-out data remain pending; no general false-alert rate claimed.

## Rich DJI telemetry (2026-09-24)

- [x] T01 — Preserve full decoded payload and raw bytes; handle UUID length and non-text bytes without rejecting otherwise valid packets.
- [x] T02 — Carry telemetry into C++ and show expandable per-drone details, acquisition age, and conservative field status.
- [x] T03 — Copy/save decoded packet JSON with raw payload; preserve unknown bits and avoid unverified units/datum claims.
- [x] T04 — Parser edge cases and real-capture subprocess telemetry transport pass; GUI builds and hardware-free expanded-panel render smoke passes. Interactive export/live telemetry review remains pending.
- [ ] T05 — Validate velocity scales/signs, height/altitude datum, GPS epoch and state masks against independent firmware-tagged measurements before enabling derived speed/UTC/authoritative flags.
