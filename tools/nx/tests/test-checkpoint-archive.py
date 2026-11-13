"""Exercise the real portable checkpoint container and transactional store."""
from pathlib import Path
import argparse,subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
a=p.parse_args();root=Path(__file__).resolve().parents[3]
out=root/'build-nx/checkpoint-archive-tests';out.mkdir(parents=True,exist_ok=True)
exe=out/'archive.exe'
subprocess.run([a.compiler,'-std=c++20','-O2','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-I'+str(root/'src'),str(Path(__file__).with_name('checkpoint-archive.cpp')),str(root/'src/game_mp/checkpoint_archive.cpp'),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
