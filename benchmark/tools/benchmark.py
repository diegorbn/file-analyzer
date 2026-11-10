#!/usr/bin/env python3
import argparse, csv, hashlib, json, platform, re, statistics, subprocess, sys, tempfile
from datetime import datetime, timezone
from pathlib import Path
REPO_TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(REPO_TOOLS))
from plugins import TARGET_NAMES, atomic_copy, install, path, remove

NAMES = TARGET_NAMES["benchmark"]

ROOT=Path(__file__).resolve().parents[1]
REPO=ROOT
p=argparse.ArgumentParser()
p.add_argument('--trials',type=int,default=30)
p.add_argument('--warmups',type=int,default=5)
p.add_argument('--order',choices=('batch','alternating'),default='batch',help='run static then dynamic batches (default), or alternate variants')
p.add_argument('--output',type=Path,help='new results directory (must not exist)')
p.add_argument('--dataset',type=Path,default=ROOT/'datasets',help='directory containing the fixed dataset')
a=p.parse_args()
if a.trials<1 or a.warmups<0: p.error('trials must be positive and warmups nonnegative')
data=a.dataset.resolve(); built=ROOT/'plugins-built'; active=ROOT/'plugins-active'
if built.is_symlink() or active.is_symlink(): sys.exit('Plugin directories must not be symbolic links')
files=[data/ext/f'{i:05d}.{ext}' for ext in ('txt','csv') for i in range(10)]
if any(not f.is_file() or f.stat().st_size != 524288 for f in files): sys.exit('Fixed dataset missing or wrong size; run python3 tools/generate.py')
for ext in ('txt','csv'):
    for i in range(10):
        line=(f'row{i:05d},123.45,example,content\n' if ext=='csv' else f'File {i:05d}: deterministic sample text and words.\n').encode()
        header=b'id,value,name,note\n' if ext=='csv' else b''
        expected_data=header+(line*((524288-len(header))//len(line)+1))[:524288-len(header)]
        if (data/ext/f'{i:05d}.{ext}').read_bytes()!=expected_data: sys.exit('Dataset content differs from the fixed generator; regenerate explicitly')
if any(not (ROOT/'bin'/n).is_file() for n in ('runner','benchmark-static','benchmark-dynamic')): sys.exit('Build first: make all')
for name in NAMES:
    f=path(active,name)
    if f.is_symlink() or (f.exists() and not f.is_file()): sys.exit(f'Unsafe plugin path: {f}')
out=a.output or ROOT/'results'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S.%fZ')
out.mkdir(parents=True,exist_ok=False)
active.mkdir(parents=True,exist_ok=True)
original={n:(path(active,n).read_bytes(), path(active,n).stat().st_mode & 0o7777) if path(active,n).exists() else None for n in NAMES}
def digest(f):
    h=hashlib.sha256()
    with f.open('rb') as stream:
        for block in iter(lambda:stream.read(1048576),b''): h.update(block)
    return h.hexdigest()
def command_output(args):
    try: return subprocess.check_output(args,cwd=REPO,text=True,stderr=subprocess.STDOUT).strip()
    except (OSError,subprocess.CalledProcessError): return 'unavailable'
metadata={'source_sha256':{str(f.relative_to(REPO)):digest(f) for f in [REPO/'common/registry.c',REPO/'src/main.c',*[REPO/'analyzers'/f'{n}_plugin.c' for n in NAMES]]},'executable_sha256':{v:digest(ROOT/'bin'/('benchmark-'+v)) for v in ('static','dynamic')},'plugin_sha256':{n:digest(path(built,n)) for n in ('txt','csv')},'version':'phase-benchmark-2-txt-csv','kernel':platform.uname()._asdict(),'compiler':command_output(['gcc','--version']),'git_revision':command_output(['git','rev-parse','HEAD']),'flags':'-std=c11 -Wall -Wextra -Wpedantic -O0; analyzers -fPIC; shared objects -shared; dynamic frontend -DDYNAMIC -ldl', 'dataset':{'count_per_format':10,'bytes_per_file':524288,'generator':'deterministic repeated lines, no random seed','sha256':{str(f.relative_to(data)):digest(f) for f in files}},'trials':a.trials,'warmups':a.warmups,'order':('static batch then dynamic batch, warm-ups before measured trials per variant' if a.order == 'batch' else 'alternating static/dynamic by repetition'),'plugins':'TXT and CSV installed before trials; acquired once at start of each phase','timers':'external CLOCK_MONOTONIC before fork through wait4; activation CLOCK_MONOTONIC around acquire; processing around each file-analysis loop; RSS wait4 ru_maxrss KiB'}
(out/'metadata.json').write_text(json.dumps(metadata,indent=2)+'\n')
fields=['run_id','scenario','variant','trial','txt_files','csv_files','total_files','total_bytes','elapsed_ms','txt_activation_us','csv_activation_us','txt_processing_ms','csv_processing_ms','processing_ms','peak_rss_kb','executable_bytes','library_bytes','status','pid','report_sha256']
rows=[]; warm=[]; expected=None
def write(name,cols,items):
    with (out/name).open('w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=cols);w.writeheader();w.writerows(items)
write('raw.csv',fields,rows); write('warmups.csv',fields,warm)
try:
    remove(active,NAMES); install(built,active,('txt','csv'))
    libs=sum(path(active,n).stat().st_size for n in ('txt','csv'))
    repetitions = range(a.warmups + a.trials)
    schedule = ([(i, variant) for variant in ('static', 'dynamic') for i in repetitions]
                if a.order == 'batch' else
                [(i, variant) for i in repetitions
                 for variant in (('static', 'dynamic') if i % 2 == 0 else ('dynamic', 'static'))])
    for i, variant in schedule:
        exe=ROOT/'bin'/('benchmark-'+variant)
        with tempfile.TemporaryDirectory(prefix='analyzer-') as td:
            report=Path(td)/'report'; metrics=Path(td)/'metrics'
            cmd=[str(ROOT/'bin/runner'),str(exe),str(data),str(active),str(report),str(metrics)]
            result=subprocess.run(cmd,text=True,capture_output=True)
            diagnostic=metrics.read_text(errors='replace') if metrics.exists() else ''
            if result.returncode: raise RuntimeError(f'{variant} iteration {i}: {result.stderr} {diagnostic}')
            match=re.fullmatch(r'METRICS ([\d.]+) ([\d.]+) ([\d.]+) ([\d.]+) 20\n',diagnostic)
            if not match: raise RuntimeError(f'Invalid metrics: {diagnostic}')
            elapsed,rss,pid=result.stdout.strip().split(','); ta,ca,tp,cp=map(float,match.groups())
            h=digest(report)
            if expected is None: expected=h
            if h!=expected: raise RuntimeError(f'Analyzer output mismatch: {variant} iteration {i}')
            row=dict(zip(fields,[out.name,'fixed-10x512KiB',variant,i-a.warmups+1,10,10,20,10485760,float(elapsed),ta,ca,tp,cp,tp+cp,int(rss),exe.stat().st_size,libs if variant=='dynamic' else 0,'ok',int(pid),h]))
            (warm if i<a.warmups else rows).append(row)
            write('warmups.csv' if i<a.warmups else 'raw.csv',fields,warm if i<a.warmups else rows)
    if any(digest(f)!=metadata['dataset']['sha256'][str(f.relative_to(data))] for f in files): raise RuntimeError('Dataset changed during benchmark')
    write('raw.csv',fields,rows); write('warmups.csv',fields,warm)
    means={v:statistics.mean(r['elapsed_ms'] for r in rows if r['variant']==v) for v in ('static','dynamic')}
    summary=[]
    for v in ('static','dynamic'):
        rs=[r for r in rows if r['variant']==v]; times=[r['elapsed_ms'] for r in rs]
        s={'variant':v,'valid_trials':len(rs),'mean_ms':statistics.mean(times),'median_ms':statistics.median(times),'min_ms':min(times),'max_ms':max(times),'stddev_ms':statistics.stdev(times) if len(times)>1 else 0,'median_peak_rss_kb':statistics.median(r['peak_rss_kb'] for r in rs),'dynamic_vs_static_percent':100*(means['dynamic']/means['static']-1)}
        for col in ('txt_activation_us','csv_activation_us','processing_ms'):
            s['mean_'+col]=statistics.mean(r[col] for r in rs); s['median_'+col]=statistics.median(r[col] for r in rs)
        summary.append(s)
    write('summary.csv',list(summary[0]),summary)
    print(f"Results: {out}\nStatic mean {means['static']:.3f} ms; dynamic mean {means['dynamic']:.3f} ms; dynamic vs static {100*(means['dynamic']/means['static']-1):+.2f}%")
except (OSError,RuntimeError,ValueError) as e:
    (out/'error.txt').write_text(str(e)+'\n'); sys.exit(str(e))
finally:
    for n,content in original.items():
        f=path(active,n)
        if content is None: f.unlink(missing_ok=True)
        else:
            with tempfile.TemporaryDirectory(prefix='restore-plugin-') as restore_dir:
                saved=Path(restore_dir)/f.name
                saved.write_bytes(content[0]); saved.chmod(content[1])
                atomic_copy(saved, f)
