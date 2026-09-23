# Detector replay benchmark

Overall: **PASS**

| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |
| --- | --- | --- | ---: | ---: | ---: |
| dji_mini2 | real / development | pass | 8 | 615.55 | 42.31 |
| dji_mavic_air_2 | real / development | pass | 1 | 181.72 | 40.33 |
| noise_2000000 | synthetic / development | pass | 0 | 10.43 | 0.52 |
| tone_2000000 | synthetic / development | pass | 0 | 11.40 | 0.57 |
| silence_2000000 | synthetic / development | pass | 0 | 0.66 | 0.03 |
| noise_4000000 | synthetic / development | pass | 0 | 15.30 | 0.76 |
| tone_4000000 | synthetic / development | pass | 0 | 18.04 | 0.90 |
| silence_4000000 | synthetic / development | pass | 0 | 1.18 | 0.06 |
| noise_25000000 | synthetic / development | pass | 0 | 19.54 | 0.98 |
| tone_25000000 | synthetic / development | pass | 0 | 40.36 | 2.02 |
| silence_25000000 | synthetic / development | pass | 0 | 18.42 | 0.92 |
| generic_css | synthetic / development | pass | 0 | 3.73 | 0.19 |

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
