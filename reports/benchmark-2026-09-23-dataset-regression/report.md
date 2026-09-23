# Detector replay benchmark

Overall: **PASS**

| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |
| --- | --- | --- | ---: | ---: | ---: |
| dji_mini2 | real / development | pass | 8 | 570.09 | 39.18 |
| dji_mavic_air_2 | real / development | pass | 1 | 178.96 | 39.72 |
| noise_2000000 | synthetic / development | pass | 0 | 9.69 | 0.48 |
| tone_2000000 | synthetic / development | pass | 0 | 11.49 | 0.57 |
| silence_2000000 | synthetic / development | pass | 0 | 0.11 | 0.01 |
| noise_4000000 | synthetic / development | pass | 0 | 13.99 | 0.70 |
| tone_4000000 | synthetic / development | pass | 0 | 18.08 | 0.90 |
| silence_4000000 | synthetic / development | pass | 0 | 0.22 | 0.01 |
| noise_25000000 | synthetic / development | pass | 0 | 11.12 | 0.56 |
| tone_25000000 | synthetic / development | pass | 0 | 42.98 | 2.15 |
| silence_25000000 | synthetic / development | pass | 0 | 9.76 | 0.49 |
| generic_css | synthetic / development | pass | 0 | 2.91 | 0.15 |

## Interpretation

Exploratory observations have no packet ground truth and are not scored accuracy tests.
Counts are decoded events, not aircraft counts. Candidate counts can include overlap retries.
Timing is offline wall time around Analyzer.analyze; it excludes radio, IPC, startup and disk I/O.
Analysis/IQ duration > 1 means processing took longer than the recorded signal duration.
First-valid time is the end of the input block that produced a valid packet, not packet arrival time.
Transmitted packet totals are unknown: packet recovery percentage is not calculated.
Synthetic negatives are not independent field validation. Existing DJI fixtures are development data.
No radio loss, scan coverage, live range or universal accuracy is measured by this replay.

Held-out captures tested: 0.
Detailed metadata, hashes, environment, events and failures are in report.json.
