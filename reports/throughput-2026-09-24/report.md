# Throughput, scan/hold and field evidence — 2026-09-24

## Network-only results

Host NIC enp3s0: negotiated 1000 Mb/s, MTU 1500. UHD receive-only, one channel, sc16 wire format, fc32 host format, ten seconds/rate. Both 20 and 25 MS/s runs completed with zero dropped samples, overruns, receive sequence errors or timeouts; NIC error/drop counters did not increase. The first 25 MS/s attempt failed during RFNoC initialization before streaming; its log and successful retry are preserved. No system network settings were changed. Short successful tests do not certify all future traffic/load conditions.

## Software measurements

- Same 84 dataset windows: analysis 4.943→3.285 s (~34% reduction), exact event/count equivalence. Removed phase/median calculations from wideband paths where their results were unused. Profiles and before/after JSON retained.
- Two known DJI fixtures: analysis 1.104→0.830 s (~25% reduction), all nine validated packets retained. Timing search uses a coarse/refined pass over the existing grid, then the original exhaustive search on validation failure. Failed candidates can cost more because of fallback; these figures are not a universal speedup.
- All seven CTest suites pass (13.62 s), including both 20 MS/s derived known captures, ±15 kHz correction, forced exhaustive fallback, hold freshness and subprocess acquisition metadata. All 12 raw and 12 DC benchmark cases pass.
- Host copies/IPC were not independently isolated by the Python profiles; they remain a possible next optimization target. The bounded queue was not enlarged.

## Live fixed-channel results

All rows: 2444.5 MHz, RX2, gain 20 dB, about ten seconds, DC off. Runs are sequential and see different RF traffic. Positive/off capture rows also copy two seconds into a bounded evidence buffer, so they are not identical performance workloads. No average detection probability or packet recovery percentage can be computed.

| Run | Rate MS/s | Analyzed M | Queue dropped M | Partial discarded M | Valid packets | Max sampled result age ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| live20 | 20 | 179.831 | 13.525 | 5.661 | 0 | 254.9 |
| live25 | 25 | 229.065 | 7.528 | 11.870 | 2 | 195.8 |
| linked-positive25 | 25 | 223.986 | 14.590 | 10.174 | 0 | 250.1 |
| final20-retry | 20 | 187.056 | 2.531 | 9.404 | 0 | 211.5 |
| target-off25 | 25 | 195.035 | 43.811 | 9.929 | 0 | 442.2 |

All completed runs reported zero receiver overflow/timeout/other-error events and zero analysis errors. The first final20 attempt failed during RFNoC initialization; the retry completed. Two valid packets were observed in the first optimized 25 MS/s run; not every linked-state run decoded an ID. Queue losses remain substantial/variable. Therefore 25 MS/s stays the profile default, with an explicit 20 MS/s lower-data-rate button for controlled comparison.

## Scan/hold changes

Confirmed observations retain their original receiver epoch and host acquisition time. Only fresh, unexpired evidence acquired after the current tune request, matching current frequency/epoch, may extend a hold. The deadline is acquisition-based, so queue-delayed packets cannot silently extend it from their decode completion time. Tests cover stale, wrong-channel/epoch, expired, future and nonconfirmed observations. GUI now shows hold/dwell countdown, detected MHz and last-seen age. No interactive GUI validation was performed.

## Field corpus seed

Two complete, continuous two-second IQ captures (50 million complex float32 samples each; 400 MB each, 800 MB total) were saved without changing Recorder. The user confirmed the linked state for the first capture and switched off both target aircraft and controller for the second. First-batch timestamps, rate/frequency, state labels and SHA256 are preserved in field-captures.json. No capture continuity reset occurred. Data is developmental, from one site/session, not independent held-out evidence.
Offline replay produced no validated ID in either two-second sample. The target-off sample still contains RF/Wi-Fi-like activity. This labels target operating state, not proof that all drones or other emitters were absent. Packet presence/totals, firmware, antenna details and range remain unknown. Reports are under field-replay/.

## Next work

Repeat labeled captures on independent sessions and archive a checksum-valid live Mini 2 packet in IQ. Continue profiling rejected-candidate work and IPC/copies, then evaluate detector modules/Remote ID. Do not infer false-alerts/hour, universal identification or field range from these short tests. Both radio tests have stopped, with no firmware/network changes. Recorder UI and recording logic remain unchanged.
