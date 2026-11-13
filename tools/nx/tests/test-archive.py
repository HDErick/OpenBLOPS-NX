"""Exercise the real unzip implementation with concurrent independent readers."""
from pathlib import Path
import argparse
import random
import re
import subprocess
import zipfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root/'build-nx/archive-tests'
include = output/'include'
for name, contents in {
    'universal/q_shared.h': '#include <cassert>\n#include <cstring>\n#define iassert assert\n#define Com_Memcpy memcpy\n',
    'zlib/zlib.h': '#include <zlib.h>\n',
    'universal/com_fileaccess.h': '''#include <cstdio>
FILE *FS_FileOpenReadBinary(const char *);
void FS_FileClose(FILE *);
unsigned FS_FileRead(void *, unsigned, FILE *);
int FS_FileSeek(FILE *, int, int);
int FS_FileGetFileSize(FILE *);
''',
}.items():
    path = include/name
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(contents)
data = random.Random(4711).randbytes(1048576)
(output/'payload.bin').write_bytes(data)
with zipfile.ZipFile(output/'stream.iwd', 'w') as archive:
    archive.writestr('stored', data, compress_type=zipfile.ZIP_STORED)
    archive.writestr('deflated', data, compress_type=zipfile.ZIP_DEFLATED)
exe = output/'archive.exe'
header = (root/'src/universal/com_files.h').read_text()
(output/'file-types.inc').write_text('\n'.join(re.search(r'(?:struct|union) '+name+r'\b.*?\n};', header, re.S).group()
    for name in ('qfile_gus', 'qfile_us', 'fileHandleData_t')))
source = (root/'src/universal/com_files.cpp').read_text()
functions = []
for name in ('FS_CheckFileSystemStarted', 'FS_FileForHandle', 'FS_Write', 'FS_Seek', 'FS_FCloseFile',
             'FS_GetHandleAndOpenFile', 'FS_FOpenTextFileWrite', 'FS_FOpenFileAppend', 'FS_Shutdown'):
    match = re.search(r'^\w[^\n]*\b'+name+r'\([^\n]*\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start())+1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    functions.append(source[match.start():end])
(output/'file-functions.inc').write_text('\n'.join(functions))
subprocess.run([args.compiler, '-std=c++17', '-O2', '-Wall', '-Wextra', '-DKISAK_NX', '-DQDECL=',
                '-I'+str(include), '-I'+str(output), str(Path(__file__).with_name('archive.cpp')),
                str(root/'src/qcommon/unzip.cpp'), '-lz', '-o', str(exe)], check=True)
subprocess.run([str(exe), str(output)], check=True)
