#!/usr/bin/env python3
"""Run every local regression without rewriting unrelated CRLF source files.
Each script receives a normalized temporary copy in the same tests directory;
it is removed even after failure. Logs and a JSON result remain outside repo.
"""
from pathlib import Path
import subprocess,tempfile,json,os,time
r=Path(__file__).resolve().parent.parent
out=Path('/tmp/zombie-engineering-20260928/test-results');out.mkdir(exist_ok=True)
results=[]
for script in sorted((r/'tests').glob('run_*')):
 if script.suffix not in ['.py','.sh']:continue
 start=time.monotonic();temp=None
 try:
  if script.suffix=='.sh':
   fd,name=tempfile.mkstemp(prefix='.normalized-',suffix='.sh',dir=script.parent);os.close(fd);temp=Path(name);temp.write_text(script.read_text());cmd=['bash',str(temp)]
  else:cmd=['python3',str(script)]
  with (out/(script.stem+'.log')).open('w') as log:
   proc=subprocess.run(cmd,cwd=r,stdout=log,stderr=subprocess.STDOUT,env={**os.environ,'VITASDK':'/usr/local/vitasdk'})
  result={'test':script.name,'exit':proc.returncode,'seconds':round(time.monotonic()-start,2)}
  results.append(result);print(result,flush=True)
 finally:
  if temp:temp.unlink(missing_ok=True)
(out/'results.json').write_text(json.dumps(results,indent=2)+'\n')
raise SystemExit(any(x['exit']!=0 for x in results))
