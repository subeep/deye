# Detector replay benchmark

Overall: **EXPLORATORY**

| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |
| --- | --- | --- | ---: | ---: | ---: |
| linked-positive25 | real / development | observed | 0 | 54.44 | 0.98 |
| target-off25 | real / development | observed | 0 | 19.85 | 0.92 |

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
