#!/usr/bin/env python3
"""Summarize frame/texture costs; sampled I/O and thread waits are not an exclusive frame partition."""
import argparse,json,re,hashlib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('log',type=Path);p.add_argument('--last',type=int,default=12);p.add_argument('--json',type=Path)
a=p.parse_args();raw=a.log.read_bytes();windows=[];current=None
for line in raw.decode('utf-8',errors='replace').splitlines():
    if line.startswith('[PERF] frame '):
        current={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)};windows.append(current)
    elif current is not None and line.startswith('[PERF] gl '):
        current.update({k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)})
if not windows:raise SystemExit('No PERF frame windows found')
selected=windows[-a.last:];frames=sum(w['frames'] for w in selected);ms=sum(w['elapsed_ms'] for w in selected);tex=sum(w.get('tex_sub_total_us',0) for w in selected)
summary={'log_sha256':hashlib.sha256(raw).hexdigest(),'window_count':len(windows),'selected_windows':len(selected),'frames':frames,'elapsed_ms':ms,'fps':frames*1000/ms,'tex_sub_us_per_frame':tex/frames,'tex_sub_wall_fraction':tex/(ms*1000),'finish_calls':sum(w.get('finish_calls',0) for w in selected)}
print(json.dumps(summary,indent=2))
if a.json:a.json.write_text(json.dumps({'summary':summary,'windows':windows},indent=2)+'\n')
