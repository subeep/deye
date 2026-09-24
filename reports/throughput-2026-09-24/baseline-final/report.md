# Detector replay benchmark

Overall: **PASS**

| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |
| --- | --- | --- | ---: | ---: | ---: |
| dji_mini2 | real / development | pass | 8 | 220.25 | 15.14 |
| dji_mavic_air_2 | real / development | pass | 1 | 459.68 | 102.02 |
| noise_2000000 | synthetic / development | pass | 0 | 10.81 | 0.54 |
| tone_2000000 | synthetic / development | pass | 0 | 12.53 | 0.63 |
| silence_2000000 | synthetic / development | pass | 0 | 0.14 | 0.01 |
| noise_4000000 | synthetic / development | pass | 0 | 15.64 | 0.78 |
| tone_4000000 | synthetic / development | pass | 0 | 20.19 | 1.01 |
| silence_4000000 | synthetic / development | pass | 0 | 0.22 | 0.01 |
| noise_25000000 | synthetic / development | pass | 0 | 13.00 | 0.65 |
| tone_25000000 | synthetic / development | pass | 0 | 48.06 | 2.40 |
| silence_25000000 | synthetic / development | pass | 0 | 6.57 | 0.33 |
| generic_css | synthetic / development | pass | 0 | 3.33 | 0.17 |

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
