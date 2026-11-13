"""Exercise cinematic routing and map teardown using the engine functions."""
from pathlib import Path
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/cinematic-lifecycle-tests'
output.mkdir(parents=True, exist_ok=True)
functions = []
for file, names in {
    'gfx_d3d/r_cinematic.cpp': (
        'R_Cinematic_IsNextReady_Internal', 'R_Cinematic_IsTargetSet',
        'R_Cinematic_IsFinished', 'R_Cinematic_IsStarted',
        'R_Cinematic_StartPlayback_Internal', 'R_Cinematic_StartPlayback',
        'R_Cinematic_SetNextPlayback', 'R_Cinematic_StartNextPlayback',
        'R_Cinematic_StopPlayback', 'R_Cinematic_StopPlayback_Now',
        'R_Cinematic_StartPlayback_Now', 'R_Cinematic_UpdateFrame_Core',
        '_UpdateFrameCallback', 'R_Cinematic_UpdateFrame', 'R_Cinematic_WaitForUpdateFrame'),
    'cgame_mp/cg_servercmds_mp.cpp': ('CG_ClearDeferred3DCinematic_SP', 'CG_StartDeferred3DCinematic_SP', 'CG_Start3DCinematic_SP'),
    'cgame_mp/cg_draw_mp.cpp': ('CG_Sp_UpdateCinematicPlayback',),
    'cgame_mp/cg_main_mp.cpp': ('CG_ShutdownOnceForAllClients',),
    'ui_mp/ui_main_mp.cpp': ('UI_Sp_SetCinematicTransitionPending', 'UI_Sp_IsCinematicTransitionPending', 'UI_Sp_DismissCinematic', 'UI_Project_Refresh'),
    'ui/ui_atoms.cpp': ('UI_DrawHandlePic',),
}.items():
    source = (root / 'src' / file).read_text()
    for name in names:
        match = re.search(r'^\w[^\n]*\b' + name + r'\([^;]*?\)\s*\{', source, re.M)
        assert match, name
        end = source.index('{', match.start()) + 1
        depth = 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        functions.append(source[match.start():end])
(output / 'lifecycle-functions.inc').write_text('\n'.join(functions))
for platform, defines in [('pc', []), ('nx', ['-DKISAK_NX'])]:
    exe = output / ('lifecycle-' + platform + '.exe')
    subprocess.run([args.compiler, '-std=c++17', '-O2', '-DKISAK_SP', *defines,
                    '-I' + str(output), str(Path(__file__).with_name('cinematic-lifecycle.cpp')),
                    '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
