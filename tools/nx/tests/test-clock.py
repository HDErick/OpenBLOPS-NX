"""Test GPU clock ownership and interaction with the loading boost controller."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
tests = root/'tools/nx/tests'
output = root/'build-nx/clock-tests'
output.mkdir(parents=True, exist_ok=True)
exe = output/'clock.exe'
subprocess.run([args.compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                '-I'+str(tests/'clock-shim'), str(tests/'clock.cpp'),
                str(root/'src/nx/nx_clock.cpp'), str(root/'src/nx/nx_load_boost.cpp'),
                '-o', str(exe)], check=True)
for scenario in ('modern', 'legacy', 'denied', 'set-failure'):
    subprocess.run([str(exe), scenario], check=True)
