#include "aim_assist.h"
#include "aim_target.h"
#include <client/splitscreen.h>
#include <client/client.h>
#include <client/screen_placement.h>
#include <cgame_mp/cg_local_mp.h>
#include <cgame_mp/cg_main_mp.h>
#include <universal/com_math_anglevectors.h>
#include <cstring>
#include <cstdio>

extern AimAssistGlobals aaGlobArray[MAX_LOCAL_CLIENTS];

void AimAssist_ResetSplitGuest()
{
    std::memset(&aaGlobArray[1], 0, sizeof(aaGlobArray[1]));
    AimTarget_ClearTargetList(1);
}

bool AimAssist_IsSplitGuestTargetInView(const centity_s *entity)
{
    float bounds[2][3], transform[4][3], clip[2][3];
    AimTarget_GetTargetBounds(entity, bounds[0], bounds[1]);
    AnglesToAxis(entity->pose.angles, transform);
    std::memcpy(transform[3], entity->pose.origin, sizeof(transform[3]));
    return AimAssist_ConvertToClipBounds(&aaGlobArray[1], bounds, transform, clip) != 0;
}

bool AimAssist_UpdateSplitGuestInput(const playerState_s *player, usercmd_s *command,
    float seconds, float pitchAxis, float yawAxis, float *rawAngles, bool reset)
{
    cg_s *game = CG_GetLocalClientGlobals(1);
    const cg_s *world = CG_GetLocalClientGlobals(0);
    if (!CL_SplitGuestHasView() || !aaGlobArray[0].initialized
        || !world->snap || !world->nextSnap || (player->otherFlags & 2)
        || scrPlaceView[1].realViewportSize[0] <= 0.0f
        || scrPlaceView[1].realViewportSize[1] <= 0.0f
        || world->refdef.tanHalfFovX <= 0.0f || world->refdef.tanHalfFovY <= 0.0f)
        return false;

    game->localClientNum = 1;
    game->clientNum = player->clientNum;
    game->time = world->time;
    game->predictedPlayerState = *player;
#ifdef KISAK_SP
    game->useAlternateAimParams = world->useAlternateAimParams;
#endif
    static snapshot_s snapshot;
    snapshot.ps = *player;
    snapshot.serverTime = clients[1].snap.serverTime;
    game->snap = game->nextSnap = &snapshot;
    for (int axis = 0; axis < 3; ++axis)
        game->predictedPlayerState.viewangles[axis] = rawAngles[axis] + player->delta_angles[axis];
    // The camera/PS are private; geometry, animation poses and collision remain
    // owned by world 0. Never initialize a second graph/UI/clientscript system.
    std::memcpy(game->refdef.vieworg, player->origin, sizeof(player->origin));
    game->refdef.vieworg[2] += player->viewHeightCurrent;
    AnglesToAxis(game->predictedPlayerState.viewangles, game->refdef.viewaxis);
    game->refdef.tanHalfFovX = world->refdef.tanHalfFovX;
    game->refdef.tanHalfFovY = world->refdef.tanHalfFovY;
    AimAssistGlobals &assist = aaGlobArray[1];
    if (reset || !assist.initialized)
        AimAssist_Setup(1, &game->predictedPlayerState);
    else
        AimAssist_BackupPlayerState(1, &game->predictedPlayerState);
    // Build the guest frustum before collecting candidates. P1's target list
    // and frustum cannot describe an independently moving, distant guest.
    std::memcpy(assist.viewOrigin, game->refdef.vieworg, sizeof(assist.viewOrigin));
    std::memcpy(assist.viewAngles, game->predictedPlayerState.viewangles, sizeof(assist.viewAngles));
    std::memcpy(assist.viewAxis, game->refdef.viewaxis, sizeof(assist.viewAxis));
    AimAssist_FovScale(&assist, game->refdef.tanHalfFovY);
    AimAssist_CreateScreenMatrix(&assist, game->refdef.tanHalfFovX, game->refdef.tanHalfFovY);
    AimTarget_UpdateClientTargets(1);
    AimAssist_UpdateScreenTargets(1, game->refdef.vieworg,
        game->predictedPlayerState.viewangles, game->refdef.tanHalfFovX, game->refdef.tanHalfFovY);

    AimInput input{};
    input.localClientNum = 1;
    input.deltaTime = seconds;
    input.pitch = game->predictedPlayerState.viewangles[0];
    input.yaw = game->predictedPlayerState.viewangles[1];
    input.pitchAxis = pitchAxis;
    input.yawAxis = yawAxis;
    input.forwardAxis = command->forwardmove / 127.0f;
    input.rightAxis = command->rightmove / 127.0f;
    input.button_bits = command->button_bits;
    AimOutput output{};
    AimAssist_UpdateGamePadInput(&input, &output);
    rawAngles[0] = output.pitch - player->delta_angles[0];
    rawAngles[1] = output.yaw - player->delta_angles[1];
    command->meleeChargeYaw = output.meleeChargeYaw;
    command->meleeChargeDist = output.meleeChargeDist;
    return true;
}

const char *AimAssist_SplitGuestStatus()
{
    static char status[256];
    const AimAssistGlobals &assist = aaGlobArray[1];
    std::snprintf(status, sizeof(status),
        "guest_aim enabled=%d ready=%d targets=%d slowdown=%d snap=%d lockon=%d",
        !AimAssist_PlayerDisabledAutoAim(), assist.initialized,
        assist.screenTargetCount, assist.aimSlowdownTargetEnt,
        assist.autoAimActive ? assist.autoAimTargetEnt : 1023, assist.lockOnTargetEnt);
    return status;
}
