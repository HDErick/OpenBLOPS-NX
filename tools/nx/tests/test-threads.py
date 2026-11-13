"""Test DXVK worker placement and permission failures against mocked Horizon calls."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
tests = Path(__file__).resolve().parent
output = root / 'build-nx/thread-tests'
output.mkdir(parents=True, exist_ok=True)
exe = output / 'threads.exe'
subprocess.run([args.compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', '-pthread',
    '-I' + str(tests / 'thread-shim'), str(tests / 'threads.cpp'),
    str(root / 'src/nx/dxvk/horizon_threads.cpp'), '-o', str(exe)], check=True)
for scenario in ('three', 'four', 'no-priority-63', 'core-query-failure', 'priority-query-failure',
    'affinity-failure', 'priority-failure', 'get-priority-failure'):
    subprocess.run([str(exe), scenario], check=True, timeout=10)
