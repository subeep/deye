# Preprocessing and reception-health milestone

## Matched replay results

| Run | Status | Windows/cases | Valid packets | Packet candidates | Processing s | Preprocessing s |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| baseline | pass | 12 | 9 | 13 | 0.930 | 0.000 |
| dc-regression | pass | 12 | 9 | 13 | 0.936 | 0.049 |
| dji-25msps | fail | 2 | 0 | 2 | 0.185 | 0.042 |
| mini2-centered | fail | 1 | 4 | 10 | 0.598 | 0.052 |
| air2-centered-low | pass | 1 | 1 | 2 | 0.126 | 0.017 |
| air2-centered-high | fail | 1 | 0 | 2 | 0.136 | 0.017 |
| mini2-polyphase | fail | 1 | 4 | 10 | 0.604 | 0.051 |
| pilot-raw | exploratory | 84 | 0 | 0 | 4.682 | 0.000 |
| pilot-dc | exploratory | 84 | 0 | 2 | 7.821 | 1.668 |
| pilot-channel | exploratory | 84 | 0 | 3 | 9.059 | 7.462 |

## Interpretation

- Raw and DC-corrected regressions preserve eight Mini 2 and one Mavic Air 2 validated packets. All 12 cases pass in each mode.
- The three dataset pilots use the same 84 hashed windows (28 recordings, 1.68 s at the assumed rate). They are exploratory: none produced a validated ID. More waveform/packet candidates do not demonstrate better detection accuracy.
- Channelized dataset run: input 60 MS/s at 2437.5 MHz, shift -3 MHz, output 20 MS/s at 2434.5 MHz, 16 MHz passband. One channel is selected; this does not represent full-band or hopping coverage.
- Decimation of known captures around their original center fails because useful signals lie off-center. Aligning the Air 2 channel at -12.5 MHz before 50-to-25 MS/s conversion preserves its one validated packet. Aligning Mini 2 at +9.6 MHz recovers four of eight, so this remains a failed strict regression.
- An experimental polyphase packet resampler also recovered only four Mini 2 packets and was reverted. Its failing report is preserved. The earlier DC startup-discard failure (seven instead of eight Mini 2 packets) is in ../benchmark-2026-09-23-dc; buffering initialization fixed that loss.
- Channelization remains offline/experimental. Live GUI exposes optional DC correction only, default off. No classifier was trained or new drone/model support claimed.
- Processing time includes preprocessing and analysis, excluding IO/startup. These are single local runs; early runs overlapped build/test activity. Do not interpret them as controlled live-throughput results.

## Metadata evidence

The author’s [thesis, Figure 19 and surrounding acquisition description](https://repository.essex.ac.uk/34531/1/1900395_SWINNEY_Thesis_Submission.pdf) supports a 60M sample-rate setting, 2.4375 GHz center and interleaved float IQ; it describes 120 million complex samples in two seconds and about 28 MHz receive bandwidth. The prose uses Mbits/s, but the sample count/time and flowgraph support interpreting the rate as 60 MS/s. Figure bandwidth control and prose differ (30M setting versus about 28 MHz described). These support the dataset-family settings, not per-file verification of this V2 archive. Endianness remains inferred from local byte probes. License, exact archive provenance and independent session grouping remain unresolved.

## Health telemetry and validation

GUI now separates UHD overflow/timeout/other-error events, detector queue drops, partial-batch discards, stop discards and samples skipped by the existing settling gate. Queue depth/oldest age, last-batch wait, result age and preprocessing time are shown. RX error events do not quantify exact missing samples. Tune-away time is a UI-sampled cumulative estimate for the current scan target, excluding settling/loss; it is not packet coverage.

Build and all seven CTest suites passed (8.17 s). Tests include chunk-independent filtering, anti-alias rejection, frequency/timestamp accuracy, state resets, real Air 2 channel decoding, DC-enabled worker decoding, loss counters and maximum-size batches. No live RF or interactive GUI validation was performed. Recorder UI and recording logic were not edited.

Next: fix reduced-rate Mini 2 compatibility before enabling live channelization; validate health counters and collect independent labeled positives/negatives using X310 + Mini 2/controller.
