#include "splitscreen.h"
#include <client/client.h>
#include <client_mp/cl_main_mp.h>
#include <cgame_mp/cg_main_mp.h>
#include <cgame_mp/cg_predict_mp.h>
#include <cgame_mp/cg_ents_mp.h>
#include <bgame/bg_pmove.h>
#include <bgame/bg_animation.h>
#include <qcommon/common.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

// Command replay follows BO1's native predictor and the BO2 guest approach in
// T6-Rebuild 3fec588/c9fd60d. No second cgame/clientscript lifecycle is started.
struct SplitGuestPrediction
{
    bool valid = false, haveSnapshot = false;
    int frame = -1, commandNumber = 0, snapshotTime = 0, serverId = 0;
    int client = -1, mode = -1, spawn = -1, teleport = 0;
    int oldCommandTime = 0, oldTime = 0, errorTime = 0, stepStart = 0;
    float oldOrigin[3]{}, snapshotOrigin[3]{}, error[3]{}, stepChange = 0.0f;
    int commands = 0, fallbacks = 0;
    const char *reason = "inactive";
    playerState_s player{};
};
static SplitGuestPrediction s_guestPrediction;
static pmove_t s_guestPmove;
static thread_local bool s_guestPredictionRunning;

bool CL_SplitGuestPredictionRunning()
{
    return s_guestPredictionRunning;
}

void CL_SplitGuestResetPrediction()
{
    s_guestPrediction = SplitGuestPrediction{};
    // Keep pmove_t's native collision visitor/vtable; never memset that object.
    s_guestPmove.proximity_data.reset();
    s_guestPmove.ps = nullptr;
    s_guestPmove.m_gjkcc_input = nullptr;
    s_guestPmove.currentPitch = s_guestPmove.averagePitch = 0.0f;
    s_guestPmove.nextPitch = 0;
    std::memset(s_guestPmove.pitchHistory, 0, sizeof(s_guestPmove.pitchHistory));
}

// Pmove writes animation conditions via TLS. Borrow the loaded world for that
// span, restoring P2's shared body metadata as well as the previous TLS pointer.
// No models/animation trees are copied, initialized, or freed by prediction.
struct SplitGuestPredictionContext
{
    bgs_t *previous;
    bgs_t &world;
    int client, proneTime, time, frameTime;
    clientInfo_t info;
    bool wasRunning;
    cg_s &traceWorld;
    snapshot_s *previousNextSnapshot;
    SplitGuestPredictionContext(bgs_t &source, int player, int now)
        : previous(bgs), world(source), client(player), proneTime(source.proneTime[player]),
          time(source.time), frameTime(source.frametime), info(source.clientinfo[player]),
          wasRunning(s_guestPredictionRunning), traceWorld(*CG_GetLocalClientGlobals(0)),
          previousNextSnapshot(traceWorld.nextSnap)
    {
        // Native client traces require nextSnap. Between incoming snapshots,
        // use the loaded world's current snapshot for this replay only.
        if (!traceWorld.nextSnap) traceWorld.nextSnap = traceWorld.snap;
        bgs = &world;
        world.time = now;
        world.frametime = cls.frametime;
        s_guestPredictionRunning = true;
    }
    ~SplitGuestPredictionContext()
    {
        world.clientinfo[client] = info;
        world.proneTime[client] = proneTime;
        world.time = time;
        world.frametime = frameTime;
        s_guestPredictionRunning = wasRunning;
        traceWorld.nextSnap = previousNextSnapshot;
        bgs = previous;
    }
};

static bool SplitGuestPredictionWanted(const playerState_s &ps)
{
    return Dvar_GetBool("splitguest_predict") && !Dvar_GetBool("cg_nopredict")
        && !Dvar_GetBool("cg_synchronousClients")
        && (ps.pm_type == 0 || ps.pm_type == 2 || ps.pm_type == 3 || ps.pm_type == 6 || ps.pm_type == 7)
        && !(ps.otherFlags & 2) && !(ps.linkFlags & 4)
        && !(ps.eFlags & 0x4300) && !(ps.eFlags2 & 0x10000000) && (ps.vehiclePos < 1 || ps.vehiclePos > 4);
}

static float SplitGuestErrorFraction(int now)
{
    const float duration = static_cast<float>(Dvar_GetFloat("cg_errorDecay"));
    return duration > 0.0f ? std::clamp(1.0f - (now - s_guestPrediction.errorTime) / duration, 0.0f, 1.0f) : 0.0f;
}

static void SplitGuestPublishPrediction()
{
    clientActive_t &client = clients[1];
    cg_s *game = CG_GetLocalClientGlobals(1);
    const playerState_s &ps = s_guestPrediction.valid ? s_guestPrediction.player : client.snap.ps;
    game->predictedPlayerState = ps;
    std::memcpy(client.cgameOrigin, ps.origin, sizeof(client.cgameOrigin));
    std::memcpy(client.cgameVelocity, ps.velocity, sizeof(client.cgameVelocity));
    std::memcpy(client.cgameViewangles, ps.viewangles, sizeof(client.cgameViewangles));
    client.cgamePredictedDataServerTime = s_guestPrediction.valid ? client.serverTime : client.snap.serverTime;
    client.cgameBobCycle = ps.bobCycle;
    client.cgameMovementDir = ps.movementDir;
}

void CL_SplitGuestUpdatePrediction()
{
    SplitGuestPrediction &state = s_guestPrediction;
    if (!clients || !clientConnections || !CL_IsSplitGuest(1))
    {
        CL_SplitGuestResetPrediction();
        return;
    }
    clientActive_t &client = clients[1];
    const playerState_s &snapshot = client.snap.ps;
    cg_s *world = CG_GetLocalClientGlobals(0);
    const int latest = client.cmdNumber;
    state.commands = 0;
    const bool ready = client.snap.valid && CL_GetLocalClientConnectionState(1) == CA_ACTIVE
        && snapshot.clientNum == clientConnections[1].clientNum
        && snapshot.clientNum >= 0 && snapshot.clientNum < 32
        && world->snap && world->bgs.animData;
    const int latestTime = latest > 0 ? client.cmds[latest & 0x7F].serverTime : snapshot.commandTime;
    const bool historyLost = latest > 127 && client.cmds[(latest - 127) & 0x7F].serverTime > snapshot.commandTime;
    if (!ready || !SplitGuestPredictionWanted(snapshot) || latest <= 0 || historyLost
        || latestTime < snapshot.commandTime || latestTime - snapshot.commandTime > 250)
    {
        state.valid = false;
        state.haveSnapshot = false;
        state.stepChange = 0.0f;
        std::memset(state.error, 0, sizeof(state.error));
        ++state.fallbacks;
        state.reason = !ready ? "not-ready" : historyLost ? "command-history"
            : !SplitGuestPredictionWanted(snapshot) ? "snapshot-mode" : "command-clock";
        s_guestPmove.proximity_data.reset();
        SplitGuestPublishPrediction();
        return;
    }

    float snapshotDelta = 0.0f;
    for (int axis = 0; axis < 3; ++axis)
    {
        const float delta = snapshot.origin[axis] - state.snapshotOrigin[axis];
        snapshotDelta += delta * delta;
    }
    const bool reset = !state.haveSnapshot || state.serverId != cls.serverId
        || state.client != snapshot.clientNum || state.mode != snapshot.pm_type
        || state.spawn != snapshot.stats[4] || state.teleport != (snapshot.eFlags & 2)
        || client.snap.serverTime < state.snapshotTime || client.snap.serverTime - state.snapshotTime > 250
        || cls.realtime < state.frame || cls.realtime - state.frame > 250
        || latest < state.commandNumber || latestTime < state.oldCommandTime
        || snapshotDelta > 128.0f * 128.0f;
    if (reset)
    {
        state.valid = false;
        state.stepChange = 0.0f;
        state.stepStart = 0;
        std::memset(state.error, 0, sizeof(state.error));
        s_guestPmove.proximity_data.reset();
        s_guestPmove.currentPitch = s_guestPmove.averagePitch = 0.0f;
        s_guestPmove.nextPitch = 0;
        std::memset(s_guestPmove.pitchHistory, 0, sizeof(s_guestPmove.pitchHistory));
    }
    state.player = snapshot;
    pmove_t &move = s_guestPmove;
    move.ps = &state.player;
    move.handler = 0; // native client traces; no server player-event callback
    move.localClientNum = 0; // geometry/movers belong to the single loaded world
#ifdef KISAK_SP
    move.tracemask = 0x281c011;
#else
    move.tracemask = 0x2818011;
#endif
    move.viewChange = 0.0f;
    move.viewChangeTime = state.stepStart;
    float moverAngles[3]{};
    CG_AdjustPositionForMover(0, state.player.origin, state.player.groundEntityNum,
        client.snap.serverTime, client.serverTime, state.player.origin, moverAngles);
    state.player.delta_angles[1] += moverAngles[1];
    const int first = (std::max)(1, latest - 126); // leave the preceding oldcmd in the 128-entry ring
    bool compared = false;
    {
        SplitGuestPredictionContext context(world->bgs, snapshot.clientNum, client.serverTime);
        for (int number = first; number <= latest; ++number)
        {
            const usercmd_s &command = client.cmds[number & 0x7F];
            if (command.serverTime <= state.player.commandTime || command.serverTime > latestTime)
                continue;
            if (state.valid && !compared && state.player.commandTime == state.oldCommandTime)
            {
                float adjusted[3], deltaAngles[3];
                CG_AdjustPositionForMover(0, state.oldOrigin, state.player.groundEntityNum,
                    state.oldTime, client.serverTime, adjusted, deltaAngles);
                float delta[3], length = 0.0f;
                for (int axis = 0; axis < 3; ++axis)
                {
                    delta[axis] = adjusted[axis] - state.player.origin[axis];
                    length += delta[axis] * delta[axis];
                }
                if (length > 0.000001f)
                {
                    const float fraction = length <= 128.0f * 128.0f ? SplitGuestErrorFraction(cls.realtime) : 0.0f;
                    for (int axis = 0; axis < 3; ++axis)
                        state.error[axis] = length <= 128.0f * 128.0f ? state.error[axis] * fraction + delta[axis] : 0.0f;
                    state.errorTime = cls.realtime;
                }
                compared = true;
            }
            move.cmd = command;
            move.oldcmd = client.cmds[(number - 1) & 0x7F];
            Pmove(&move);
            ++state.commands;
        }
    }
    // Camera look is sampled every displayed frame, even while replaying an
    // unacknowledged command whose packed angles belong to an older frame.
    CL_SplitGuestGetViewAngles(state.player.viewangles);

    const float duration = static_cast<float>(Dvar_GetFloat("cg_viewZSmoothingTime") * 1000.0);
    if (move.viewChange != 0.0f && move.viewChangeTime != state.stepStart && !reset
        && (snapshot.pm_type == 0 || snapshot.pm_type == 2 || snapshot.pm_type == 3)
        && std::fabs(move.viewChange) >= Dvar_GetFloat("cg_viewZSmoothingMin"))
    {
        const float remaining = duration > 0.0f
            ? std::clamp(1.0f - (move.viewChangeTime - state.stepStart) / duration, 0.0f, 1.0f) * state.stepChange : 0.0f;
        const float maximum = (std::max)(0.0f, static_cast<float>(Dvar_GetFloat("cg_viewZSmoothingMax")));
        state.stepChange = std::clamp(remaining + move.viewChange, -maximum, maximum);
        state.stepStart = move.viewChangeTime;
    }
    else if (client.serverTime - state.stepStart >= duration)
        state.stepChange = 0.0f;
    state.valid = state.haveSnapshot = true;
    state.frame = cls.realtime;
    state.commandNumber = latest;
    state.snapshotTime = client.snap.serverTime;
    state.serverId = cls.serverId;
    state.client = snapshot.clientNum;
    state.mode = snapshot.pm_type;
    state.spawn = snapshot.stats[4];
    state.teleport = snapshot.eFlags & 2;
    state.oldCommandTime = state.player.commandTime;
    state.oldTime = client.serverTime;
    std::memcpy(state.oldOrigin, state.player.origin, sizeof(state.oldOrigin));
    std::memcpy(state.snapshotOrigin, snapshot.origin, sizeof(state.snapshotOrigin));
    state.reason = "predicted";
    SplitGuestPublishPrediction();
}

const playerState_s *CL_SplitGuestPredictedState()
{
    const SplitGuestPrediction &state = s_guestPrediction;
    return state.valid && clients && state.frame == cls.realtime && state.serverId == cls.serverId
        && state.commandNumber == clients[1].cmdNumber && state.snapshotTime == clients[1].snap.serverTime
        && Dvar_GetBool("splitguest_predict") ? &state.player : nullptr;
}

bool CL_SplitGuestPredictedCamera(float *origin, float *angles)
{
    const playerState_s *player = CL_SplitGuestPredictedState();
    if (!player)
        return false;
    std::memcpy(origin, player->origin, sizeof(player->origin));
    origin[2] += player->viewHeightCurrent;
    std::memcpy(angles, player->viewangles, sizeof(player->viewangles));
    const float fraction = SplitGuestErrorFraction(cls.realtime);
    for (int axis = 0; axis < 3; ++axis)
        origin[axis] += s_guestPrediction.error[axis] * fraction;
    const float duration = static_cast<float>(Dvar_GetFloat("cg_viewZSmoothingTime") * 1000.0);
    if (duration > 0.0f)
        origin[2] -= s_guestPrediction.stepChange
            * std::clamp(1.0f - (clients[1].serverTime - s_guestPrediction.stepStart) / duration, 0.0f, 1.0f);
    return true;
}

const char *CL_SplitGuestPredictionStatus()
{
    static char text[192];
    std::snprintf(text, sizeof(text), "guest_prediction %s commands=%d command_time=%d snapshot_time=%d fallbacks=%d",
        s_guestPrediction.reason, s_guestPrediction.commands, s_guestPrediction.player.commandTime,
        s_guestPrediction.snapshotTime, s_guestPrediction.fallbacks);
    return text;
}
