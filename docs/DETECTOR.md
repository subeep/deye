# Drone detector

The Drone ID Detector tab now consumes live complex IQ directly from the USRP
receive thread. It uses a separate bounded queue and Python DSP process; the
Recorder tab, recorder implementation, plots, and recording format are unchanged.
The radio is shared: detection cannot start during recording. Starting recording
stops detection. Stopping detection retains the detector's current frequency,
gain and sample rate; it does not restore the Recorder's initial 433 MHz settings.
Connecting from the detector applies its selected profile before opening the radio.
The observations list defaults to 420 pixels high; drag its bottom divider to
extend or shrink it (double-click to reset). Column headers stay visible while scrolling.

## Supported behavior

| Target | Implementation | Identity / certainty |
| --- | --- | --- |
| DJI DroneID | Burst extraction, carrier/timing correction, OFDM/QPSK, Gold descrambling, LTE rate recovery and turbo decoding, CRC24A and CRC16, payload parsing | Validated broadcast serial and model; verified on published Mini 2 and Mavic Air 2 IQ |
| LoRa CSS / ExpressLRS candidate | Repeated dechirped preamble detection, SF5–9, 500/812.5/1625 kHz bandwidth | Modulation candidate only; other LoRa radios can match |
| DIY FSK control/telemetry candidate | Two-state instantaneous-frequency clustering | Does not distinguish FrSky, FlySky, FLRC or other FSK protocols |
| Wi-Fi-like / DJI-like OFDM | Repeated cyclic-prefix timing and correlation | Waveform candidate, not manufacturer or aircraft confirmation |
| Analog FPV video | FM demodulation and PAL/NTSC-like horizontal-sync periodicity | Video candidate, not an aircraft ID |

Each validated DJI serial has its own row. Candidate observations are grouped by
waveform and receive center frequency. They are **not** counted as drones or
assigned to a DJI aircraft. No claim is made that a video and control signal
belong to the same drone. Broadcast identifiers are reported as received, not
authenticated identities; recordings can contain spoofed identifiers.

DJI O3/O4, ELRS packet CRC/UID decoding, FrSky/FlySky packet parsing, Bluetooth/Wi-Fi
Remote ID, and synchronized on-air hop following are not implemented or validated.
Legacy 8-symbol DJI demodulation is attempted, but no legacy-model capture was
available to validate it. Other models in the payload model table are not a
tested-support list. Coordinates are broadcast values; zero pairs display as
unavailable. dBFS is sample power, not calibrated antenna RSSI.

## Acquisition / hopping modes

- **Fixed channel:** inspect one channel or a custom receive center.
- **Scan channels:** cycle the selected band's channel list at 100–3000 ms dwell.
- **Scan / hold on valid DJI ID:** extend dwell after a CRC-valid DJI packet on
  the current channel, then resume scanning.
- **ELRS reconstructed order:** recreate the firmware FHSS permutation from the
  supplied 32-bit FHSS seed, then scan that order at the configured slow dwell.
  The plotted sequence is the reconstruction, not a measured transmitter trace.

ELRS profiles include 2.4 GHz, FCC915, EU868, and IN866. The seed is the firmware
FHSS seed, not a binding phrase. The reference firmware derives it from UID bytes
and an OTA version value; automatic phrase/UID conversion is deliberately not
assumed across versions. RF register rounding can introduce small frequency
differences from the nominal plotted channel centers.

Matching a permutation is not time synchronization: the transmitter's nonce,
sequence position, packet rate and hop interval are still needed for real hop
following. The USRP software scanner does not reproduce fast ELRS packet timing.
DJI's channel lists are documented acquisition frequencies, not a universal
OcuSync hopping schedule. A single receiver observes only its instantaneous
bandwidth and can miss bursts on other channels. All operation is receive-only.

## Build and use

```sh
cmake -S . -B build
cmake --build build -j4
ctest --test-dir build --output-on-failure
./run.sh
```

Open **Drone ID Detector**, connect the X310 if needed, select a profile/mode,
gain and channel, then start. The detector has its own connect button. The
configured USRP address remains `192.168.10.2`. DJI profiles request 25 MS/s to
accommodate the X310's 200 MHz clock; the actual UHD rate is passed to the DSP.
DIY profiles request 2 or 4 MS/s. Tuning changes discard 50 ms of received samples
and reset partial packets. Stream errors and queue losses also reset continuity.

The Python interpreter selected by CMake needs `src/detector/python/requirements.txt`.
Dependencies are already available in the tested environment. The turbo library
is built locally from vendored source; x86 SSSE3 is required. Paths to the Python
worker and turbo library are embedded at configuration time: re-run CMake after
moving this source tree. No dependency downloads happen at runtime.

Offline use goes through the same analyzer as live reception:

```sh
python3 src/detector/python/backend.py --turbo build/libdrone_turbo.so \
  --input tests/data/dji_mavic_air_2.fc32 --rate 50000000 --frequency 2444500000
```

Input is little-endian complex float32 IQ (interleaved I/Q floats). Output is JSON
with validated packets, waveform observations, candidate/rejection counts, and
processing time. Offline inputs are read in bounded chunks.

`build/detector_live_check NEW_REPORT.json` is an optional, manual receive-only
hardware check using seven 2.4 GHz profile centers. `--fixed` selects 2444.5 MHz;
`--rate 20000000` selects 20 MS/s (default 25 MS/s). Do not run a second receiver
while the application owns the USRP. It is not part of CTest. See
DETECTOR_BENCHMARK.md for bounded IQ capture and report interpretation.

The GUI reports analyzed samples, detector queue drops and batch processing time.
The reference-derived Python demodulator may run slower than real time, especially
with many candidate bursts. A 256-chunk queue bounds memory; overload is reported
and partial packets are discarded across losses. This is not guaranteed continuous
coverage or a guaranteed-range detector.

## Source provenance and licensing

- `src/detector/python/reference/` and both real IQ fixtures come from
  [RUB-SysSec/DroneSecurity](https://github.com/RUB-SysSec/DroneSecurity), commit
  `9ff819843bee48fb140a0704ec78aff757896dea`, **AGPL-3.0**. The license is retained
  in that directory. Local changes fix removed NumPy type aliases, per-instance
  payload state, legacy symbol indexing, vectorize timing searches, and share a
  non-overlapping coarse STFT between modern/legacy burst searches. The
  reference's hard-bit-only turbo bypass is not used by the production analyzer;
  the analyzer uses the actual turbo decoder and CRC24A.
- `vendor/turbofec/` comes from [ttsou/turbofec](https://github.com/ttsou/turbofec),
  commit `6de1f4604933d6c21a0ff0c75401cffa7debf3cd`, **GPL-3.0-or-later**.
  License and authors are retained. SSSE3 is explicitly enabled: the upstream
  scalar fallback is a stub and must not be used for this decoder.
- DJI FEC parameters were cross-checked with
  [proto17/dji_droneid](https://github.com/proto17/dji_droneid), `cpp/remove_turbo.cc`.
- The ELRS permutation/domain reference is
  [ExpressLRS](https://github.com/ExpressLRS/ExpressLRS), commit
  `a60b68af521d546fd395674d66e83291d035ae61`, `src/lib/FHSS/`, **GPL-3.0**.
  The golden vector was generated by compiling that upstream sequence builder
  and RNG for seed `0x01020304` and 80 channels.
  A copy of the GPL is in `src/detector/licenses/ExpressLRS-GPL-3.0.txt`.

## Validation in this workspace

- Published Mavic Air 2 capture: one CRC24A/CRC16-valid packet.
- Published Mini 2 capture: eight CRC24A/CRC16-valid packets.
- Synthetic CSS, FSK, OFDM and FM-video examples; noise/tone rejection, invalid
  metadata, frame boundaries, gap resets, deduplication and CRC check vector.
- Native subprocess integration, bounded-queue overload, cancellation/restart,
  radio ownership/recording exclusion and upstream ELRS sequence comparison.
- X310 live receive/retune smoke check at 2 and 25 MS/s: approximately 66.2 million
  samples analyzed and 12.0 million detector-queue samples dropped in the optimized
  run. No valid live DJI packet was observed during that short run. The hardware
  emitted a UDP send-buffer-size warning; no system settings were changed.
- The rebuilt GUI initialized successfully on the desktop. Recorder source and
  Recorder control/plot source checksums remain unchanged.

These copyleft dependencies and adaptations carry distribution/source obligations;
this implementation should not be represented as a permissively licensed bundle.
The original publications are the basis of the reverse-engineered protocol support,
not a guarantee that future DJI/ELRS firmware uses the same on-air formats.


## 2026-09-24 detector updates

- DJI profiles offer an explicit 20 MS/s lower-data-rate button; profile defaults
  remain 25 MS/s. Both rates pass known-capture tests; neither guarantees live
  throughput under arbitrary traffic.
- Scan/hold accepts fresh confirmed observations from the current tuning epoch.
  The countdown is based on acquisition time. Detected MHz and last-seen age are
  distinct from current receiver tuning; old identity rows remain historical.
- Wideband classification skips irrelevant phase clustering. Packet timing uses
  a coarse/refined search with exhaustive fallback and unchanged CRC checks.
- Manual hardware check now requires a new report filename. See
  DETECTOR_BENCHMARK.md for fixed 20/25 MS/s tests and bounded evidence capture.
- Current measured results and limitations supersede older smoke-run numbers:
  reports/throughput-2026-09-24/report.md. Queue loss remains a known limitation.

## DJI packet telemetry

After receiving a CRC-validated DJI packet, expand `Telemetry: <serial> (<model>)`
below the identity table. The inspector shows aircraft/app/home coordinates,
separate height and altitude, raw north/east/up velocity, raw angle, GPS time,
sequence, UUID, and state bits. Every displayed field belongs to the latest
accepted packet for that serial; unavailable new values replace old values.
Packet acquisition age applies to all fields. `STALE` means no accepted packet
for over ten seconds, not a decoded flight state or proof the aircraft left.

Velocity remains raw, GPS time remains an integer, and the existing raw/3.281
height/altitude conversion is explicitly provisional. No terrain AGL, altitude
datum, m/s, derived ground speed or UTC interpretation is asserted. State labels
are tentative annotations from the reference parser, including bit 9 **private
mode disabled**; uninterpreted bits remain visible. Positions show range/zero
checks and tentative unset flags, not authenticated or independently verified
fixes. App/controller coordinates need not be current operator GPS.

`Copy packet JSON` copies the latest decoded packet. `Save packet JSON` creates
a unique file under `reports/telemetry/` relative to the launch directory and
shows its absolute path. Export includes all parser fields and the original
91-byte payload as hex, including CRC16, raw vertical values, and UUID bytes.
It is a packet export, not an IQ recording or a track history. UUID length is
bounded to 20 bytes; malformed text is displayed with replacement characters
while its original bytes remain available. CRC24A/CRC16 acceptance is unchanged.
