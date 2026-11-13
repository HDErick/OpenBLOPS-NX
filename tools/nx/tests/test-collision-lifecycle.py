"""Exercise native collision-map reset and subsequent asset traversal."""
from pathlib import Path
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/collision-lifecycle-tests'
output.mkdir(parents=True, exist_ok=True)


def extract(source, pattern, struct=False):
    match = re.search(pattern, source, re.M)
    assert match, pattern
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end + int(struct)]


header = (root / 'src/qcommon/cm_trace.h').read_text()
declarations = [extract(header, r'^struct[^\n]*\b' + name + r'\b[^\n]*\n\{', True)
                for name in ('cLeaf_s', 'cmodel_t', 'clipMap_t')]
(output / 'collision-types.inc').write_text('\n'.join(declarations))
functions = []
for file, names in {
    'qcommon/cm_load.cpp': ('CM_Shutdown', 'CM_Unload'),
    'qcommon/cm_load_obj.cpp': ('CM_LoadMapFromBsp',),
    'database/db_load.cpp': ('Mark_MapEntsPtr', 'Mark_clipMap_t'),
}.items():
    source = (root / 'src' / file).read_text()
    for name in names:
        functions.append(extract(source, r'^void[^\n]*\b' + name + r'\([^;]*?\)\s*\{'))
(output / 'collision-functions.inc').write_text('\n'.join(functions))
exe = output / 'collision-lifecycle.exe'
subprocess.run([args.compiler, '-std=c++17', '-O2', '-Wall', '-Wextra',
                '-I' + str(output), str(Path(__file__).with_name('collision-lifecycle.cpp')),
                '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
