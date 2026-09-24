# Detector replay benchmark

Overall: **PASS**

| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |
| --- | --- | --- | ---: | ---: | ---: |
| dji_mini2 | real / development | pass | 8 | 629.58 | 43.27 |
| dji_mavic_air_2 | real / development | pass | 1 | 406.41 | 90.20 |
| noise_2000000 | synthetic / development | pass | 0 | 10.65 | 0.53 |
| tone_2000000 | synthetic / development | pass | 0 | 12.89 | 0.64 |
| silence_2000000 | synthetic / development | pass | 0 | 0.19 | 0.01 |
| noise_4000000 | synthetic / development | pass | 0 | 15.81 | 0.79 |
| tone_4000000 | synthetic / development | pass | 0 | 21.57 | 1.08 |
| silence_4000000 | synthetic / development | pass | 0 | 0.25 | 0.01 |
| noise_25000000 | synthetic / development | pass | 0 | 14.04 | 0.70 |
| tone_25000000 | synthetic / development | pass | 0 | 52.38 | 2.62 |
| silence_25000000 | synthetic / development | pass | 0 | 10.86 | 0.54 |
| generic_css | synthetic / development | pass | 0 | 3.22 | 0.16 |

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
