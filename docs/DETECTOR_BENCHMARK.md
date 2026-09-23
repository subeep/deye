# Offline detector benchmark

Run from the project root after building:

```sh
python3 src/detector/python/benchmark.py --manifest tests/data/captures.json --turbo build/libdrone_turbo.so --output reports/my-new-run
```

The output directory must not already exist; previous evidence is preserved.
`report.json` includes input/source/library hashes, environment, metadata, events,
expectations, failures and timing. `report.md` is the readable comparison.
Exit codes: 0 all expectations pass; 1 capture failure/error; 2 invalid configuration.
No RF hardware is opened. The live Analyzer processes sequential chunks (default
20 ms, capped at its maximum size), with a fresh instance per independent capture.
Use `--chunk-ms` to test boundaries and `--split held_out` for independent sessions.
Empty selections fail rather than reporting success.

## Manifest v1

See `tests/data/captures.json`. Each entry declares a unique ID, session, split,
real/synthetic kind, cf32_le format, sample rate, replay center and provenance.
File paths are relative to the manifest and require SHA256 integrity checks.
Record firmware, model, antenna, gain, operating state and capture date in metadata;
unknown fields stay null. A chosen replay center is not evidence of the original
RF frequency. Files must contain complete 8-byte interleaved complex samples.
One session cannot cross development and held-out splits. Merely renaming chunks
cannot establish independence; curate truly separate sessions.

Expectations declare confirmed count bounds, required/allowed protocol labels,
and confirmed models. Generic CSS is allowed as a waveform candidate, never as
confirmed ELRS. Existing published DJI captures are development evidence, not
held-out data. Synthetic noise, tones and silence do not replace real Wi-Fi/BLE
or non-drone LoRa traffic. Source attribution/license is included in the manifest.

## Metrics and limits

Timing measures wall time around Analyzer.analyze, excluding startup, IPC, I/O and
radio losses. Analysis/IQ duration above 1 indicates slower-than-recorded-time
processing. Burst-dense short fixtures are more demanding than quiet RF windows.
First-valid capture time means the end of the block yielding a valid result,
not exact packet arrival. Candidate counts may include overlap retries.
Packet recovery percentage is null because transmitted totals are unknown.
Results do not establish live range, scan coverage or universal classification accuracy.

Next checkpoint: independent Mini 2 sessions and real negative traffic using the
X310, with recorded firmware/settings and operating states. Heltec/Nucleo/LR2021
are not required for that checkpoint. See DETECTOR_PROGRESS.md for hardware needs.

## DroneDetect_V2 external dataset

The importer audits the external dataset read-only and creates a small pilot manifest;
it does not copy raw IQ into the repository. Run from the project root, choosing new
output directory names for subsequent runs:

```sh
python3 src/detector/python/dronedetect.py \
  --root /home/sudeep/Pictures/DroneDetect_V2 \
  --output reports/dronedetect-2026-09-23-audit \
  --sample-rate-hz 60000000 --center-frequency-hz 2437500000
python3 src/detector/python/benchmark.py \
  --manifest reports/dronedetect-2026-09-23-audit/pilot.json \
  --turbo build/libdrone_turbo.so \
  --output reports/dronedetect-2026-09-23-pilot
```

The rate and center above are **provisional acquisition assumptions**. They are
explicit CLI arguments and remain labeled as assumptions in artifacts. Local DAT
files have no embedded metadata establishing them. Authoritative V2 acquisition
metadata and license still need confirmation from the dataset distribution.
Little-endian float32 IQ is plausible from bounded byte probes; this is not an
embedded format declaration. Aircraft codes are kept as codes, avoiding guessed
model or protocol mappings.

`audit.json` inventories sizes, filename/directory consistency, coverage gaps and
three small quality probes per file. Probes measure finite fraction, mean I/Q,
RMS, AC RMS, peak component and DC power fraction. They do not establish quality
of every sample. `audit.md` summarizes the inventory.

`pilot.json` selects repeat 00, ON mode, all available aircraft/condition groups,
with 20 ms windows starting at 0.25, 0.9 and 1.5 seconds. Each window starts a
fresh Analyzer: separated windows cannot establish hopping continuity. At the
assumed 60 MS/s, 84 windows represent 1.68 seconds and 806,400,000 raw bytes.
This is a plumbing/compatibility pilot, not representative temporal coverage.

Window manifests use `sha256_scope: "window"`, `sample_offset`, `sample_count`,
`source_size_bytes`, and SHA256 of exactly those raw bytes. Replay checks source
size and window integrity and maps only the selected interval. It does not hash
or attest the remaining file contents. Timestamps retain the source sample offset.
Existing whole-file fixtures retain their original hashing behavior.

`evaluation: "exploratory"` requires absent/null expectations. Successful replay
is `observed`, with overall `exploratory`, never a scored pass. Processing errors
still fail the run. Exit zero means the observations completed, not that a drone
was detected. All DroneDetect windows are development data with one conservative
unknown-session group. Neither repeat numbers nor files establish independent
acquisition sessions. A source path or session cannot cross development/held-out
splits; copied/renamed source files still require curator review for leakage.

WIFI, BLUE and BOTH contain interference alongside drone recordings; they are
not real drone-free negatives. A waveform candidate is not a validated protocol,
aircraft count, or model identification. No classifier is trained by these tools.

## Optional preprocessing (2026-09-23)

Replay accepts `--dc-block`, `--shift-hz`, `--output-rate-hz` and
`--bandwidth-hz`. For example, append these options to the pilot command, using
a new output directory:

```sh
--dc-block --shift-hz -3000000 --output-rate-hz 20000000 --bandwidth-hz 16000000
```

A positive shift selects an RF channel above the input center; the mixer moves
that channel to baseband and the reported center becomes input center + shift.
This first channelizer supports integer downsampling only (e.g. 60→20 or 50→25
MS/s). It rejects noninteger ratios, upsampling, invalid bands and excessively
narrow filter transitions. Default passband width is 80% of output sample rate.
A causal Kaiser FIR targets 60 dB stopband attenuation. FIR startup is omitted;
output timestamps compensate its group delay. The input sample count/hash remains
that of the raw evidence; output sample count and filter metadata are separate.

The optional 1 kHz DC blocker buffers its first 4096 samples to initialize from
their mean, then retains streaming state. It does not normalize signal power.
Its phase response is frequency-dependent and is not timestamp-corrected.
Rate/frequency/epoch changes and timestamp discontinuities reset filter, mixer
and initialization state. Incomplete initialization at an input end/gap produces
no output for those buffered samples. This is not an automatic gain correction.

**DC correction passes the existing packet regressions. Channelization remains
experimental and offline-only:** the aligned Air 2 test retains its packet, but
the aligned 25 MS/s Mini 2 test currently recovers four of eight. Do not reduce
rate without selecting a channel or assume that frequency translation alone
establishes decoder compatibility. No live automatic channel selection is added.

Matched reports and failures: `reports/preprocessing-2026-09-23/comparison.md`.
Author metadata found after the earlier audit corroborates dataset-family settings:
[thesis, Figure 19 and acquisition description](https://repository.essex.ac.uk/34531/1/1900395_SWINNEY_Thesis_Submission.pdf).
Exact local V2 per-file settings, archive provenance, license and independent
session grouping remain unverified. Historical reports retain their original
provisional metadata rather than being retroactively rewritten.
