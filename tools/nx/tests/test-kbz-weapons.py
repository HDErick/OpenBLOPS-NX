"""Check legacy weapon aliases against the engine's model-reference traversal."""
from pathlib import Path
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--zones', type=Path)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/kbz-weapon-tests'
output.mkdir(parents=True, exist_ok=True)
source = (root / 'src/database/db_load.cpp').read_text()
functions = []
for name in ('Mark_XModelPtr', 'Mark_XModelPtrArray'):
    match = re.search(r'^void [^\n]*\b' + name + r'\([^\n]*\)\s*\{', source, re.M)
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    functions.append(source[match.start():end])
(output / 'mark-models.inc').write_text('\n'.join(functions))
exe = output / 'kbz-weapons.exe'
subprocess.run([args.compiler, '-std=c++17', '-O2', '-Wall', '-Wextra',
                '-I' + str(output), str(Path(__file__).with_name('kbz-weapons.cpp')),
                str(root / 'src/nx/nx_kbz_storage.cpp'), '-o', str(exe)], check=True)
zones = sorted(args.zones.glob('*.kbz')) if args.zones else []
if args.zones:
    assert zones, 'no KBZ fixtures found'
subprocess.run([str(exe), *map(str, zones)], check=True)
