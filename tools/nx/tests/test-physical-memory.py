"""Exercise named physical-memory allocation unwinding with native pointer sizes."""
from pathlib import Path
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/physical-memory-tests'
output.mkdir(parents=True, exist_ok=True)
source = (root / 'src/universal/physicalmemory.cpp').read_text()
functions = []
for name in ('PMem_InitPhysicalMemory', 'PMem_BeginAllocInPrim', 'PMem_EndAllocInPrim',
             'PMem_FreeIndex', 'PMem_FreeInPrim', '_PMem_AllocNamed'):
    match = re.search(r'^\w[^\n]*\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    functions.append(source[match.start():end])
(output / 'physical-functions.inc').write_text('\n'.join(functions))
exe = output / 'physical-memory.exe'
subprocess.run([args.compiler, '-std=c++17', '-O2', '-I' + str(output),
                str(Path(__file__).with_name('physical-memory.cpp')), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
