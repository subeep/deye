# Detector replay benchmark

Overall: **PASS**

| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |
| --- | --- | --- | ---: | ---: | ---: |
| dji_mini2 | real / development | pass | 8 | 603.83 | 41.50 |
| dji_mavic_air_2 | real / development | pass | 1 | 179.59 | 39.86 |
| noise_2000000 | synthetic / development | pass | 0 | 10.57 | 0.53 |
| tone_2000000 | synthetic / development | pass | 0 | 11.50 | 0.57 |
| silence_2000000 | synthetic / development | pass | 0 | 0.10 | 0.01 |
| noise_4000000 | synthetic / development | pass | 0 | 13.88 | 0.69 |
| tone_4000000 | synthetic / development | pass | 0 | 18.03 | 0.90 |
| silence_4000000 | synthetic / development | pass | 0 | 0.21 | 0.01 |
| noise_25000000 | synthetic / development | pass | 0 | 11.14 | 0.56 |
| tone_25000000 | synthetic / development | pass | 0 | 43.16 | 2.16 |
| silence_25000000 | synthetic / development | pass | 0 | 6.21 | 0.31 |
| generic_css | synthetic / development | pass | 0 | 2.89 | 0.14 |

## Interpretation

Counts are decoded events, not aircraft counts. Candidate counts can include overlap retries.
Timing is offline wall time around Analyzer.analyze; it excludes radio, IPC, startup and disk I/O.
Analysis/IQ duration > 1 means processing took longer than the recorded signal duration.
First-valid time is the end of the input block that produced a valid packet, not packet arrival time.
Transmitted packet totals are unknown: packet recovery percentage is not calculated.
Synthetic negatives are not independent field validation. Existing DJI fixtures are development data.
No radio loss, scan coverage, live range or universal accuracy is measured by this replay.

Held-out captures tested: 0.
Detailed metadata, hashes, environment, events and failures are in report.json.
