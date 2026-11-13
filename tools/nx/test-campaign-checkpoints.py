"""One-command checkpoint regressions, optional builds, and opt-in isolated PC probe.
Run with native Windows Python for the x86 ABI comparison and runtime mode.
No game is launched unless --runtime is explicitly supplied.
"""
from pathlib import Path
import argparse,datetime,json,os,re,subprocess,sys,time,uuid
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe')
p.add_argument('--build',action='store_true',help='build PC SP and Switch SP NRO')
p.add_argument('--runtime',action='store_true',help='launch isolated PC campaign probes, then quit')
p.add_argument('--save-cycle',action='store_true',help='verify the actual checkpoint prepare/commit lifecycle and death recovery')
p.add_argument('--restore',action='store_true',help='kill the player and restore an in-process checkpoint, then verify continued simulation')
p.add_argument('--death-restart',action='store_true',help='after a diagnostic capture, kill the player and verify full-map death recovery')
p.add_argument('--map',default='vorkuta')
p.add_argument('--maps',nargs='+',help='run consecutive isolated campaign probes on these maps')
p.add_argument('--probe-count',type=int,default=1,help='capture every ten simulated seconds, 1 to 10 times per map')
p.add_argument('--restore-count',type=int,default=1,help='restore the same checkpoint after this many consecutive deaths, 1 to 5')
p.add_argument('--timeout',type=int,default=240)
p.add_argument('--sp-build',default='build-nx/pc-splitscreen-sp')
p.add_argument('--nx-build',default='build-nx/sarek-sp')
a=p.parse_args();root=Path(__file__).resolve().parents[2]
maps=a.maps or [a.map]
if any(not re.fullmatch('[a-z0-9_]+',name) or name.startswith('zombie') or name in ['frontend','zombietron'] for name in maps):
 p.error('--map must name a campaign map')
if not 1<=a.probe_count<=10:p.error('--probe-count must be between 1 and 10')
if a.save_cycle and (a.restore or a.death_restart or a.probe_count!=1):p.error('--save-cycle requires a single probe and no other death test')
if a.restore and (a.death_restart or a.probe_count!=1):p.error('--restore requires one capture and cannot be combined with --death-restart')
if a.death_restart and a.probe_count!=1:p.error('--death-restart uses one probe before and one after restart; leave --probe-count at 1')
if not 1<=a.restore_count<=5:p.error('--restore-count must be between 1 and 5')
if a.restore_count!=1 and not(a.restore or a.save_cycle):p.error('--restore-count requires --restore or --save-cycle')
if len(set(maps))!=len(maps):p.error('--maps must not contain duplicates')
if not 20<=a.timeout<=1800:p.error('--timeout must be between 20 and 1800 seconds')
stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')+'-'+uuid.uuid4().hex[:6]
out=root/'build-nx/checkpoint-tests'/stamp;out.mkdir(parents=True,exist_ok=False)
env=os.environ.copy();env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
report={'schema':1,'scope':'checkpoint-regressions','restoration_implemented':True,'checks':[],'runtime':{'status':'not_requested'},'runtime_runs':[]}
def persist():
 (out/'result.json').write_text(json.dumps(report,indent=2))
 (out.parent/'latest.json').write_text(json.dumps({'result':str(out/'result.json')},indent=2))
def check(name,cmd,workdir=root):
 print('RUN:',name,flush=True);t=time.monotonic()
 result=subprocess.run(cmd,cwd=workdir,env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,errors='replace')
 (out/(name+'.log')).write_text(result.stdout)
 report['checks'].append({'name':name,'passed':result.returncode==0,'seconds':round(time.monotonic()-t,2),'log':str(out/(name+'.log'))})
 persist();print(result.stdout[-3500:],flush=True)
 if result.returncode:raise RuntimeError(name+' failed (see its log)')
def runtime(mapname):
 if sys.platform!='win32':raise RuntimeError('--runtime requires native Windows Python')
 # Do not disturb an existing user-owned game instance.
 existing=subprocess.run(['powershell.exe','-NoProfile','-Command','Get-Process KisakBlack -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Id'],capture_output=True,text=True)
 if existing.stdout.strip():raise RuntimeError('A KisakBlack instance is already running; runtime probe was not launched')
 build=(root/a.sp_build).resolve();exe=build/'Release/KisakBlack.exe'
 if not exe.is_file():raise RuntimeError('PC SP executable missing; use --build')
 home=out/mapname/'home';(home/'players').mkdir(parents=True)
 command=[str(exe),'+set','fs_basepath',str(exe.parent),'+set','fs_homepath',str(home),
  '+set','r_fullscreen','0','+set','r_mode','1280x720','+set','logfile','2',
  '+set','checkpoint_probe_auto',str(int(not a.save_cycle)),'+set','checkpoint_probe_exit',str(int(not a.save_cycle)),
  '+set','campaign_checkpoints',str(int(a.save_cycle)),'+set','checkpoint_test_save_cycle',str(int(a.save_cycle)),
  '+set','checkpoint_probe_count',str(a.probe_count),'+set','checkpoint_test_restore_count',str(a.restore_count),
  '+set','checkpoint_test_restore',str(int(a.restore)),
  '+set','checkpoint_test_death',str(int(a.death_restart)),'+spdevmap' if a.death_restart or a.restore or a.save_cycle else '+spmap',mapname]
 report['runtime']={'status':'running','scope':'checkpoint-restore' if a.restore or a.save_cycle else 'live-checkpoint-capture','map':mapname,'home':str(home)};persist()
 report['runtime_runs'].append(report['runtime'])
 with (out/mapname/'process-output.log').open('w') as stream:
  process=subprocess.Popen(command,cwd=exe.parent,env=env,stdout=stream,stderr=subprocess.STDOUT)
  report['runtime']['pid']=process.pid;persist();deadline=time.monotonic()+a.timeout
  candidates=[home/'main'/f'console_mp_{process.pid}.log',exe.parent/'main'/f'console_mp_{process.pid}.log']
  try:
   while process.poll() is None and time.monotonic()<deadline:
    time.sleep(1)
    log=next((file for file in candidates if file.is_file()),None)
    if log and ('Com_ERROR:' in log.read_text(errors='replace') or (exe.parent/f'assert_{process.pid}.log').is_file()):
     process.terminate();process.wait(timeout=20);break
   if process.poll() is None:
    process.terminate();process.wait(timeout=20)
    raise RuntimeError('runtime probe timed out; only the test-owned process was stopped')
   report['runtime']['exit_code']=process.returncode
   log=next((file for file in candidates if file.is_file()),None)
   if not log:raise RuntimeError('runtime console log not found')
   report['runtime']['console_log']=str(log)
   text=log.read_text(errors='replace');matches=re.findall(r'CHECKPOINT_PROBE (\{[^\r\n]+\})',text)
   if not matches:raise RuntimeError('game exited without a checkpoint probe result')
   results=[json.loads(match) for match in matches];report['runtime']['probe']=results[-1];report['runtime']['probes']=results
   if len(results)!=(a.probe_count+int(a.death_restart)):raise RuntimeError('wrong number of checkpoint probes')
   if any(not result.get('ok') for result in results):raise RuntimeError('a live probe failed; see console log')
   if len({(result['map_identity'],result['script_identity'],result['script_sources']) for result in results})!=1:raise RuntimeError('map/script identity changed across unchanged mission capture/reload')
   if not a.death_restart and any(results[i]['time']<=results[i-1]['time'] for i in range(1,len(results))):raise RuntimeError('probe clock did not advance')
   for result in results:
    if result.get('restorable') or result.get('objects',0)<=0 or result.get('section_mask')!=63 or result.get('entities',0)<=0 or result.get('players',0)<=0 or result.get('sentients',0)<=0 or result.get('dobjs',0)<=0 or result.get('animation_nodes',0)<=0:
     raise RuntimeError('live record capture failed or incomplete checkpoint was incorrectly marked restorable')
   if a.restore or a.save_cycle:
    if 'script runtime error' in text.split('CHECKPOINT_RESTORE {',1)[-1]:raise RuntimeError('restored mission produced script runtime errors')
    events=[json.loads(value) for value in re.findall(r'CHECKPOINT_RESTORE (\{[^\r\n]+\})',text)];report['runtime']['restore_events']=events
    if [event['stage'] for event in events]!=['applied','continued']*a.restore_count or not all(event.get('ok') for event in events):raise RuntimeError('checkpoint restore did not apply and continue')
    if any(event.get('origin_error',1)>0.01 for event in events if event['stage']=='applied'):raise RuntimeError('saved player position was not restored')
    if any(event.get('animated_actors',0)<=0 or event.get('unposed_actors',1)!=0 for event in events if event['stage']=='continued'):raise RuntimeError('restored actors have no active rendered animation pose')
    if a.save_cycle:
     saves=[json.loads(value) for value in re.findall(r'CHECKPOINT_SAVE (\{[^\r\n]+\})',text)];report['runtime']['save_events']=saves
     if [event['stage'] for event in saves]!=['requested','prepared','committed'] or len({event['id'] for event in saves})!=1 or not saves[1].get('ok'):raise RuntimeError('prepare/commit lifecycle failed')
    if len({event['generation'] for event in events})!=1:raise RuntimeError('restore reloaded the map')
    if any(events[i]['time']!=results[0]['time'] or events[i+1]['time']<events[i]['time']+5000 for i in range(0,len(events),2)):raise RuntimeError('restore did not rewind and resume simulation')
   if a.death_restart:
    events=[json.loads(value) for value in re.findall(r'CHECKPOINT_DEATH (\{[^\r\n]+\})',text)];report['runtime']['death_events']=events
    if [event['stage'] for event in events]!=['killed','map_reinitialized','recovered'] or events[-1].get('health',0)<=0 or events[-1].get('reloading')!=0:
     raise RuntimeError('ordinary campaign death did not recover to a living player on the same map')
   if process.returncode!=0:raise RuntimeError('game did not exit cleanly after probe')
   report['runtime']['status']='passed';print('PASS:',mapname,'checkpoint prepare/commit/death/restore' if a.save_cycle else 'death/checkpoint restore' if a.restore else 'capture and death/restart' if a.death_restart else 'capture validation',flush=True)
  finally:
   if process.poll() is None:process.terminate();process.wait(timeout=20)
   persist()
try:
 check('actor-targets',[sys.executable,str(root/'tools/nx/tests/test-actor-targets.py'),'--compiler',a.compiler])
 check('checkpoint-vehicles',[sys.executable,str(root/'tools/nx/tests/test-checkpoint-vehicles.py'),'--compiler',a.compiler])
 check('campaign-scenes',[sys.executable,str(root/'tools/nx/tests/test-campaign-scenes.py'),'--compiler',a.compiler])
 vm=[sys.executable,str(root/'tools/nx/tests/test-checkpoint-script-vm.py'),'--compiler',a.compiler]
 if sys.platform=='win32':vm.append('--msvc32')
 check('script-vm',vm)
 fields=[sys.executable,str(root/'tools/nx/tests/test-checkpoint-fields.py'),'--compiler',a.compiler]
 if sys.platform=='win32':fields.append('--msvc32')
 check('typed-record-fields',fields)
 for name in ['test-checkpoint-archive','test-campaign-restart-progress']:
  check(name,[sys.executable,str(root/'tools/nx/tests'/f'{name}.py'),'--compiler',a.compiler])
 if a.build:
  cache=(root/a.sp_build/'CMakeCache.txt').read_text()
  cmake=next(l.split('=',1)[1] for l in cache.splitlines() if l.startswith('CMAKE_COMMAND:INTERNAL='))
  check('build-pc',[cmake,'--build',str(root/a.sp_build),'--config','Release','--parallel','4'])
  bash='C:/devkitPro/msys2/usr/bin/bash.exe'
  nx=(root/a.nx_build).resolve().as_posix();nx='/'+nx[0].lower()+nx[2:]
  check('build-nx',[bash,'-lc',f'set -e; export DEVKITPRO=/opt/devkitpro; export DEVKITA64=$DEVKITPRO/devkitA64; export PATH=$DEVKITA64/bin:$DEVKITPRO/tools/bin:/usr/bin:$PATH; cmake --build "{nx}" -j 6'])
 if a.runtime:
  for mapname in maps:runtime(mapname)
 report['passed']=True;persist()
 print('RESULT:',out/'result.json')
except Exception as error:
 report['passed']=False;report['error']=str(error)
 if report['runtime']['status']=='running':report['runtime']['status']='failed'
 persist();print('FAIL:',error,file=sys.stderr);print('RESULT:',out/'result.json');sys.exit(1)
