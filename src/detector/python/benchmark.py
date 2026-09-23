#!/usr/bin/env python3
"""Manifest-driven offline regression through the live Analyzer. Never opens RF hardware."""
import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
import platform
import re
import sys
import time

import numpy as np
import scipy
from backend import Analyzer, MAX_SAMPLES


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as stream:
        for block in iter(lambda: stream.read(1024*1024), b""):
            h.update(block)
    return h.hexdigest()


def window_digest(path, offset, count):
    """Hash exactly count complex samples, rejecting truncated evidence."""
    h = hashlib.sha256()
    with open(path, "rb") as stream:
        stream.seek(offset * 8)
        remaining = count * 8
        while remaining:
            block = stream.read(min(1024 * 1024, remaining))
            if not block:
                raise ValueError("Window extends beyond file")
            h.update(block)
            remaining -= len(block)
    return h.hexdigest()


def load_manifest(path):
    data = json.loads(Path(path).read_text())
    if data.get("schema_version") != 1 or not isinstance(data.get("captures"), list) or not data["captures"]:
        raise ValueError("Expected schema_version 1 and nonempty captures")
    ids, sessions, recordings = set(), {}, {}
    for c in data["captures"]:
        if not re.fullmatch(r"[a-z0-9_-]+", c.get("id", "")) or c["id"] in ids:
            raise ValueError("Capture IDs must be unique lowercase slugs")
        ids.add(c["id"])
        if c.get("split") not in ("development", "held_out") or not c.get("session"):
            raise ValueError("Each capture needs a split and independent session ID")
        if sessions.setdefault(c["session"], c["split"]) != c["split"]:
            raise ValueError("A session cannot occur in both development and held-out sets")
        if c.get("format") != "cf32_le":
            raise ValueError("Only interleaved little-endian float32 IQ is supported")
        for key, low, high in (("sample_rate_hz", 1e5, 100e6), ("center_frequency_hz", 1, 10e9)):
            value = c.get(key)
            if isinstance(value, bool) or not isinstance(value, (int, float)) or not math.isfinite(value) or not low <= value <= high:
                raise ValueError(f"Invalid {key} for {c['id']}")
        if c.get("kind") not in ("real", "synthetic") or not c.get("provenance"):
            raise ValueError("Capture kind and provenance are required")
        evaluation = c.get("evaluation", "regression")
        if evaluation not in ("regression", "exploratory"):
            raise ValueError("Unknown evaluation mode")
        if evaluation == "exploratory":
            if c.get("expected") is not None:
                raise ValueError("Exploratory captures must not carry scored expectations")
        else:
            expected = c.get("expected", {})
            bounds = [expected.get("confirmed_min"), expected.get("confirmed_max")]
            if any(type(x) is not int or x < 0 for x in bounds) or bounds[0] > bounds[1]:
                raise ValueError("Expected confirmed_min/max must be ordered nonnegative integers")
            for key in ("required_protocols", "allowed_protocols", "confirmed_models"):
                if not isinstance(expected.get(key), list) or not all(isinstance(x, str) for x in expected[key]):
                    raise ValueError(f"Expected {key} must be a list of strings")
        if "generator" in c:
            g = c["generator"]
            if c["kind"] != "synthetic" or g.get("type") not in ("noise", "tone", "silence", "css"):
                raise ValueError("Unsupported synthetic generator")
            if type(g.get("samples")) is not int or not 4096 <= g["samples"] <= MAX_SAMPLES:
                raise ValueError("Synthetic fixtures must contain 4096..MAX_SAMPLES samples")
            if type(g.get("seed")) is not int or g["seed"] < 0:
                raise ValueError("Synthetic fixtures require a nonnegative integer seed")
            if g["type"] == "css" and c["sample_rate_hz"] != 2e6:
                raise ValueError("CSS reference generator requires 2 MS/s")
        elif not isinstance(c.get("path"), str) or not re.fullmatch(r"[0-9a-f]{64}", c.get("sha256", "")):
            raise ValueError("File fixtures require path and SHA256")
        if "path" in c:
            recording = str((Path(path).resolve().parent / c["path"]).resolve())
            if recordings.setdefault(recording, c["split"]) != c["split"]:
                raise ValueError("Windows from one recording cannot cross splits")
        scope = c.get("sha256_scope", "file")
        if scope not in ("file", "window"):
            raise ValueError("Unknown SHA256 scope")
        if scope == "window":
            if "generator" in c:
                raise ValueError("Window hashing requires a file")
            for key, minimum in (("sample_offset", 0), ("sample_count", 1), ("source_size_bytes", 8)):
                if type(c.get(key)) is not int or c[key] < minimum:
                    raise ValueError(f"Invalid window {key}")
            if c["source_size_bytes"] % 8 or (c["sample_offset"] + c["sample_count"]) * 8 > c["source_size_bytes"]:
                raise ValueError("Window exceeds complete source samples")
        elif any(k in c for k in ("sample_offset", "sample_count")):
            raise ValueError("Window bounds require window SHA256 scope")
    return data


def generated(c):
    g, rate = c["generator"], c["sample_rate_hz"]
    n = g["samples"]
    rng = np.random.default_rng(g["seed"])
    if g["type"] == "noise":
        x = .01*(rng.normal(size=n)+1j*rng.normal(size=n))
    elif g["type"] == "tone":
        x = .1*np.exp(2j*np.pi*100000*np.arange(n)/rate)
    elif g["type"] == "css":
        from scipy.signal import resample_poly
        t = np.arange(128)
        chirp = np.exp(1j*np.pi*(t*t/128-t))
        x = resample_poly(np.tile(chirp, math.ceil(n/512)), 4, 1)[:n] * .1
    else:
        x = np.zeros(n)
    return np.asarray(x, dtype="<c8")


def run_capture(c, base, turbo, chunk_ms, analyzer_factory=Analyzer, preprocessing=None):
    result = dict(id=c["id"], kind=c["kind"], split=c["split"], session=c["session"],
                  status="error", failures=[], metadata=c)
    try:
        if "generator" in c:
            samples = generated(c)
            result["input_sha256"] = hashlib.sha256(samples.tobytes()).hexdigest()
        else:
            path = (base/c["path"]).resolve()
            scope = c.get("sha256_scope", "file")
            result["input_hash_scope"] = scope
            if scope == "window":
                if path.stat().st_size != c["source_size_bytes"]:
                    raise ValueError("Source size changed")
                result["input_sha256"] = window_digest(path, c["sample_offset"], c["sample_count"])
            else:
                result["input_sha256"] = digest(path)
            if result["input_sha256"] != c["sha256"]:
                raise ValueError("Input SHA256 mismatch; refusing to score changed evidence")
            if path.stat().st_size == 0 or path.stat().st_size % 8:
                raise ValueError("IQ file must contain complete 8-byte complex samples")
            samples = np.memmap(path, mode="r", dtype="<c8",
                offset=c.get("sample_offset", 0)*8,
                shape=(c["sample_count"],) if scope == "window" else None)
        analyzer = analyzer_factory(turbo)
        if preprocessing:
            from preprocessing import PreparedAnalyzer
            analyzer = PreparedAnalyzer(analyzer, **preprocessing)
        preprocessing_ms = 0.
        processed_samples = 0
        processing_metadata = None
        rate, freq = c["sample_rate_hz"], c["center_frequency_hz"]
        chunk_size = max(1, min(MAX_SAMPLES, int(rate*chunk_ms/1000)))
        times, events = [], []
        candidates = rejected = 0
        first_available = None
        for position in range(0, len(samples), chunk_size):
            block = samples[position:position+chunk_size]
            start = time.perf_counter()
            output = analyzer.analyze(block, rate, freq, (position+c.get("sample_offset", 0))/rate, 0)
            times.append((time.perf_counter()-start)*1000)
            if output.get("error"):
                raise ValueError(output["error"])
            preprocessing_ms += output.get("preprocessing_ms", 0)
            processing_metadata = output.get("preprocessing", processing_metadata)
            processed_samples += output.get("preprocessing", {}).get("output_samples", len(block))
            candidates += output["candidates"]
            rejected += output["rejected"]
            if first_available is None and any(e["confirmed"] for e in output["events"]):
                first_available = (position+len(block)+c.get("sample_offset", 0))/rate
            events.extend(output["events"])
        confirmed = [e for e in events if e["confirmed"]]
        protocols = Counter(e["protocol"] for e in events)
        models = sorted(set(e.get("model", "") for e in confirmed))
        failures = []
        exploratory = c.get("evaluation") == "exploratory"
        if not exploratory:
            expected = c["expected"]
            if not expected["confirmed_min"] <= len(confirmed) <= expected["confirmed_max"]:
                failures.append(f"Confirmed count {len(confirmed)} outside [{expected['confirmed_min']}, {expected['confirmed_max']}]")
            missing = set(expected["required_protocols"])-protocols.keys()
            unexpected = protocols.keys()-set(expected["allowed_protocols"])
            if missing: failures.append("Missing protocols: "+", ".join(sorted(missing)))
            if unexpected: failures.append("Unexpected protocols: "+", ".join(sorted(unexpected)))
            if set(models) != set(expected["confirmed_models"]):
                failures.append(f"Confirmed models differ: {models}")
        duration = len(samples)/rate
        result.update(status="observed" if exploratory else ("fail" if failures else "pass"), failures=failures,
            samples=len(samples), duration_s=duration, chunks=len(times), confirmed=len(confirmed),
            candidates=candidates, rejected=rejected, protocols=dict(sorted(protocols.items())),
            confirmed_models=models, first_valid_available_capture_s=first_available,
            analysis_ms=sum(times), chunk_p50_ms=float(np.percentile(times,50)),
            chunk_p95_ms=float(np.percentile(times,95)), realtime_factor=sum(times)/1000/duration,
            preprocessing_ms=preprocessing_ms, processed_samples=processed_samples,
            preprocessing=processing_metadata, packet_recovery_fraction=None, events=events)
    except Exception as exc:
        result["failures"] = [f"{type(exc).__name__}: {exc}"]
    return result


def markdown(report):
    lines = ["# Detector replay benchmark", "", f"Overall: **{report['status'].upper()}**", "",
        "| Capture | Kind / split | Result | Valid packets | p95 chunk ms | Analysis / IQ duration |",
        "| --- | --- | --- | ---: | ---: | ---: |"]
    for r in report["results"]:
        lines.append(f"| {r['id']} | {r['kind']} / {r['split']} | {r['status']} | {r.get('confirmed','—')} | {r.get('chunk_p95_ms',0):.2f} | {r.get('realtime_factor',0):.2f} |")
        for error in r["failures"]:
            lines.append("\nFailure: "+r["id"]+": "+error.replace("\n", " ")+"\n")
    lines += ["", "## Interpretation", "",
        "Exploratory observations have no packet ground truth and are not scored accuracy tests.",
        "Counts are decoded events, not aircraft counts. Candidate counts can include overlap retries.",
        "Timing includes enabled preprocessing and Analyzer.analyze; it excludes radio, IPC, startup and disk I/O.",
        "Analysis/IQ duration > 1 means processing took longer than the recorded signal duration.",
        "First-valid time is the end of the input block that produced a valid packet, not packet arrival time.",
        "Transmitted packet totals are unknown: packet recovery percentage is not calculated.",
        "Synthetic negatives are not independent field validation. Existing DJI fixtures are development data.",
        "No radio loss, scan coverage, live range or universal accuracy is measured by this replay.", "",
        f"Held-out captures tested: {sum(r['split']=='held_out' for r in report['results'])}.",
        "Detailed metadata, hashes, environment, events and failures are in report.json."]
    return "\n".join(lines)+"\n"


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--manifest", type=Path, required=True)
    p.add_argument("--turbo", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True, help="New report directory (will not overwrite prior evidence)")
    p.add_argument("--chunk-ms", type=float, default=20)
    p.add_argument("--split", choices=("all", "development", "held_out"), default="all")
    p.add_argument("--dc-block", action="store_true")
    p.add_argument("--shift-hz", type=float, default=0.)
    p.add_argument("--output-rate-hz", type=float)
    p.add_argument("--bandwidth-hz", type=float)
    args = p.parse_args(argv)
    preprocessing = dict(dc_block=args.dc_block, shift_hz=args.shift_hz,
        output_rate_hz=args.output_rate_hz, bandwidth_hz=args.bandwidth_hz)
    if not any(preprocessing.values()): preprocessing = None
    if not math.isfinite(args.chunk_ms) or not .1 <= args.chunk_ms <= 1000:
        p.error("--chunk-ms must be between 0.1 and 1000")
    try:
        manifest = load_manifest(args.manifest)
        captures = [c for c in manifest["captures"] if args.split == "all" or c["split"] == args.split]
        if not captures: raise ValueError("No captures selected; an empty set cannot pass")
        turbo_hash = digest(args.turbo)
        args.output.mkdir(parents=True, exist_ok=False)
    except (ValueError, OSError, TypeError) as exc:
        p.error(str(exc))
    source = Path(__file__).parent
    report = dict(schema_version=1, manifest_sha256=digest(args.manifest), turbo_sha256=turbo_hash,
        source_sha256={str(f.relative_to(source)):digest(f) for f in sorted(source.rglob("*.py"))},
        environment=dict(python=platform.python_version(), numpy=np.__version__, scipy=scipy.__version__, platform=platform.platform()),
        chunk_ms=args.chunk_ms, preprocessing=preprocessing, results=[])
    for c in captures:
        r = run_capture(c, args.manifest.resolve().parent, args.turbo.resolve(), args.chunk_ms, preprocessing=preprocessing)
        report["results"].append(r)
        print(f"{c['id']}: {r['status']}", flush=True)
    statuses = {r["status"] for r in report["results"]}
    report["status"] = "fail" if statuses & {"fail", "error"} else ("exploratory" if "observed" in statuses else "pass")
    (args.output/"report.json").write_text(json.dumps(report, indent=2, allow_nan=False)+"\n")
    (args.output/"report.md").write_text(markdown(report))
    return 0 if report["status"] in ("pass", "exploratory") else 1


if __name__ == "__main__":
    sys.exit(main())
