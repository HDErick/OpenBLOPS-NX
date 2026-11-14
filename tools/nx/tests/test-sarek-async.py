"""Exercise Sarek's async queue and shutdown with a blocked pipeline compiler."""
from pathlib import Path
import re
import subprocess

root = Path(__file__).resolve().parents[3]
source = root / 'build-nx/deps/dxvk-sarek-1.14.1/src/dxvk'
output = root / 'build-nx/sarek-work/async-tests'
output.mkdir(parents=True, exist_ok=True)

for name in ('dxvk_asynccompiler.h', 'dxvk_asynccompiler.cpp'):
    text = (source / name).read_text()
    text = re.sub(r'^#include "[^"]+"\s*$', '', text, flags=re.M)
    (output / name).write_text(text)

manager = (source / 'dxvk_pipemanager.cpp').read_text()
shutdown = re.search(r'  void DxvkPipelineManager::stopWorkerThreads\(\)(?: const)? \{.*?\n  \}',
                     manager, re.S).group()
destructor = re.search(r'  DxvkPipelineManager::~DxvkPipelineManager\(\)(?: = default;| \{.*?\n  \})',
                       manager, re.S).group()
(output / 'manager-shutdown.inc').write_text(shutdown + '\n' + destructor)

exe = output / 'sarek-async.exe'
subprocess.run(['/ucrt64/bin/g++.exe', '-std=c++20', '-O2', '-Wall', '-Wextra', '-pthread',
                '-I' + str(output), str(Path(__file__).with_name('sarek-async.cpp')),
                '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True, timeout=20)
