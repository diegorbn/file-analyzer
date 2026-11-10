#!/usr/bin/env python3
import csv, os, shutil, statistics, subprocess, sys, tempfile
from pathlib import Path
R=Path(__file__).resolve().parents[1]; B=R/'benchmark'; D=R/'demo'
sys.path.insert(0, str(R/'tools'))
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
    temp=Path(td); active=temp/'active';active.mkdir(); built=D/'plugins-built'
    for v in ('static','dynamic'):
        p=run([D/'bin'/('demo-'+v),active],input='analyze '+str(R/'example/ejemplo.txt')+'\nanalyze '+str(R/'example/ejemplo.csv')+'\nanalyze '+str(R/'example/ejemplo.log')+'\nquit\n')
        require(p.returncode==0, p.stderr)
        if v=='static': require('No analyzer' not in p.stdout and 'Analysis failed' not in p.stdout, p.stdout)
        else: require(p.stdout.count('No analyzer')==3,p.stdout)
    p=subprocess.Popen([D/'bin/demo-dynamic',str(active)],stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,bufsize=1)
    try:
        transcript(p); pid=p.pid
        require('No analyzers' in send(p,'list'),'empty registry')
        shutil.copy2(built/'txt_plugin.so',active); send(p,'refresh'); send(p,'refresh')
        require(send(p,'list').count('.txt')==1,'duplicate load')
        txt=send(p,'analyze '+str(R/'example/ejemplo.txt')); require('Líneas' in txt,txt)
        old_inode=(active/'txt_plugin.so').stat().st_ino
        install(built,active,('txt',))
        require((active/'txt_plugin.so').stat().st_ino != old_inode, 'plugin replacement was not atomic')
        require('Líneas' in send(p,'analyze '+str(R/'example/ejemplo.txt')), 'replacement affected loaded plugin')
        shutil.copy2(built/'csv_plugin.so',active);send(p,'refresh')
        require('.csv' in send(p,'list'),'CSV load')
        require('Filas' in send(p,'analyze '+str(R/'example/ejemplo.csv')),'CSV analysis')
        (active/'txt_plugin.so').unlink()
        require('Líneas' in send(p,'analyze '+str(R/'example/ejemplo.txt')),'mapped plugin after removal')
        send(p,'unload .txt'); require('No analyzer' in send(p,'analyze '+str(R/'example/ejemplo.txt')),'unload')
        shutil.copy2(built/'txt_plugin.so',active);send(p,'refresh');require('.txt' in send(p,'list'),'reload')
        require(p.pid==pid and p.poll() is None,'process changed')
        p.stdin.write('quit\n');p.stdin.flush();require(p.wait(timeout=5)==0,'demo exit')
    finally:
        if p.poll() is None: p.kill();p.wait()
        p.stdin.close();p.stdout.close();p.stderr.close()
    # Compare complete TXT/CSV/LOG reports from the two interfaces.
    shutil.copy2(built/'log_plugin.so',active)
    script=''.join('analyze '+str(R/'example'/('ejemplo.'+e))+'\n' for e in ('txt','csv','log'))+'quit\n'
    left=run([D/'bin/demo-static',active],input=script)
    right=run([D/'bin/demo-dynamic',active],input=script)
    require(left.stdout==right.stdout, 'demo analyzer output mismatch')
    (active/'bad.so').write_text('invalid')
    x=run([D/'bin/demo-dynamic',active],input='list\nquit\n');require(x.returncode==0 and 'Cannot load' in x.stderr,'invalid .so')
    (active/'bad.so').unlink()
    src=temp/'bad.c';src.write_text('int unrelated(void) { return 0; }\n')
    subprocess.run(['gcc','-shared','-fPIC',str(src),'-o',str(active/'bad.so')],check=True)
    x=run([D/'bin/demo-dynamic',active],input='quit\n');require(x.returncode==0 and 'missing plugin_name' in x.stderr,'missing symbols')
    for names in (('txt',),('csv',),('txt','csv'),('txt','csv','log')):
        d=temp/('config'+str(len(names))+names[0]);d.mkdir()
        for n in names: shutil.copy2(built/(n+'_plugin.so'),d)
        x=run([D/'bin/demo-dynamic',d],input='list\nquit\n');require(x.returncode==0 and all('.'+n in x.stdout for n in names),'startup config')
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
    cli=run(['python3',R/'tools/plugins.py','--target','benchmark','activate','log'])
    require(cli.returncode != 0, 'benchmark accepted LOG plugin')
    unrelated=temp/'unrelated'; unrelated.mkdir(); (unrelated/'keep.txt').write_text('keep')
    install(built,unrelated,('txt','csv')); remove(unrelated,('txt','csv'))
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
    require([r['variant'] for r in raw]==['dynamic','static','static','dynamic'],'alternation')
    require(len({r['report_sha256'] for r in raw+warm})==1,'report equality')
    for s in summary:
        samples=[float(r['elapsed_ms']) for r in raw if r['variant']==s['variant']]
        require(int(s['valid_trials'])==2 and abs(float(s['mean_ms'])-statistics.mean(samples))<1e-8,'summary mismatch')
    x=run(['python3',B/'tools/benchmark.py','--trials','1','--warmups','0','--dataset',dataset,'--output',out]);require(x.returncode!=0,'overwrote results')
print('Functional, live loading, failure path, report and smoke-statistics tests passed')
