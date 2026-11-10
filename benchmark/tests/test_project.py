#!/usr/bin/env python3
import csv, os, shutil, statistics, subprocess, sys, tempfile
from pathlib import Path
B=Path(__file__).resolve().parents[1]; R=B
sys.path.insert(0, str(B/'tools'))
from plugins import install, remove
def run(args,**kw): return subprocess.run([str(x) for x in args],capture_output=True,text=True,**kw)
def require(condition, message):
    if not condition: raise AssertionError(message)
def transcript(proc):
    result=''
    while not result.endswith('> '):
        c=proc.stdout.read(1)
        if not c: raise AssertionError('demo terminated early: '+result)
        result+=c
    return result
def send(proc, command):
    proc.stdin.write(command+'\n'); proc.stdin.flush(); return transcript(proc)
with tempfile.TemporaryDirectory(prefix='analyzer-tests-') as td:
    temp=Path(td)
    dataset=temp/'datasets'
    subprocess.run(['python3',str(B/'tools/generate.py'),'--directory',str(dataset)],check=True,stdout=subprocess.DEVNULL)
    trace=temp/'trace';trace.mkdir()
    for n in ('txt','csv'): shutil.copy2(B/'plugins-built'/(n+'_plugin.so'),trace)
    env=dict(os.environ, LD_DEBUG='files')
    traced=run([B/'bin/benchmark-dynamic',dataset,trace],env=env)
    require(traced.returncode==0,traced.stderr)
    ti=traced.stderr.find('calling init: '+str(trace/'txt_plugin.so'))
    ci=traced.stderr.find('calling init: '+str(trace/'csv_plugin.so'))
    require(ti>=0 and ci>ti and traced.stderr.count('calling init: '+str(trace/'txt_plugin.so'))==1 and traced.stderr.count('calling init: '+str(trace/'csv_plugin.so'))==1,'plugin load order/count')
    require(all((dataset/e/f'{i:05d}.{e}').stat().st_size == 524288 for e in ('txt','csv') for i in range(10)), 'dataset size')
    empty=temp/'empty';empty.mkdir()
    x=run([B/'bin/benchmark-dynamic',dataset,empty]);require(x.returncode!=0 and 'Cannot load' in x.stderr,'missing plugin failure')
    nm=run(['nm','-u',B/'bin/benchmark-static']);require('dlopen' not in nm.stdout and 'dlsym' not in nm.stdout,'static dynamic dependency')
    defined=run(['nm','--defined-only',B/'bin/benchmark-static'])
    require('txt_analyze' in defined.stdout and 'csv_analyze' in defined.stdout and 'log_analyze' not in defined.stdout, 'benchmark analyzer set')
    cli=run(['python3',B/'tools/plugins.py','--target','benchmark','activate','log'])
    require(cli.returncode != 0, 'benchmark accepted LOG plugin')
    unrelated=temp/'unrelated'; unrelated.mkdir(); (unrelated/'keep.txt').write_text('keep')
    install(B/'plugins-built',unrelated,('txt','csv')); remove(unrelated,('txt','csv'))
    require((unrelated/'keep.txt').read_text()=='keep', 'plugin reset touched unrelated file')
    linked=temp/'linked'; linked.symlink_to(unrelated, target_is_directory=True)
    try:
        remove(linked,('txt',))
        raise AssertionError('symlink plugin directory accepted')
    except ValueError:
        pass
    benchmark_active=B/'plugins-active'
    def saved_state():
        return {n: ((benchmark_active/n).read_bytes(), (benchmark_active/n).stat().st_mode & 0o7777) if (benchmark_active/n).exists() else None for n in ('txt_plugin.so','csv_plugin.so')}
    before=saved_state()
    out=temp/'smoke';x=run(['python3',B/'tools/benchmark.py','--trials','2','--warmups','1','--dataset',dataset,'--output',out]);require(x.returncode==0,x.stderr)
    require(saved_state()==before, 'benchmark did not restore active plugins')
    with (out/'raw.csv').open() as f: raw=list(csv.DictReader(f))
    with (out/'warmups.csv').open() as f: warm=list(csv.DictReader(f))
    with (out/'summary.csv').open() as f: summary=list(csv.DictReader(f))
    require(len(raw)==4 and len(warm)==2 and len({x['pid'] for x in raw+warm})==6,'trial or process count')
    require([r['variant'] for r in raw]==['static','static','dynamic','dynamic'],'batch order')
    require([r['variant'] for r in warm]==['static','dynamic'],'warm-up order')
    require([r['trial'] for r in raw]==['1','2','1','2'],'batch trial numbering')
    import json
    metadata=json.loads((out/'metadata.json').read_text())
    require('static batch then dynamic batch' in metadata['order'] and '-O0' in metadata['flags'], 'batch metadata')
    require(len({r['report_sha256'] for r in raw+warm})==1,'report equality')
    for s in summary:
        samples=[float(r['elapsed_ms']) for r in raw if r['variant']==s['variant']]
        require(int(s['valid_trials'])==2 and abs(float(s['mean_ms'])-statistics.mean(samples))<1e-8,'summary mismatch')
    alt=temp/'alternate'; x=run(['python3',B/'tools/benchmark.py','--trials','2','--warmups','1','--order','alternating','--dataset',dataset,'--output',alt]);require(x.returncode==0,x.stderr)
    with (alt/'raw.csv').open() as f: alternate=list(csv.DictReader(f))
    require([r['variant'] for r in alternate]==['dynamic','static','static','dynamic'],'alternating order')
    x=run(['python3',B/'tools/benchmark.py','--trials','1','--warmups','0','--dataset',dataset,'--output',out]);require(x.returncode!=0,'overwrote results')
print('Benchmark loading, failure path, report and smoke-statistics tests passed')
