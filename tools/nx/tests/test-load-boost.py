"""Test the loading boost controller with a libnx service double."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
tests = root/'tools/nx/tests'
output = root/'build-nx/load-boost-tests'
output.mkdir(parents=True, exist_ok=True)
exe = output/'load-boost.exe'
subprocess.run([args.compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                '-I'+str(tests/'load-boost-shim'), str(tests/'load-boost.cpp'),
                str(root/'src/nx/nx_load_boost.cpp'), '-o', str(exe)], check=True)
for scenario in ('lifecycle', 'focus', 'restore-retry', 'exit', 'unsupported', 'denied'):
    subprocess.run([str(exe), scenario], check=True)
