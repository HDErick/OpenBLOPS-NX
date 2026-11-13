#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef __cdecl
#define __cdecl
#endif
#define __int8 char
#define __debugbreak() assert(false)
#define PROF_SCOPED(x)
template<class... T> bool Assert_MyHandler(T...) { assert(false); return false; }
template<class... T> void Com_Printf(T...) {}
template<class... T> void Com_PrintWarning(T...) {}
void I_strncpyz(char *out, const char *in, size_t size) { snprintf(out, size, "%s", in); }

struct dvar_s { struct { bool enabled; float value; } current; };
static dvar_s fullscreen{{true, 0}}, master{{false, 0.8f}}, volume{{false, 0.5f}}, server{};
static const dvar_s *cg_cinematicFullscreen = &fullscreen, *snd_menu_master = &master;
static const dvar_s *snd_menu_cinematic = &volume, *com_sv_running = &server;
void Dvar_SetBool(dvar_s *dvar, bool value) { dvar->current.enabled = value; }
enum CinematicEnum { CINEMATIC_NOT_PAUSED, CINEMATIC_PAUSED };
struct Bink {};
static Bink movie;
struct Hunk { void *base; };
struct {
    char currentCinematicName[256], targetCinematicName[256], nextCinematicName[256];
    unsigned playbackFlags, nextCinematicPlaybackFlags;
    float playbackVolume;
    bool targetCinematicChanged, cinematicFinished, firstFrameNotify, lastFrameNotify, underrun, isPreloaded;
    CinematicEnum targetPaused, currentPaused;
    int activeImageFrame = -1, activeTextureSet, fileIoState, timeInMsec, framesStopped;
    Hunk binkHunk;
    Bink *bink;
    struct { int bink_buffers; } binkTextureSet;
} cinematicGlob;
static bool r_cinematic_disabled, g_cinematicInitialized = true, queued, zoneReleased;
static int cineThreadBlock, _UpdateFrameWorkerCmd, closes, decodes;
struct CINE_JQ_CMD {};
enum { CRITSECT_CINEMATIC, CRITSECT_CINEMATIC_TARGET_CHANGE };
void Sys_EnterCriticalSection(int) {}
void Sys_LeaveCriticalSection(int) {}
int _InterlockedExchange(int *p, int v) { int old = *p; *p = v; return old; }
void Sys_AddWorkerCmdInternal(int *, unsigned char *, int) { assert(!queued); queued = true; }
int _UpdateFrameCallback();
void Sys_WaitWorkerCmdInternal(int *) { if (queued) { queued = false; _UpdateFrameCallback(); } }
void R_Cinematic_UpdateRendererImages() {}
void R_Cinematic_CheckBinkError() {}
void R_Cinematic_BlackRendererImages() {}
void R_Cinematic_InitBinkVolumes() {}
void R_Cinematic_InitBinkTextures() {}
void R_Cinematic_RelinquishIO() { cinematicGlob.fileIoState = 0; }
void CinematicHunk_Reset(Hunk *) {}
bool CinematicHunk_IsEmpty(Hunk *) { return true; }
void R_Cinematic_HunksReset(int, unsigned) {}
bool R_Cinematic_HunksOpen(int, unsigned) { return true; }
void R_Cinematic_Bink_Alloc() {}
void R_Cinematic_Bink_Free() {}
void BinkSetMemory(void (*)(), void (*)()) {}
static unsigned g_cinematicDS;
void BinkOpenDirectSound() {}
void BinkSetSoundSystem(void (*)(), unsigned) {}
bool R_Cinematic_AreHunksOpen() { return false; }
void R_Cinematic_HunksClose() {}
bool Sys_IsRenderThread() { return false; }
void RB_Resource_Callback(void (*callback)()) { callback(); }
void RB_Resource_Flush() {}
void SND_NotifyCinematicStart(float) {}
void SND_NotifyCinematicEnd() {}
void BinkSetSoundTrack(int, unsigned *) {}
void BinkGetFrameBuffersInfo(Bink *, int *) {}
void BinkRegisterFrameBuffers(Bink *, int *) {}
void BinkClose(Bink *bink) { assert(bink && !zoneReleased); ++closes; }
static std::vector<std::string> opened;
bool R_Cinematic_BinkOpen(const char *name, unsigned, char *error, unsigned) {
    opened.emplace_back(name);
    if (!strcmp(name, "missing") || !strcmp(name, "default")) { strcpy(error, "file missing"); return false; }
    cinematicGlob.bink = &movie;
    return true;
}
char R_Cinematic_Advance(bool) { assert(cinematicGlob.bink && !zoneReleased); ++decodes; return 1; }

constexpr int MAX_LOCAL_CLIENTS = 1;
static bool s_deferred3DCinematicPending_SP[1];
static char s_deferred3DCinematicName_SP[1][256];
static unsigned s_deferred3DCinematicFlags_SP[1];
void Com_ShutdownDynamicMemorySystems() {
    assert(!cinematicGlob.bink && !cineThreadBlock && !queued);
    zoneReleased = true;
}
void SND_GameReset() {}
void CG_SndGameReset() {}
void Ragdoll_Shutdown() {}
void Com_FreeWeaponInfoMemory(int) {}
enum { SCRIPTINSTANCE_CLIENT, SCRIPTINSTANCE_SERVER };
void Scr_ShutdownSystem(int, unsigned, int) {}
void CScr_FreeScripts() {}
void Scr_FreeScripts(int, unsigned) {}
void Scr_ShutdownGameStrings(int) {}
void Scr_FreeEntityList(int) {}
void BG_ShutdownFire() {}
void CG_ClearCompassPingData() {}
void CG_ShutdownConsoleCommands() {}
void CG_ResetClientInitializationState() {}

struct ScreenPlacement {};
struct Material { const char *name; };
static Material cinematic{"cinematic"}, loadscreen{"loadscreen"}, ordinary{"ordinary"};
struct { Material *loadingScreen = &loadscreen; } sharedUiInfo;
constexpr int CA_ACTIVE = 9;
static int connection;
int CL_GetLocalClientConnectionState(int) { return connection; }
const char *Material_GetName(Material *m) { return m->name; }
static Material *drawn;
void CL_DrawStretchPic(const ScreenPlacement *, float, float, float, float, int, int,
                      float, float, float, float, const float *, Material *m) { drawn = m; }

constexpr int UISP_NONE = 0, UISP_PREGAME = 4;
static int activeMenu, paused;
static bool menuFullscreen;
static bool s_nxCinematicSkipRequested[1];
static bool s_spCinematicTransitionPending[MAX_LOCAL_CLIENTS];
bool UI_Sp_IsCinematicTransitionPending(int);
void UI_Sp_SetCinematicTransitionPending(int, bool);
struct uiInfo_s { int uiDC; } ui;
static dvar_s intro;
static const dvar_s *com_introPlayed = &intro;
uiInfo_s *UI_GetInfo(int) { return &ui; }
int UI_GetActiveMenu(int) { return activeMenu; }
int UI_IsFullscreen(int) { return menuFullscreen; }
const char *Dvar_GetString(const char *) { return ""; }
void Dvar_SetIntByName(const char *name, int value) { if (!strcmp(name, "cl_paused")) paused = value; }
int Dvar_GetInt(const char *) { return 0; }
dvar_s *Dvar_FindVar(const char *) { return nullptr; }
unsigned Sys_Milliseconds() { return 1234; }
void CG_FadeScreen_SP(int, int, int, int, int, int, int, bool) {}
void Menus_CloseAll(int, int *) { menuFullscreen = false; }
void UI_SetActiveMenuSp(int, int menu) { activeMenu = menu; if (menu == UISP_NONE) paused = 0; }
void UI_SetShaderTime(int) {}
int Com_LocalClients_GetPrimary() { return 0; }
bool Key_IsCatcherActive(int, int) { return false; }
void UI_EmblemUpdate(int) {}
void R_Cinematic_WaitForUpdateFrame();
void R_Cinematic_UpdateFrame(bool);
#include "lifecycle-functions.inc"

static void update() { R_Cinematic_UpdateFrame(true); R_Cinematic_WaitForUpdateFrame(); }
static void draw(Material *m) { UI_DrawHandlePic(nullptr, 0, 0, 640, 480, 0, 0, nullptr, m); }

int main() {
    R_Cinematic_StartPlayback_Internal("frontend", 10, 0);
    Dvar_SetBool(&fullscreen, false);
    update();
    draw(&cinematic);
#ifdef KISAK_NX
    assert(drawn == &loadscreen);
#else
    assert(drawn == &cinematic);
#endif
    draw(&ordinary); assert(drawn == &ordinary);
    connection = CA_ACTIVE;
    draw(&cinematic); assert(drawn == &cinematic);
    connection = 0;

    R_Cinematic_StartPlayback("treyarch", 4, 0.16f);
    update();
    draw(&cinematic); assert(drawn == &cinematic);
    assert(closes == 1 && fullscreen.current.enabled);

    s_deferred3DCinematicPending_SP[0] = true;
    strcpy(s_deferred3DCinematicName_SP[0], "frontend");
    s_deferred3DCinematicFlags_SP[0] = 10;
    R_Cinematic_SetNextPlayback("number_lady_intro", 0);
    cinematicGlob.cinematicFinished = true;
    CG_StartDeferred3DCinematic_SP(0);
    assert(s_deferred3DCinematicPending_SP[0]);
    R_Cinematic_StartNextPlayback();
    CG_StartDeferred3DCinematic_SP(0);
    assert(s_deferred3DCinematicPending_SP[0]);
    assert(std::abs(cinematicGlob.playbackVolume - 0.16f) < 0.0001f);
    update();
    assert(opened.back() == "number_lady_intro");
    cinematicGlob.cinematicFinished = true;
    CG_StartDeferred3DCinematic_SP(0);
    assert(!s_deferred3DCinematicPending_SP[0] && !fullscreen.current.enabled);
    update();
    assert(opened.back() == "frontend");

    // Repeat the frontend -> narrative load -> frontend handoff. A 3D request
    // must not replace a pending/playing fullscreen movie. Test both natural
    // completion and the stop request used by the pregame skip handler.
    for (int pass = 0; pass < 4; ++pass) {
        CG_Start3DCinematic_SP(0, "frontend", 10);
        update();
        assert(!fullscreen.current.enabled && opened.back() == "frontend");
        R_Cinematic_StartPlayback("so_narrative1_frontend_load", 4, 0.16f);
        assert(fullscreen.current.enabled);
        const std::string previousTarget = cinematicGlob.targetCinematicName;
        CG_Start3DCinematic_SP(0, "frontend", 10);
        assert(s_deferred3DCinematicPending_SP[0]);
        assert(previousTarget == cinematicGlob.targetCinematicName);
        CG_StartDeferred3DCinematic_SP(0);
        assert(s_deferred3DCinematicPending_SP[0]);
        update();
        CG_Start3DCinematic_SP(0, "frontend", 10);
        CG_Sp_UpdateCinematicPlayback();
        assert(s_deferred3DCinematicPending_SP[0]);
        assert(fullscreen.current.enabled && opened.back() == "so_narrative1_frontend_load");
        if (pass % 2) {
            R_Cinematic_StopPlayback();
            update();
        } else {
            cinematicGlob.cinematicFinished = true;
        }
        CG_Sp_UpdateCinematicPlayback();
        assert(!s_deferred3DCinematicPending_SP[0]);
        assert(!fullscreen.current.enabled);
        assert(!strcmp(cinematicGlob.targetCinematicName, "frontend"));
        update();
        assert(opened.back() == "frontend" && R_Cinematic_IsStarted());
    }
    // Reproduce actual frame ordering: CG draw runs before the pregame UI.
    // Natural completion must remain visible to UI so it can close/unpause.
    for (int pass = 0; pass < 2; ++pass) {
        R_Cinematic_StartPlayback("so_narrative1_frontend_load", 4, 0.16f);
        update();
        CG_Start3DCinematic_SP(0, "frontend", 10);
        activeMenu = UISP_PREGAME;
        menuFullscreen = true;
        paused = 1;
        cinematicGlob.cinematicFinished = true;
        CG_Sp_UpdateCinematicPlayback();
        assert(R_Cinematic_IsFinished());
        assert(s_deferred3DCinematicPending_SP[0] && fullscreen.current.enabled);
        UI_Project_Refresh(0);
        assert(activeMenu == UISP_NONE && !menuFullscreen && paused == 0);
        update();
        CG_Sp_UpdateCinematicPlayback();
        update();
        assert(opened.back() == "frontend" && !fullscreen.current.enabled);
    }
    // The frontend client script can replace pregame with its main/script
    // menu before the movie finishes. Completion must still unpause it and
    // preserve the new menu, regardless of whether that menu is fullscreen.
    for (int pass = 0; pass < 2; ++pass) {
        R_Cinematic_StartPlayback("so_narrative1_frontend_load", 4, 0.16f);
        update();
        UI_Sp_SetCinematicTransitionPending(0, true);
        activeMenu = 6; // UIMENU_SCRIPT_POPUP, written by UI_PopupScriptMenu
        menuFullscreen = pass != 0;
        paused = 1;
        cinematicGlob.cinematicFinished = true;
        CG_Sp_UpdateCinematicPlayback();
        assert(R_Cinematic_IsFinished());
        assert(UI_Sp_IsCinematicTransitionPending(0));
        UI_Project_Refresh(0);
        assert(paused == 0 && activeMenu == 6 && menuFullscreen == (pass != 0));
        assert(!UI_Sp_IsCinematicTransitionPending(0));
        update();
        // With the server unpaused, its 3D frontend movie can now start.
        CG_Start3DCinematic_SP(0, "frontend", 10);
        update();
        assert(opened.back() == "frontend" && !fullscreen.current.enabled);
    }
    activeMenu = UISP_NONE;
    // A script skip closes pregame without resetting currentMenuType. The
    // stale type alone must not prevent the deferred frontend from starting.
    R_Cinematic_StartPlayback("so_narrative1_frontend_load", 4, 0.16f);
    update();
    CG_Start3DCinematic_SP(0, "frontend", 10);
    activeMenu = UISP_PREGAME;
    menuFullscreen = true;
    paused = 1;
    UI_Sp_DismissCinematic(0);
    assert(!menuFullscreen && paused == 0);
    update();
    CG_Sp_UpdateCinematicPlayback();
    update();
    assert(opened.back() == "frontend" && !fullscreen.current.enabled);
    activeMenu = UISP_NONE;
    // Queue promotion must restore fullscreen after a 3D menu movie too.
    R_Cinematic_SetNextPlayback("number_lady_intro", 0);
    cinematicGlob.cinematicFinished = true;
    CG_Start3DCinematic_SP(0, "frontend", 10);
    // No fullscreen movie was active, so this starts directly as a 3D reel.
    update();
    CG_Sp_UpdateCinematicPlayback();
    assert(fullscreen.current.enabled && R_Cinematic_IsTargetSet());
    CG_Start3DCinematic_SP(0, "frontend", 10);
    assert(s_deferred3DCinematicPending_SP[0]);
    update();
    cinematicGlob.cinematicFinished = true;
    CG_Sp_UpdateCinematicPlayback();
    update();
    assert(opened.back() == "frontend" && !fullscreen.current.enabled);

    const size_t before = opened.size();
    R_Cinematic_StartPlayback("missing", 4, 1);
    update();
    #ifdef KISAK_NX
    assert(opened.size() == before + 1 && opened.back() == "missing");
#else
    assert(opened.size() == before + 2 && opened.back() == "default");
#endif
    assert(R_Cinematic_IsFinished() && !R_Cinematic_IsStarted() && !cinematicGlob.bink);
    draw(&cinematic);
#ifdef KISAK_NX
    assert(drawn == &loadscreen);
#else
    assert(drawn == &cinematic);
#endif

    R_Cinematic_StartPlayback_Internal("frontend", 10, 0);
    update();
    const int frames = decodes, closed = closes;
    R_Cinematic_UpdateFrame(false);
    assert(queued);
    s_deferred3DCinematicPending_SP[0] = true;
    UI_Sp_SetCinematicTransitionPending(0, true);
    CG_ShutdownOnceForAllClients();
    assert(zoneReleased && closes == closed + 1 && decodes == frames);
    assert(!R_Cinematic_IsStarted());
    assert(!s_deferred3DCinematicPending_SP[0]);
    assert(!UI_Sp_IsCinematicTransitionPending(0));
    puts("PASS: fullscreen/3D routing, repeated return-to-menu completion/skip, queue priority, missing-movie fallback, safe zone teardown");
}
