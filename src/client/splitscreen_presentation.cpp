#include "splitscreen.h"
#include <client/client.h>
#include <client_mp/cl_main_mp.h>
#include <client_mp/cl_cgame_mp.h>
#include <universal/com_workercmds.h>
#include <cgame_mp/cg_local_mp.h>
#include <cgame_mp/cg_main_mp.h>
#include <cgame/cg_visionsets.h>
#include <EffectsCore/fx_update.h>
#include <EffectsCore/fx_shared_guest.h>
#include <universal/com_math_anglevectors.h>
#include <qcommon/common.h>
#include <cstring>
#include <sound/snd_public_async.h>
#include <cstdio>
#include <string>

struct SplitGuestPresentation
{
    bool valid = false, visionInitialized = false;
    int snapshotTime = 0, arrivalTime = 0, duration = 0, mode = 0, teleport = 0, spawnCount = -1;
    float from[3]{}, to[3]{};
    std::string nakedConfig, nightConfig, lastStandConfig;
};
static SplitGuestPresentation s_guestPresentation;

void CL_SplitGuestResetPresentation()
{
    R_WaitFrontendWorkerCmds();
    s_guestPresentation = SplitGuestPresentation{};
    CL_SplitGuestResetPrediction();
    SND_DisconnectListener(1);
    FX_SplitGuestReset();
    CG_SplitGuestResetHud();
}

static void SplitGuestCameraPosition(const SplitGuestPresentation &state, int now, float *origin)
{
    float fraction = state.duration > 0 ? static_cast<float>(now - state.arrivalTime) / state.duration : 1.0f;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    for (int axis = 0; axis < 3; ++axis)
        origin[axis] = state.from[axis] + fraction * (state.to[axis] - state.from[axis]);
}

void CL_SplitGuestSmoothCamera(refdef_s *view)
{
    SplitGuestPresentation &state = s_guestPresentation;
    const playerState_s &ps = clients[1].snap.ps;
    const int time = clients[1].snap.serverTime;
    float distanceSq = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
    {
        const float delta = view->vieworg[axis] - state.to[axis];
        distanceSq += delta * delta;
    }
    const bool reset = !state.valid || state.mode != ps.pm_type || state.teleport != (ps.eFlags & 2)
        || state.spawnCount != ps.stats[4] || time < state.snapshotTime || time - state.snapshotTime > 250 || distanceSq > 128.0f * 128.0f;
    if (reset)
    {
        state.valid = true;
        state.duration = 0;
        state.arrivalTime = cls.realtime;
        state.snapshotTime = time;
        std::memcpy(state.from, view->vieworg, sizeof(state.from));
        std::memcpy(state.to, view->vieworg, sizeof(state.to));
        view->playerTeleported = true;
    }
    else if (time != state.snapshotTime)
    {
        // Blend from the displayed position, including an unfinished preceding
        // interval. This keeps jittery snapshot arrivals continuous at 60+ FPS.
        SplitGuestCameraPosition(state, cls.realtime, state.from);
        state.duration = time - state.snapshotTime;
        if (state.duration > 100) state.duration = 100;
        state.arrivalTime = cls.realtime;
        state.snapshotTime = time;
        std::memcpy(state.to, view->vieworg, sizeof(state.to));
    }
    state.mode = ps.pm_type;
    state.spawnCount = ps.stats[4];
    state.teleport = ps.eFlags & 2;
    SplitGuestCameraPosition(state, cls.realtime, view->vieworg);
}

static std::string SplitGuestVisionConfig(int index, int clientNum)
{
#ifdef KISAK_SP
    char key[16];
    std::snprintf(key, sizeof(key), "%d", clientNum);
    return Info_ValueForKey(CL_GetConfigString(index), key);
#else
    return CL_GetConfigString(index);
#endif
}

void CL_SplitGuestApplyVision(refdef_s *view)
{
    cg_s *game = CG_GetLocalClientGlobals(1);
    const cg_s *host = CG_GetLocalClientGlobals(0);
    // Native MP vision reads snap->ps for flare/perk state before the HUD or
    // viewmodel helpers run. Supply this connection's snapshot on the first
    // frame as well as subsequent frames, without borrowing the host's state.
    static snapshot_s snapshot;
    snapshot.ps = clients[1].snap.ps;
    snapshot.serverTime = clients[1].snap.serverTime;
    game->snap = game->nextSnap = &snapshot;
    game->localClientNum = 1;
    game->clientNum = clients[1].snap.ps.clientNum;
    game->predictedPlayerState = clients[1].snap.ps;
    CL_SplitGuestGetViewAngles(game->predictedPlayerState.viewangles);
    game->time = view->time;
    game->refdef = *view;
    // These copy only vision values, never the host's ownership-bearing cg_s.
    // The processed host refdef may already be monochrome/damaged; use the
    // naked environment channel as the guest's initial baseline instead.
    const bool initialize = !s_guestPresentation.visionInitialized;
    if (initialize)
    {
        for (int channel = 0; channel < VISIONSETMODECOUNT; ++channel)
        {
            game->visionSetFrom[channel] = host->visionSetCurrent[VISIONSETMODE_NAKED];
            game->visionSetTo[channel] = game->visionSetFrom[channel];
            game->visionSetCurrent[channel] = game->visionSetFrom[channel];
            game->visionSetLerpData[channel] = {};
            game->visionSetLerpData[channel].style = VISIONSETLERP_NONE;
        }
        I_strncpyz(game->visionNameNaked, host->visionNameNaked, sizeof(game->visionNameNaked));
        CG_InitVisionSets(1);
#ifdef KISAK_SP
        CG_VisionSetStartLerp_To(1, VISIONSETMODE_LASTSTAND, VISIONSETLERP_NONE, "laststand", 0);
#endif
        s_guestPresentation.visionInitialized = true;
    }
    const std::string naked = SplitGuestVisionConfig(CS_VISIONSET_NAKED, game->clientNum);
    const std::string night = SplitGuestVisionConfig(1551, game->clientNum);
#ifdef KISAK_SP
    const std::string lastStand = SplitGuestVisionConfig(CS_SP_VISIONSET_LASTSTAND, game->clientNum);
#else
    const std::string lastStand;
#endif
    if (naked.empty())
    {
        game->visionSetFrom[VISIONSETMODE_NAKED] = host->visionSetCurrent[VISIONSETMODE_NAKED];
        game->visionSetTo[VISIONSETMODE_NAKED] = game->visionSetFrom[VISIONSETMODE_NAKED];
        game->visionSetCurrent[VISIONSETMODE_NAKED] = game->visionSetFrom[VISIONSETMODE_NAKED];
        game->visionSetLerpData[VISIONSETMODE_NAKED].style = VISIONSETLERP_NONE;
    }
    else if (initialize || naked != s_guestPresentation.nakedConfig)
        CG_VisionSetConfigString_Naked(1);
    if (!night.empty() && (initialize || night != s_guestPresentation.nightConfig))
        CG_VisionSetConfigString_Night(1);
#ifdef KISAK_SP
    if (!lastStand.empty() && (initialize || lastStand != s_guestPresentation.lastStandConfig))
        CG_VisionSetConfigString_LastStand_SP(1);
#endif
    s_guestPresentation.nakedConfig = naked;
    s_guestPresentation.nightConfig = night;
    s_guestPresentation.lastStandConfig = lastStand;
    CG_VisionSetsUpdate(1);
    CG_VisionSetApplyToRefdef(1, false);
    view->visionset = game->refdef.visionset;
    view->postEmissiveBrightening = game->refdef.postEmissiveBrightening;
    // Camera/player overlays cannot be inherited from the upper player's state.
    view->dof = {};
    view->flameFx = {};
    view->waterSheetingFx = {};
    view->genericFilter = {};
    view->poisonFx = {};
    view->electrifiedFx = {};
    view->transportedFx = {};
}

void CL_SplitGuestSetFxCamera()
{
    if (!CL_SplitGuestHasView())
        return;
    const refdef_s &host = CG_GetLocalClientGlobals(0)->refdef;
    FxCameraUpdate camera{};
    if (!CL_SplitGuestGetWorldCamera(camera.vieworg, camera.viewaxis))
        return;
    camera.tanHalfFovX = host.tanHalfFovX;
    camera.tanHalfFovY = host.tanHalfFovY;
    camera.zfar = host.zFar;
    FX_SetExtraCamera(0, &camera);
}
