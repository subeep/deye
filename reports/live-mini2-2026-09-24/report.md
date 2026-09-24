# Mini 2 / X310 live health check — 2026-09-24

User confirmed the radio connected and Mini 2 controller on and linked. Firmware, antenna model, range and RF band selected by the aircraft were not independently verified. This is a receive-only health test, not a packet-recovery or detection-accuracy benchmark.

X310 192.168.10.2, RX2, actual 25 MS/s, 20 dB gain; seven documented 2.4 GHz scan centers, three seconds per channel. Raw and DC runs occurred sequentially with changing ambient RF. Both runs tuned to all targets and completed; radio was disconnected afterward.

| Metric | Raw | DC enabled |
| --- | ---: | ---: |
| Analyzed samples (M) | 369.246 | 276.808 |
| Queue-dropped samples (M) | 124.944 | 224.870 |
| Partial-batch discards (M) | 23.478 | 16.253 |
| Stop discards (M) | 0.401 | 0.156 |
| Receiver overflow events | 0.000 | 0.000 |
| Receiver timeout events | 0.000 | 0.000 |
| Receiver other errors | 0.000 | 0.000 |
| Analysis errors | 0.000 | 0.000 |
| Validated packets | 0.000 | 0.000 |
| Packet candidates | 2.000 | 3.000 |
| Maximum sampled queue age (ms) | 286.116 | 315.698 |
| Maximum sampled result age (ms) | 326.525 | 398.960 |
| Settle skips (s) | 0.351 | 0.351 |

## Findings and limits

- Neither short run returned a validated DJI ID. This does not establish absence of the linked drone or a measured detection failure rate: transmitted ID packet totals, emission band and coverage are unknown.
- Host detector queue loss was substantial even though no UHD error events were reported. Queue loss and partial-frame discard are separate causes. Zero UHD error events does not prove zero RF/network loss.
- The DC run dropped more samples and had higher sampled latency. This is not a controlled A/B efficacy test. DC remains optional/default off; processing throughput takes priority.
- Per-phase results are cumulative and can lag tuning. They must not be interpreted as per-channel packet counts or exact scan coverage.
- UHD printed socket-buffer recommendations. No system networking/sysctl settings were changed in response.
- No Recorder source or recording workflow changed. These runs saved metrics/logs, not a labeled raw-IQ corpus.

## Offline decoder correction

The channelized Mini 2 regression was caused by a residual integer-carrier ambiguity: cyclic-prefix synchronization estimates frequency offset modulo 15 kHz. The decoder now tries the original frame and the two adjacent carrier hypotheses, accepting only the existing turbo/CRC24A/CRC16 and payload checks. The corrected +9.6 MHz channel, 50→25 MS/s regression recovers all eight original sequences (786, 787, 788, 789, 800, 804, 805, 806). No expected count was relaxed. Added a regression for injected ±15 kHz errors. Extra retries are bounded but can increase processing cost on rejected candidates.

All seven CTest suites pass (12.58 s), and all 12 raw and 12 DC baseline benchmark cases pass. Known channelized Air 2 remains valid. Artifacts: ../mini2-cfo-2026-09-24, ../cfo-2026-09-24-baseline, ../cfo-2026-09-24-dc.

Next: profile and reduce live queue loss, then obtain labeled fixed-channel IQ positives and real negatives, firmware/setup metadata and repeated/held-out sessions. These short scans do not close that field-validation work.
