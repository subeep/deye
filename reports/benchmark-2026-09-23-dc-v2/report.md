# Detector replay benchmark

Overall: **PASS**

| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |
| --- | --- | --- | ---: | ---: | ---: |
| dji_mini2 | real / development | pass | 8 | 630.36 | 43.32 |
| dji_mavic_air_2 | real / development | pass | 1 | 179.49 | 39.84 |
| noise_2000000 | synthetic / development | pass | 0 | 10.43 | 0.52 |
| tone_2000000 | synthetic / development | pass | 0 | 11.42 | 0.57 |
| silence_2000000 | synthetic / development | pass | 0 | 0.61 | 0.03 |
| noise_4000000 | synthetic / development | pass | 0 | 15.83 | 0.79 |
| tone_4000000 | synthetic / development | pass | 0 | 18.98 | 0.95 |
| silence_4000000 | synthetic / development | pass | 0 | 1.15 | 0.06 |
| noise_25000000 | synthetic / development | pass | 0 | 17.85 | 0.89 |
| tone_25000000 | synthetic / development | pass | 0 | 36.35 | 1.82 |
| silence_25000000 | synthetic / development | pass | 0 | 13.80 | 0.69 |
| generic_css | synthetic / development | pass | 0 | 3.71 | 0.19 |

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
