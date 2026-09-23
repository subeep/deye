# Detector replay benchmark

Overall: **FAIL**

| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |
| --- | --- | --- | ---: | ---: | ---: |
| dji_mini2 | real / development | fail | 7 | 653.73 | 44.93 |

Failure: dji_mini2: Confirmed count 7 outside [8, 8]

| dji_mavic_air_2 | real / development | pass | 1 | 223.40 | 49.58 |
| noise_2000000 | synthetic / development | pass | 0 | 10.62 | 0.53 |
| tone_2000000 | synthetic / development | pass | 0 | 11.66 | 0.58 |
| silence_2000000 | synthetic / development | pass | 0 | 0.64 | 0.03 |
| noise_4000000 | synthetic / development | pass | 0 | 15.58 | 0.78 |
| tone_4000000 | synthetic / development | pass | 0 | 19.86 | 0.99 |
| silence_4000000 | synthetic / development | pass | 0 | 1.22 | 0.06 |
| noise_25000000 | synthetic / development | pass | 0 | 19.78 | 0.99 |
| tone_25000000 | synthetic / development | pass | 0 | 42.01 | 2.10 |
| silence_25000000 | synthetic / development | pass | 0 | 15.04 | 0.75 |
| generic_css | synthetic / development | pass | 0 | 3.54 | 0.18 |

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
