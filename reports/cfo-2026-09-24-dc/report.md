# Detector replay benchmark

Overall: **PASS**

| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |
| --- | --- | --- | ---: | ---: | ---: |
| dji_mini2 | real / development | pass | 8 | 628.90 | 43.22 |
| dji_mavic_air_2 | real / development | pass | 1 | 407.92 | 90.54 |
| noise_2000000 | synthetic / development | pass | 0 | 11.19 | 0.56 |
| tone_2000000 | synthetic / development | pass | 0 | 12.13 | 0.61 |
| silence_2000000 | synthetic / development | pass | 0 | 0.72 | 0.04 |
| noise_4000000 | synthetic / development | pass | 0 | 16.31 | 0.82 |
| tone_4000000 | synthetic / development | pass | 0 | 19.13 | 0.96 |
| silence_4000000 | synthetic / development | pass | 0 | 1.31 | 0.07 |
| noise_25000000 | synthetic / development | pass | 0 | 19.74 | 0.99 |
| tone_25000000 | synthetic / development | pass | 0 | 42.22 | 2.11 |
| silence_25000000 | synthetic / development | pass | 0 | 17.70 | 0.89 |
| generic_css | synthetic / development | pass | 0 | 3.74 | 0.19 |

## Interpretation

Exploratory observations have no packet ground truth and are not scored accuracy tests.
Counts are decoded events, not aircraft counts. Candidate counts can include overlap retries.
Timing includes enabled preprocessing and Analyzer.analyze; it excludes radio, IPC, startup and disk I/O.
Analysis/IQ duration > 1 means processing took longer than the recorded signal duration.
First-valid time is the end of the input block that produced a valid packet, not packet arrival time.
Transmitted packet totals are unknown: packet recovery percentage is not calculated.
Synthetic negatives are not independent field validation. Existing DJI fixtures are development data.
No radio loss, scan coverage, live range or universal accuracy is measured by this replay.

Held-out captures tested: 0.
Detailed metadata, hashes, environment, events and failures are in report.json.
