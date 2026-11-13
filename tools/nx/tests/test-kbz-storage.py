"""Exercise the zone reader and optionally validate installed KBZ files."""
from pathlib import Path
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
parser.add_argument('--zones', type=Path)
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root/'build-nx/kbz-storage-tests'
output.mkdir(parents=True, exist_ok=True)
exe = output/'kbz-storage.exe'

def function(source, name):
    match = re.search(r'^void [^\n]*\b'+name+r'\([^\n]*\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start())+1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]+'\n'

header = (root/'src/database/db_registry.h').read_text()
types = '\n'.join(re.search(r'struct '+name+r'\b.*?\n};', header, re.S).group()
                  for name in ('XBlock', 'XZone'))
(output/'zone-types.inc').write_text(types)
registry = (root/'src/database/db_registry.cpp').read_text()
loader = (root/'src/nx/nx_kbz.cpp').read_text()
(output/'zone-unload.inc').write_text(function(loader, 'NX_FreeKbz')
    +function(registry, 'DB_UnloadXZoneMemory')+function(registry, 'DB_UnloadXAssetsMemory'))
subprocess.run([args.compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror',
                '-DKISAK_NX', '-I'+str(output),
                str(Path(__file__).with_name('kbz-storage.cpp')), '-o', str(exe)], check=True)
zones = sorted(args.zones.glob('*.kbz')) if args.zones else []
subprocess.run([str(exe), str(output), *map(str, zones)], check=True)
if args.zones:
    assert zones, 'no retail KBZ files found'
    print(f'Validated {len(zones)} retail KBZ files')
