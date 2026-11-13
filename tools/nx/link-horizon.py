"""Translate MSYS paths inside response files for native devkitA64 tools."""
from pathlib import Path
import re
import shlex
import subprocess
import sys

msys_root=subprocess.check_output(['/usr/bin/cygpath','-m','/'],text=True).strip().rstrip('/')
mounts=[]
for line in subprocess.check_output(['/usr/bin/mount'],text=True).splitlines():
    found=re.match(r'^(.+?) on (/\S*) type ',line)
    if found and found[2] != '/' and not re.match(r'^/[a-zA-Z]$',found[2]):
        mounts.append((found[2].rstrip('/'),found[1].rstrip('/')))
mounts.sort(key=lambda mount: len(mount[0]),reverse=True)
def winpath(path):
    drive=re.match(r'^/([a-zA-Z])(?:/|$)',path)
    if drive:
        return drive[1].upper()+':'+path[2:]
    for point,target in mounts:
        if path==point or path.startswith(point+'/'):
            return target+path[len(point):]
    return msys_root+path

def normalize(arg):
    if arg.startswith('/'):
        return winpath(arg)
    for prefix in ('-L','-I','-specs=','--sysroot='):
        if arg.startswith(prefix+'/'):
            return prefix+winpath(arg[len(prefix):])
    if arg.startswith('-Wl,'):
        return ','.join(winpath(part) if part.startswith('/') else part for part in arg.split(','))
    return arg

args=[]
for arg in sys.argv[2:]:
    if arg.startswith('@'):
        original=Path(arg[1:])
        translated=original.with_suffix(original.suffix+'.windows')
        words=[normalize(word) for word in shlex.split(original.read_text())]
        translated.write_text('\n'.join('"'+word.replace('\\','\\\\').replace('"','\\"')+'"' for word in words)+'\n')
        args.append('@'+str(translated))
    else:
        args.append(normalize(arg))
raise SystemExit(subprocess.call([sys.argv[1],*args]))
