#!/usr/bin/env python3
"""Read-only DroneDetect_V2 inventory and bounded exploratory pilot preparation."""
import argparse
from collections import Counter
import itertools
import json
import math
from pathlib import Path
import re
import numpy as np
from benchmark import window_digest, load_manifest

CONDITIONS = {'CLEAN':'00', 'BLUE':'01', 'WIFI':'10', 'BOTH':'11'}
MODES = {'ON':'00', 'HO':'01', 'FY':'10'}
AIRCRAFT = {'AIR':'AIR', 'DIS':'DIS', 'INS':'INS', 'MIN':'MIN', 'MP1':'MA1', 'MP2':'MAV', 'PHA':'PHA'}


def labels(relative):
    parts = Path(relative).parts
    if len(parts) != 3 or parts[0] not in CONDITIONS:
        raise ValueError('Unexpected condition/path layout')
    match = re.fullmatch(r'([A-Z0-9]+)_(ON|HO|FY)', parts[1])
    if not match or match[1] not in AIRCRAFT:
        raise ValueError('Unknown aircraft or mode code')
    aircraft, mode = match.groups()
    prefix = AIRCRAFT[aircraft] + '_' + CONDITIONS[parts[0]] + MODES[mode] + '_'
    run = re.fullmatch(re.escape(prefix)+r'(0[0-4])\.dat', parts[2])
    if not run:
        raise ValueError('Filename does not match directory condition/mode/aircraft')
    return dict(condition=parts[0], aircraft_code=aircraft, mode=mode, repeat=int(run[1]))


def probe(path, count=16384):
    size = path.stat().st_size
    if not size or size % 8:
        raise ValueError('Not complete 8-byte complex samples')
    n = size//8
    count = min(count, n)
    out = []
    with path.open('rb') as f:
        for offset in sorted({0, max(0, (n-count)//2), n-count}):
            f.seek(offset*8)
            x = np.frombuffer(f.read(count*8), dtype='<c8').astype(np.complex128)
            if len(x) != count:
                raise ValueError('Source truncated while probing')
            finite = np.isfinite(x)
            q = dict(sample_offset=offset, samples=count, finite_fraction=float(finite.mean()))
            if finite.all():
                mean = x.mean()
                power = float(np.mean(np.abs(x)**2))
                q.update(mean_i=float(mean.real), mean_q=float(mean.imag),
                    rms=math.sqrt(power), ac_rms=float(np.sqrt(np.mean(np.abs(x-mean)**2))),
                    dc_power_fraction=float(abs(mean)**2/power) if power else 0.,
                    peak_component=float(max(np.abs(x.real).max(), np.abs(x.imag).max())))
            out.append(q)
    return out


def prepare(root, output, rate, frequency, window_ms=20):
    root = root.resolve()
    if not root.is_dir():
        raise ValueError('Dataset directory does not exist')
    files = sorted(root.rglob('*.dat'))
    if not files:
        raise ValueError('No DAT recordings found')
    rows, errors = [], []
    for path in files:
        try:
            row = dict(path=str(path.relative_to(root)), **labels(path.relative_to(root)),
                       size_bytes=path.stat().st_size, probes=probe(path))
            row['duration_s_assumed'] = row['size_bytes']/8/rate
            rows.append(row)
        except (ValueError, OSError) as exc:
            errors.append(dict(path=str(path.relative_to(root)), error=str(exc)))
    counts = Counter((r['condition'], r['aircraft_code'], r['mode']) for r in rows)
    missing = [dict(condition=c, aircraft_code=a, mode=m, count=counts[c,a,m])
               for c,a,m in itertools.product(CONDITIONS,AIRCRAFT,MODES) if counts[c,a,m] != 5]
    audit = dict(schema_version=1, root=str(root), recordings=len(files), total_bytes=sum(p.stat().st_size for p in files),
        metadata_status='Provisional: rate/center supplied by operator; local files contain no verified acquisition metadata',
        sample_rate_hz_assumed=rate, center_frequency_hz_assumed=frequency,
        format='cf32_le (plausible from bounded byte probes, not an embedded format declaration)',
        session_status='Acquisition session independence unknown; all windows development only',
        license_status='No local license found; redistribution not authorized by this audit',
        errors=errors, incomplete_groups=missing, recordings_detail=rows)
    captures = []
    count = round(rate*window_ms/1000)
    for row in rows:
        if row['mode'] != 'ON' or row['repeat'] != 0:
            continue
        path = root/row['path']
        # Separated windows do not pretend to preserve hopping or decoder continuity.
        for index, seconds in enumerate((.25, .9, 1.5)):
            offset = round(rate*seconds)
            if (offset+count)*8 > row['size_bytes']:
                errors.append(dict(path=row['path'], error=f'Pilot window {index} outside recording'))
                continue
            captures.append(dict(id=f"dd-{row['condition']}-{row['aircraft_code']}-on-00-w{index}".lower(),
                kind='real', evaluation='exploratory', expected=None, split='development',
                session='dronedetect-v2-acquisition-unknown', format='cf32_le',
                path=str(path), sha256_scope='window', sha256=window_digest(path,offset,count),
                sample_offset=offset, sample_count=count, source_size_bytes=row['size_bytes'],
                sample_rate_hz=rate, center_frequency_hz=frequency,
                provenance='Local DroneDetect_V2; https://ieee-dataport.org/comment/4739; acquisition metadata provisional',
                dataset_labels={k:row[k] for k in ('condition','aircraft_code','mode','repeat')},
                metadata_status=audit['metadata_status']))
    if not captures:
        raise ValueError('No eligible ON repeat-00 windows')
    output.mkdir(parents=True, exist_ok=False)
    manifest = output/'pilot.json'
    manifest.write_text(json.dumps(dict(schema_version=1,captures=captures),indent=2)+'\n')
    load_manifest(manifest)
    (output/'audit.json').write_text(json.dumps(audit,indent=2,allow_nan=False)+'\n')
    lines = ['# DroneDetect_V2 local audit','',f'Recordings: {len(files)}; bytes: {audit["total_bytes"]:,}.',
        '',audit['metadata_status']+'.',audit['session_status']+'.',audit['license_status']+'.',
        '',f'Pilot: {len(captures)} independently reset windows from ON repeat 00, at 0.25, 0.9 and 1.5 s (assuming {rate:g} samples/s).',
        'Hashes cover selected raw bytes only; source files are never copied or modified.',
        'Aircraft codes are dataset labels, not decoded protocol or identity ground truth.',
        'Interference conditions are not drone-free negatives. Probes do not audit every sample.',
        '', '| Condition | Files |', '| --- | ---: |']
    lines += [f'| {c} | {sum(r["condition"]==c for r in rows)} |' for c in CONDITIONS]
    lines += ['', '## Incomplete groups relative to five repeats per combination','']
    lines += [f'- {r["condition"]}/{r["aircraft_code"]}_{r["mode"]}: {r["count"]}' for r in missing]
    lines += ['',f'Errors: {len(errors)}. Full file inventory, quality probes and errors: audit.json.']
    (output/'audit.md').write_text('\n'.join(lines)+'\n')
    return audit, captures


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--sample-rate-hz',type=float,required=True,help='Explicit assumption until acquisition metadata verified')
    p.add_argument('--center-frequency-hz',type=float,required=True)
    p.add_argument('--window-ms',type=float,default=20)
    a=p.parse_args()
    for v,lo,hi in ((a.sample_rate_hz,1e5,100e6),(a.center_frequency_hz,1,10e9),(a.window_ms,.1,1000)):
        if not math.isfinite(v) or not lo<=v<=hi: p.error('Invalid numeric parameter')
    try:
        audit,captures=prepare(a.root,a.output,a.sample_rate_hz,a.center_frequency_hz,a.window_ms)
    except (OSError,ValueError) as exc: p.error(str(exc))
    print(f"Audited {audit['recordings']} files; prepared {len(captures)} windows; {len(audit['errors'])} errors")
    return 1 if audit['errors'] else 0

if __name__=='__main__':
    raise SystemExit(main())
