"""Exercise shared SP death-fade reset and movie dismissal across repeated map loads."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/screen-fade-tests'
output.mkdir(parents=True, exist_ok=True)
draw = (root / 'src/cgame_mp/cg_draw_mp.cpp').read_text()
fade = draw[draw.index('struct CG_ScreenFadeState_SP'):draw.index('\n#endif', draw.index('void CG_DrawScreenFade_SP'))]
ui = (root / 'src/ui_mp/ui_main_mp.cpp').read_text()
start = ui.index('static void UI_Sp_DismissCinematic(')
end = ui.index('{', start) + 1
depth = 1
while depth:
    depth += (ui[end] == '{') - (ui[end] == '}')
    end += 1
dismiss = ui[start:end]
source = output / 'fade.cpp'
source.write_text(r'''
#include <cassert>
#include <bit>
#include <cmath>
#include <cstring>
constexpr int MAX_LOCAL_CLIENTS = 1;
static unsigned now;
unsigned Sys_Milliseconds() { return now; }
struct dvar_s { struct { unsigned char color[4]; } current; };
static dvar_s defaultColor{};
static dvar_s intro;
static dvar_s *cg_defaultFadeScreenColor = &defaultColor, *com_introPlayed = &intro;
struct { void *whiteMaterial = reinterpret_cast<void *>(1); } cls;
static int draws, paused, menusClosed, stopped, promoted, unknownFade;
static float alpha;
static bool queued, playing, introPlayed;
void CL_GetScreenDimensions(int *w, int *h, float *aspect) { *w = 1280; *h = 720; *aspect = 1; }
void CL_DrawStretchPicPhysical(float, float, float, float, float, float, float, float,
    const float *color, void *) { ++draws; alpha = color[3]; assert(std::isfinite(alpha)); }
struct uiInfo_s { int uiDC; } uiInfo;
uiInfo_s *UI_GetInfo(int) { return &uiInfo; }
void UI_Sp_SetCinematicTransitionPending(int, bool) {}
void Dvar_SetIntByName(const char *name, int value) { assert(!strcmp(name, "cl_paused")); paused = value; }
void Cbuf_AddText(int, const char *) { ++unknownFade; }
void Menus_CloseAll(int, int *) { ++menusClosed; }
bool R_Cinematic_IsNextReady_Internal() { return queued; }
void R_Cinematic_StartNextPlayback() { ++promoted; queued = false; playing = true; }
void R_Cinematic_StopPlayback() { ++stopped; playing = false; }
void Dvar_SetBool(dvar_s *, bool value) { introPlayed = value; }
''' + fade + '\n' + dismiss + r'''
static float drawAlpha() {
    draws = 0;
    alpha = 0;
    CG_DrawScreenFade_SP(0);
    return alpha;
}
int main() {
    now = 0;
    CG_ResetScreenFade_SP(0);
    assert(drawAlpha() == 0 && draws == 0);
    for (int restart = 0; restart < 2; ++restart) {
        now += 100;
        CG_FadeScreen_SP(0, 0, 0, 0, 255, int(now), 100, false);
        now += 101;
        assert(drawAlpha() == 1 && draws == 1);
        // CG_Init clears the sidecar along with the new map's client state.
        CG_ResetScreenFade_SP(0);
        assert(drawAlpha() == 0 && draws == 0);
        // Dismissing a completed load movie must drive the clear fade directly.
        playing = true;
        queued = false;
        paused = 1;
        UI_Sp_DismissCinematic(0);
        assert(!playing && paused == 0 && introPlayed && unknownFade == 0);
        assert(drawAlpha() == 1);
        now += 1500;
        assert(std::fabs(drawAlpha() - 0.5f) < 0.001f);
        now += 1500;
        assert(drawAlpha() == 0 && draws == 0);
        now += 100;
        assert(drawAlpha() == 0 && draws == 0);
    }
    assert(stopped == 2 && menusClosed == 2);
    queued = true;
    UI_Sp_DismissCinematic(0);
    assert(promoted == 1 && playing && !queued);
    CG_FadeScreen_SP(0, 0, 0, 0, 0, int(now), 0, false);
    assert(drawAlpha() == 0 && draws == 0);
}
''')
for mode, defines in [('PC', []), ('NX', ['-DKISAK_NX'])]:
    exe = output / (mode + '.exe')
    subprocess.run([args.compiler, '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror',
                    '-Wno-unused-parameter', '-DKISAK_SP', *defines, str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
    print('PASS:', mode, 'repeated death/map reset, movie dismissal, three-second fade, queued movie, zero-duration fade')