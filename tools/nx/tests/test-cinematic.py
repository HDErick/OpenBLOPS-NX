"""Exercise the engine cinematic advance and texture upload code with a fake device."""
import argparse
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root/'build-nx/cinematic-tests'
include = output/'include/binklib'
include.mkdir(parents=True, exist_ok=True)
(include/'bink.h').write_text('''#define __RADLINUX__
#define __int64 long long
#define __RADWIN__
#define __RADNT__
#define __RAD32__
#define __RAD64__
#define __RADX64__
#include_next <binklib/bink.h>
''')
source = (root/'src/gfx_d3d/r_cinematic.cpp').read_text()
functions = []
for name in ('R_Cinematic_UploadPlane', 'Unlock_Bink_textures2', 'Unlock_Bink_textures_Callback',
             'R_Cinematic_FailPlayback', 'R_Cinematic_Advance'):
    match = re.search(r'^\w[^\n]*\b'+name+r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    functions.append(source[match.start():end])
(output/'cinematic-functions.inc').write_text('\n'.join(functions))
exe = output/'cinematic.exe'
subprocess.run([args.compiler, '-std=c++17', '-O2', '-Wno-attributes', '-DKISAK_NX',
                '-I'+str(include.parent), '-I'+str(root/'src'), '-I'+str(output),
                str(root/'tools/nx/tests/cinematic.cpp'), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
print('Cinematic completion, looping, pause, bounded catch-up, pitched uploads and failures passed')
(include.parent/'switch.h').write_text('''#include <cstdint>
enum AppletFocusState { AppletFocusState_InFocus, AppletFocusState_OutOfFocus };
enum AppletHookType { AppletHookType_OnFocusState, AppletHookType_OnResume };
struct AppletHookCookie {};
uint64_t armGetSystemTick();
uint64_t armTicksToNs(uint64_t);
AppletFocusState appletGetFocusState();
void appletHook(AppletHookCookie *, void (*)(AppletHookType, void *), void *);
''')
clock = output/'bink-clock.exe'
subprocess.run([args.compiler, '-std=c++17', '-O2', '-I'+str(include.parent), '-I'+str(root/'src/nx'),
                str(root/'tools/nx/tests/bink-clock.cpp'), str(root/'src/nx/nx_bink_clock.cpp'),
                '-o', str(clock)], check=True)
subprocess.run([str(clock)], check=True)
print('Horizon focus loss, repeated events and resume preserve the cinematic clock')
