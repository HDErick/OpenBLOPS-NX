"""Compile portable field codecs in both native ABIs; require identical bytes."""
from pathlib import Path
import argparse,subprocess,hashlib
from checkpoint_compiler import msvc32
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe')
p.add_argument('--msvc32',action='store_true')
a=p.parse_args();root=Path(__file__).resolve().parents[3];out=root/'build-nx/checkpoint-field-tests';out.mkdir(parents=True,exist_ok=True)
source=Path(__file__).with_name('checkpoint-fields.cpp');archive=root/'src/game_mp/checkpoint_archive.cpp';exe=out/'fields64.exe';wire=out/'fields64.bin'
subprocess.run([a.compiler,'-std=c++20','-O2','-I'+str(root/'src'),str(source),str(archive),'-o',str(exe)],check=True)
subprocess.run([str(exe),str(wire)],check=True)
if a.msvc32:
 cl,env=msvc32();exe=out/'fields32.exe';wire32=out/'fields32.bin'
 subprocess.run([cl,'/nologo','/std:c++20','/O2','/EHsc','/I'+str(root/'src'),str(source),str(archive),'/Fe:'+str(exe),'/Fo:'+str(out)+'\\'],env=env,check=True)
 subprocess.run([str(exe),str(wire32)],env=env,check=True)
 assert wire.read_bytes()==wire32.read_bytes(),'32/64-bit typed field archive mismatch'
 print('PASS: identical typed field, symbolic ID and animation representation across x86 and x64')
print('Field archive SHA256:',hashlib.sha256(wire.read_bytes()).hexdigest())
