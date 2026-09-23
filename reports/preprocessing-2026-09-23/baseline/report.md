# Detector replay benchmark

Overall: **PASS**

| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |
| --- | --- | --- | ---: | ---: | ---: |
| dji_mini2 | real / development | pass | 8 | 606.52 | 41.69 |
| dji_mavic_air_2 | real / development | pass | 1 | 198.88 | 44.14 |
| noise_2000000 | synthetic / development | pass | 0 | 10.43 | 0.52 |
| tone_2000000 | synthetic / development | pass | 0 | 12.27 | 0.61 |
| silence_2000000 | synthetic / development | pass | 0 | 0.13 | 0.01 |
| noise_4000000 | synthetic / development | pass | 0 | 14.92 | 0.75 |
| tone_4000000 | synthetic / development | pass | 0 | 19.40 | 0.97 |
| silence_4000000 | synthetic / development | pass | 0 | 0.21 | 0.01 |
| noise_25000000 | synthetic / development | pass | 0 | 11.73 | 0.59 |
| tone_25000000 | synthetic / development | pass | 0 | 45.78 | 2.29 |
| silence_25000000 | synthetic / development | pass | 0 | 6.68 | 0.33 |
| generic_css | synthetic / development | pass | 0 | 3.10 | 0.16 |

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
