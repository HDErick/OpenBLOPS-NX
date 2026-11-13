"""Run the actual checkpoint VM codec on 64-bit and 32-bit hosts; compare bytes."""
from pathlib import Path
import argparse,subprocess,os,shutil,json,hashlib
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
p.add_argument('--msvc32',action='store_true',help='also test the actual 32-bit PC ABI')
a=p.parse_args();root=Path(__file__).resolve().parents[3]
out=root/'build-nx/checkpoint-script-tests';out.mkdir(parents=True,exist_ok=True)
parts=[]
for name in ['cscr_save_write.cpp','cscr_save.cpp']:
 source=(root/'src/clientscript'/name).read_text()
 parts.append('\n'.join(l for l in source.splitlines() if not l.startswith('#include')))
(out/'checkpoint-vm-functions.inc').write_text('\n'.join(parts))
source=Path(__file__).with_name('checkpoint-script-vm.cpp');exe=out/'vm64.exe';wire=out/'vm64.bin'
subprocess.run([a.compiler,'-std=c++20','-O2','-DKISAK_SP','-I'+str(out),str(source),'-o',str(exe)],check=True)
subprocess.run([str(exe),str(wire)],check=True)
if a.msvc32:
 # Run this option with Windows Python; use MSVC's installed x86 environment.
 vswhere=Path(os.environ.get('ProgramFiles(x86)','C:/Program Files (x86)'))/'Microsoft Visual Studio/Installer/vswhere.exe'
 install=subprocess.check_output([str(vswhere),'-latest','-products','*','-property','installationPath'],text=True).strip()
 vcvars=Path(install)/'VC/Auxiliary/Build/vcvars32.bat'
 output=subprocess.check_output(f'call "{vcvars}" >nul && set',shell=True,text=True,errors='replace')
 env={}
 for line in output.splitlines():
  if '=' in line:
   key,value=line.split('=',1)
   if key:env[key]=value
 native_path=next(v for k,v in env.items() if k.upper()=='PATH')
 cl=next((str(Path(d.replace('\\','/'))/'cl.exe') for d in native_path.split(';') if (Path(d.replace('\\','/'))/'cl.exe').is_file()),None)
 assert cl,'MSVC x86 compiler unavailable'
 exe32=out/'vm32.exe';wire32=out/'vm32.bin'
 subprocess.run([cl,'/nologo','/std:c++20','/O2','/EHsc','/DKISAK_SP','/I'+str(out),str(source),'/Fe:'+str(exe32),'/Fo:'+str(out/'vm32.obj')],env=env,check=True)
 subprocess.run([str(exe32),str(wire32)],env=env,check=True)
 assert wire.read_bytes()==wire32.read_bytes(),'32/64-bit VM archive mismatch'
 print('PASS: identical VM archive bytes across PC x86 and host x64')
print('VM archive SHA256:',hashlib.sha256(wire.read_bytes()).hexdigest())
