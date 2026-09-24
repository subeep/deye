# Detector replay benchmark

Overall: **PASS**

| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |
| --- | --- | --- | ---: | ---: | ---: |
| dji_mini2 | real / development | pass | 8 | 222.72 | 15.31 |
| dji_mavic_air_2 | real / development | pass | 1 | 494.98 | 109.86 |
| noise_2000000 | synthetic / development | pass | 0 | 11.16 | 0.56 |
| tone_2000000 | synthetic / development | pass | 0 | 12.64 | 0.63 |
| silence_2000000 | synthetic / development | pass | 0 | 0.73 | 0.04 |
| noise_4000000 | synthetic / development | pass | 0 | 16.50 | 0.82 |
| tone_4000000 | synthetic / development | pass | 0 | 19.56 | 0.98 |
| silence_4000000 | synthetic / development | pass | 0 | 1.41 | 0.07 |
| noise_25000000 | synthetic / development | pass | 0 | 20.14 | 1.01 |
| tone_25000000 | synthetic / development | pass | 0 | 43.55 | 2.18 |
| silence_25000000 | synthetic / development | pass | 0 | 17.59 | 0.88 |
| generic_css | synthetic / development | pass | 0 | 3.68 | 0.18 |

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
