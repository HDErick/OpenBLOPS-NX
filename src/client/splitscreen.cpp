#ifdef KISAK_NX
#include <nx/nx_stall_scope.h>
#endif
#include "splitscreen.h"
#include <universal/assertive.h>
#include <ui/ui_main.h>
#include <qcommon/com_clients.h>
#include "cl_main.h"

// Client zero remains active for the frontend, including before profile sign-in.
int __cdecl CL_LocalClient_GetActiveCount()
{
    int count = 0;
    for (int client = 0; client < MAX_LOCAL_CLIENTS; ++client)
        count += CL_LocalClient_IsActive(client) != 0;
    return count;
}

char __cdecl CL_LocalClient_IsActive(int localClientNum)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
    return localClientNum == 0 || Com_LocalClient_IsBeingUsed(localClientNum);
}

char __cdecl CL_LocalClient_IsFirstActive(int localClientNum)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
    if (!CL_LocalClient_IsActive(localClientNum))
        return false;
    for (int client = 0; client < localClientNum; ++client)
        if (CL_LocalClient_IsActive(client))
            return false;
    return true;
}

char __cdecl CL_AllLocalClientsDisconnected()
{
    for (int client = 0; client < MAX_LOCAL_CLIENTS; ++client)
    {
        if (CL_LocalClient_IsActive(client) && !UI_IsFullscreen(client)
            && clientUIActives[client].connectionState >= CA_CONNECTING)
            return false;
    }
    return true;
}

char __cdecl CL_AnyLocalClientStateActive()
{
    int localClientNum; // [esp+0h] [ebp-4h]

    for ( localClientNum = 0; localClientNum < MAX_LOCAL_CLIENTS; ++localClientNum )
    {
        if (CL_LocalClient_IsActive(localClientNum) && clientUIActives[localClientNum].connectionState == CA_ACTIVE)
            return 1;
    }
    return 0;
}

char __cdecl CL_AnyLocalClientChallenging()
{
    int localClientNum; // [esp+0h] [ebp-4h]

    for ( localClientNum = 0; localClientNum < MAX_LOCAL_CLIENTS; ++localClientNum )
    {
        if (CL_LocalClient_IsActive(localClientNum) && clientUIActives[localClientNum].connectionState == CA_CHALLENGING)
            return 1;
    }
    return 0;
}

void __cdecl CL_LocalClients_SetAllUsedActive()
{
    int localClientNum; // [esp+0h] [ebp-4h]

    for ( localClientNum = 0; localClientNum < MAX_LOCAL_CLIENTS; ++localClientNum )
    {
        Com_LocalClient_IsBeingUsed(localClientNum);
        //BLOPS_NULLSUB();
    }
}

int __cdecl CL_LocalActiveIndexFromClientNum(int localClientNum)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
    iassert(CL_LocalClient_IsActive(localClientNum));
    int index = 0;
    for (int client = 0; client < localClientNum; ++client)
        index += CL_LocalClient_IsActive(client) != 0;
    return index;
}

extern const dvar_t *name; // KISAKTODO: remove this extern hack after live/ folder done
const char *__cdecl CL_ControllerIndex_GetUsername()
{
    return name->current.string;
}


#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
#include <client/client.h>
#include <client_mp/cl_cgame_mp.h>
#include <client_mp/cl_input_mp.h>
#include <client_mp/sv_client_mp.h>
#include <cgame_mp/cg_main_mp.h>
#include <cgame_mp/cg_local_mp.h>
#include <qcommon/common.h>
#include <qcommon/cmd.h>
#include <qcommon/net_chan_mp.h>
#include <server_mp/sv_main_mp.h>
#include <cstring>
#include <cstdio>
#include <Windows.h>
#include <gfx_d3d/r_scene.h>
#include <cgame/cg_weapons.h>
#include <bgame/bg_weapons.h>
#include <aim_assist/aim_assist.h>
#include <cstdlib>
#include <sound/snd_public_async.h>
#include <universal/com_workercmds.h>
#include <gfx_d3d/r_rendercmds.h>
#include <universal/com_math_anglevectors.h>

static bool s_splitGuestConnected;
static unsigned int s_splitGuestSerial;
static int s_splitGuestStartTime;
static bool s_splitGuestViewSubmitted;
static bool s_splitGuestAutoAttempted;

// Native file I/O keeps diagnostics beside this executable, independent of
// the shared asset junction, pure filesystem and in-game console filters.
void CL_SplitGuestLog(const char *event)
{
    char path[MAX_PATH];
    const DWORD length = GetModuleFileNameA(nullptr, path, sizeof(path));
    if (!length || length >= sizeof(path))
        return;
    char *filename = std::strrchr(path, '\\');
#ifdef KISAK_NX
    // libnx's executable path uses forward slashes.
    if (!filename) filename = std::strrchr(path, '/');
#endif
    if (!filename || sizeof(path) - (++filename - path) < sizeof("splitguest-status.log"))
        return;
    std::strcpy(filename, "splitguest-status.log");
    FILE *file = std::fopen(path, "a");
    if (!file)
        return;
    const int state = clientUIActives[1].connectionState;
    std::fprintf(file, "[%lu:%d] %s | enabled=%d host=%d map=%s allocated=%d camera_submitted=%d\n",
        GetCurrentProcessId(), cls.realtime, event, s_splitGuestConnected,
        clientUIActives[0].connectionState, Dvar_GetString("mapname"), cl_maxLocalClients, s_splitGuestViewSubmitted);
    if (s_splitGuestConnected && clients && clientConnections)
    {
        const clientActive_t &guest = clients[1];
        const clientConnection_t &connection = clientConnections[1];
        const char *guestName = "(connecting)";
        if (svs.clients && com_maxclients && state >= CA_PRIMED
            && connection.clientNum >= 0 && connection.clientNum < com_maxclients->current.integer)
            guestName = svs.clients[connection.clientNum].name;
        std::fprintf(file, "  name=%s connection=%d qport=%d wire_qport=%d entity=%d snapshot=%d "
            "snapshot_time=%d player_mode=%d origin=%.1f %.1f %.1f commands=%d packet_age_ms=%d "
            "server_state=%d server_signon=%d\n",
            guestName, state, connection.qport, connection.netchan.qport, connection.clientNum,
            guest.snap.valid, guest.snap.serverTime, guest.snap.ps.pm_type,
            guest.snap.ps.origin[0], guest.snap.ps.origin[1], guest.snap.ps.origin[2],
            guest.cmdNumber, cls.realtime - connection.lastPacketTime,
            svs.clients && state >= CA_PRIMED && connection.clientNum >= 0
                && connection.clientNum < com_maxclients->current.integer
                ? svs.clients[connection.clientNum].header.state : -1,
            svs.clients && state >= CA_PRIMED && connection.clientNum >= 0
                && connection.clientNum < com_maxclients->current.integer
                #ifdef KISAK_SP
                ? svs.clients[connection.clientNum].header.connectionState_SP : -1);
#else
                ? svs.clients[connection.clientNum].header.state : -1);
#endif
    }
    if (s_splitGuestConnected && clients && clients[1].snap.valid)
    {
        const playerState_s &ps = clients[1].snap.ps;
        std::fprintf(file, "  guest_hud weapon=%u weapon_state=%d weapon_flags=%x hint=%d hint_string=%d host_mode=%d\n",
            static_cast<unsigned int>(ps.weapon), ps.weaponstate, ps.weapFlags,
            ps.cursorHint, ps.cursorHintString, CG_GetLocalClientGlobals(0)->predictedPlayerState.pm_type);
    }
    std::fprintf(file, "  %s\n", CG_SplitGuestHudStatus());
    std::fprintf(file, "  %s\n", CL_SplitGuestInputStatus());
    std::fprintf(file, "  %s\n", CL_SplitGuestPredictionStatus());
    std::fprintf(file, "  %s\n", AimAssist_SplitGuestStatus());
    std::fclose(file);
}

int CL_SplitGuestExpectedPlayers()
{
    // Report both initial players before either connection becomes active.
    // The normal GSC all-players-connected barrier then starts them together.
    const char *map = Dvar_GetString("mapname");
#ifdef KISAK_SP
    const bool supported = Com_IsZombieMap(map);
#else
    const bool supported = !Dvar_GetBool("onlinegame") && !std::strncmp(map, "mp_", 3);
#endif
    return CL_SplitScreenSessionRequested() && Dvar_GetBool("systemlink")
        && supported && com_maxclients && com_maxclients->current.integer >= 2 ? 2 : 1;
}

bool CL_IsSplitGuest(int localClientNum)
{
    return localClientNum == 1 && s_splitGuestConnected;
}

// The shared viewport layout starts before MP class selection. Rendering a
// camera still requires a valid snapshot; SP also waits for a spawned player.
bool CL_SplitGuestHasViewport()
{
#ifdef KISAK_SP
    return CL_SplitGuestHasView();
#else
    // Reserve both halves before either player picks a class. Guest admission
    // starts while P1 is primed, before P1's first active/menu frame.
    return s_splitGuestConnected && clients && clientConnections
        && CL_GetLocalClientConnectionState(0) == CA_ACTIVE
        && Dvar_GetBool("splitguest_view")
        && !CG_GetLocalClientGlobals(0)->refdef.extraCamPosValid;
#endif
}

bool CL_SplitGuestHasView()
{
    return s_splitGuestConnected && clients && clientConnections
        && CL_GetLocalClientConnectionState(0) == CA_ACTIVE
        && CL_GetLocalClientConnectionState(1) == CA_ACTIVE
        && clients[1].snap.valid && !(clients[1].snap.snapFlags & 2)
#ifdef KISAK_SP
        && clients[1].snap.ps.pm_type != 4 /* spectator */
#endif
        && clients[1].snap.ps.pm_type != 5 /* intermission */
        && (clients[1].snap.ps.clientNum == clientConnections[1].clientNum
            || ((clients[1].snap.ps.otherFlags & 2) && clients[1].snap.ps.clientNum < 32)
#ifndef KISAK_SP
            || CL_SplitGuestOwnsMenu() /* waiting spectators still need a menu draw target */
#endif
            )
        && Dvar_GetBool("splitguest_view")
#ifdef KISAK_SP
        && !UI_IsFullscreen(0)
#endif
        && !CG_GetLocalClientGlobals(0)->refdef.extraCamPosValid;
}

// Main-thread snapshot of the second camera. Renderer workers consume only the
// copied LOD parameters; they never query connection, input or UI state.
bool CL_SplitGuestGetWorldCamera(float *origin, float (*axis)[3])
{
    if (!CL_SplitGuestHasView())
        return false;
    float angles[3];
    if (CL_SplitGuestPredictedCamera(origin, angles))
    {
        AnglesToAxis(angles, axis);
        return true;
    }
    const playerState_s &player = clients[1].snap.ps;
    std::memcpy(origin, player.origin, sizeof(player.origin));
    origin[2] += player.viewHeightCurrent;
    if (!CL_SplitGuestGetViewAngles(angles))
        std::memcpy(angles, player.viewangles, sizeof(angles));
    AnglesToAxis(angles, axis);
    return true;
}

void CL_SplitGuestUpdateSound()
{
    static bool listening = false;
    float origin[3], axis[3][3];
    if (CL_SplitGuestGetWorldCamera(origin, axis))
    {
        const int clientNum = clients[1].snap.ps.clientNum;
        const cg_s *world = CG_GetLocalClientGlobals(0);
        const team_t team = clientNum >= 0 && clientNum < 32
            ? world->bgs.clientinfo[clientNum].team : TEAM_FREE;
        SND_SetListener(1, clientNum, team, origin, axis);
        listening = true;
    }
    else if (listening)
    {
        SND_DisconnectListener(1);
        listening = false;
    }
}

void CL_SplitGuestApplyHostViewport(refdef_s *view)
{
    LocalClientViewport bounds;
    if (!CL_SplitGuestHasViewport() || !CL_CalculateLocalClientViewport(
        cls.vidConfig.displayWidth, cls.vidConfig.displayHeight, 0, 2, &bounds))
        return;
    // Preserve the vertical field of view and account for non-square pixels.
    const float fullAspect = CG_GetLocalClientStaticGlobals(0)->viewAspect;
    view->tanHalfFovX = view->tanHalfFovY * fullAspect
        * cls.vidConfig.displayHeight / bounds.height;
    view->x = bounds.x; view->y = bounds.y;
    view->width = bounds.width; view->height = bounds.height;
    view->useScissorViewport = false;
    view->splitscreen = true;
}

void CL_SplitGuestRender()
{
#ifdef KISAK_NX
    NX_STALL_SCOPED("P2 render");
#endif

    if (!CL_SplitGuestHasView() || frontEndDataOut->viewInfoCount != 1)
        return;
    LocalClientViewport bounds;
    if (!CL_CalculateLocalClientViewport(cls.vidConfig.displayWidth,
        cls.vidConfig.displayHeight, 1, 2, &bounds))
        return;
    // refdef is large. Give this camera its own persistent storage rather than
    // copying an entire cg_s (which owns animation trees and snapshot pointers).
    static refdef_s view;
    const cg_s *host = CG_GetLocalClientGlobals(0);
    view = host->refdef;
    view.x = bounds.x; view.y = bounds.y;
    view.width = bounds.width; view.height = bounds.height;
    view.tanHalfFovX = view.tanHalfFovY * CG_GetLocalClientStaticGlobals(0)->viewAspect
        * cls.vidConfig.displayHeight / bounds.height;
    const playerState_s &player = clients[1].snap.ps;
    std::memcpy(view.vieworg, player.origin, sizeof(view.vieworg));
    view.vieworg[2] += player.viewHeightCurrent;
    view.playerTeleported = !s_splitGuestViewSubmitted;
    CL_SplitGuestSmoothCamera(&view);
    float angles[3];
    if (!CL_SplitGuestPredictedCamera(view.vieworg, angles)
        && !CL_SplitGuestGetViewAngles(angles))
        std::memcpy(angles, player.viewangles, sizeof(angles));
    AnglesToAxis(angles, view.viewaxis);
    view.yaw = angles[1];
    SND_SetListener(1, player.clientNum, host->bgs.clientinfo[player.clientNum].team,
        view.vieworg, view.viewaxis);
    std::memcpy(CG_GetLocalClientGlobals(1)->predictedPlayerState.viewangles,
        angles, sizeof(angles));
    view.useScissorViewport = false;
    view.extraCamPosValid = false;
    view.splitscreen = true;
    // This pass draws the already submitted world from a different camera.
    // The primary still owns entity models/FX; no second CG_Init or simulation.
    view.localClientNum = 0;
    view.blurRadius = 0.0f;
    std::memset(&view.doubleVision, 0, sizeof(view.doubleVision));
    std::memset(&view.saveScreenFx, 0, sizeof(view.saveScreenFx));
    CL_SplitGuestApplyVision(&view);
    CG_GetLocalClientGlobals(1)->refdef = view;
    if (!CG_SplitGuestDrawHud(&view))
        R_ClearClientCmdList2D();
    R_RenderSharedGuestView(&view, host->frametime, player.clientNum);
    if (!s_splitGuestViewSubmitted && frontEndDataOut->viewInfoCount == 2)
    {
        s_splitGuestViewSubmitted = true;
        CL_SplitGuestLog("First guest snapshot camera submitted (host top, guest bottom)");
    }
}

static void CL_SplitGuestDrainPackets()
{
    unsigned char data[1264];
    msg_t message;
    netadr_t from;
    MSG_Init(&message, data, sizeof(data));
    while (NET_GetLoopPacket(Com_LocalClient_GetNetworkID(1), &from, &message))
        ;
}

void CL_SplitGuestRemove(bool notifyServer)
{
    if (!s_splitGuestConnected)
        return;
    if (notifyServer && clients && clientConnections && com_sv_running->current.enabled
        && CL_GetLocalClientConnectionState(1) >= CA_CONNECTED)
    {
        CL_AddReliableCommand(1, "disconnect");
        CL_WritePacket(1);
        CL_WritePacket(1);
        CL_WritePacket(1);
    }
    CL_SplitGuestLog("Removing guest");
    CL_SplitGuestResetMenu();
    s_splitGuestConnected = false;
    CL_SplitGuestResetInput();
    CL_SplitGuestResetPresentation();
    s_splitGuestViewSubmitted = false;
    // Wait for skinning/culling before releasing the guest's private DObj.
    R_WaitFrontendWorkerCmds();
    CG_SplitGuestFreeViewmodel();
    // Do not use CL_Disconnect: its audio/profile/cinematic teardown is shared.
    if (clientConnections)
        std::memset(&clientConnections[1], 0, sizeof(clientConnections[1]));
    if (clients)
        std::memset(&clients[1], 0, sizeof(clients[1]));
    std::memset(&clientUIActives[1], 0, sizeof(clientUIActives[1]));
    Com_LocalClient_SetBeingUsed(1, false);
    CL_SplitGuestDrainPackets();
    Com_Printf(14, "Local guest removed; host resources retained.\n");
}

void CL_SplitGuestBeginMap()
{
    // A full map load retains the server's network clients. Retire the old
    // local guest before its client state is cleared, otherwise its new join
    // collides with the still-connected slot at the same loopback return port.
    // The caller synchronizes server/frontend work before replacing the map.
    if (s_splitGuestConnected && com_sv_running->current.enabled && svs.clients
        && clientConnections && com_maxclients)
    {
        const auto &connection = clientConnections[1];
        // The qport and return queue identify P2 even before its first
        // gamestate assigns clientNum. Never retire the host or a remote peer.
        for (int slot = 0; slot < com_maxclients->current.integer; ++slot)
        {
            if (slot == clientConnections[0].clientNum) continue;
            client_t *serverGuest = &svs.clients[slot];
            const auto &channel = serverGuest->header.netchan;
            if (serverGuest->header.state >= CS_CONNECTED && channel.qport == connection.qport
                && channel.remoteAddress.type == NA_LOOPBACK
                && channel.remoteAddress.port == Com_LocalClient_GetNetworkID(1))
            {
                CL_SplitGuestLog("Retiring old local server guest before map reload");
                SV_DropClient(serverGuest, "EXE_DISCONNECTED", false, false);
                break;
            }
        }
    }
    CL_SplitGuestRemove(false);
    s_splitGuestAutoAttempted = false;
}

static void CL_SplitGuestAdd_f()
{
    if (!CL_SplitScreenSessionRequested()) return;
    if (s_splitGuestConnected)
    {
        CL_SplitGuestLog("Add ignored: guest already exists");
        Com_Printf(14, "A local guest already exists; use splitguest_status or splitguest_remove.\n");
        return;
    }
    if (!com_sv_running->current.enabled || !clients || !clientConnections
        || CL_GetLocalClientConnectionState(0) < CA_PRIMED)
    {
        CL_SplitGuestLog("Add rejected: host is not active");
        Com_Printf(14, "Start a local game before adding the guest.\n");
        return;
    }
#ifdef KISAK_SP
    const bool supportedMap = Com_IsZombieMap(Dvar_GetString("mapname"));
#else
    const bool supportedMap = !Dvar_GetBool("onlinegame") && Dvar_GetBool("systemlink")
        && !std::strncmp(Dvar_GetString("mapname"), "mp_", 3);
#endif
    if (!supportedMap || com_maxclients->current.integer < 2)
    {
        CL_SplitGuestLog("Add rejected: Supported local map or server capacity missing");
        Com_Printf(14, "The prototype needs a supported local map and systemlink 1.\n");
        return;
    }
    if (cl_maxLocalClients < 2)
    {
        CL_SplitGuestLog("Add rejected: fewer than two allocated local clients");
        Com_Printf(14, "Two local-client slots were not allocated; restart with the experimental build.\n");
        return;
    }
    CL_SplitGuestDrainPackets();
    CL_SplitGuestResetInput();
    CL_SplitGuestResetPresentation();
    std::memset(&clients[1], 0, sizeof(clients[1]));
    std::memset(&clientConnections[1], 0, sizeof(clientConnections[1]));
    std::memset(&clientUIActives[1], 0, sizeof(clientUIActives[1]));
    // The physical guest pad is separate; account/profile settings remain shared.
    Com_LocalClient_SetControllerIndex(1, Com_LocalClient_GetControllerIndex(0));
    Com_LocalClient_SetBeingUsed(1, false);
    clientConnection_t *guest = &clientConnections[1];
    const unsigned int hostQport = static_cast<unsigned int>(clientConnections[0].qport) & 0xFFFF;
    unsigned int qport = 1 + ((hostQport + ++s_splitGuestSerial) % 65535);
    if (qport == hostQport)
        qport = 1 + (qport % 65535);
    // Netchan_Init and MSG_ReadShort both use a signed 16-bit qport.
    // Preserve that representation in the join userinfo and channel too;
    // positive 32768..65535 would never match the server's decoded packets.
    guest->qport = static_cast<short>(qport);
    guest->connectTime = cls.realtime - 3000;
    guest->lastPacketSentTime = -9999;
    NET_StringToAdr("localhost", &guest->serverAddress);
    clientUIActives[1].flags = 2 | 4;
    CL_SetLocalClientConnectionState(1, CA_CHALLENGING);
    s_splitGuestStartTime = cls.realtime;
    s_splitGuestConnected = true;
    s_splitGuestAutoAttempted = true;
    CL_CheckForResend(1);
    CL_SplitGuestLog("Guest join requested");
    Com_Printf(14, "Local guest connecting (qport %u). Separate gamepad input; the second camera opens when the guest spawns.\n", qport);
}

bool CL_SplitGuestPrime(int localClientNum)
{
    if (!CL_IsSplitGuest(localClientNum))
        return false;
    CL_SetLocalClientConnectionState(localClientNum, CA_PRIMED);
    CL_SendPureChecksums(localClientNum);
    CL_WritePacket(localClientNum);
    CL_SplitGuestLog("Gamestate received; sending initial commands");
    return true;
}

static void CL_SplitGuestRemove_f()
{
    s_splitGuestAutoAttempted = true; // Explicit removal lasts until the next map.
    CL_SplitGuestRemove(true);
}

static void CL_SplitGuestStatus_f()
{
    CL_SplitGuestLog("Status requested");
    if (!s_splitGuestConnected)
    {
        Com_Printf(14, "No local guest. Choose Split-Screen from the frontend menu.\n");
        return;
    }
    clientActive_t *guest = &clients[1];
    const int clientNum = clientConnections[1].clientNum;
    const char *guestName = "(connecting)";
    if (CL_GetLocalClientConnectionState(1) >= CA_PRIMED
        && clientNum >= 0 && clientNum < com_maxclients->current.integer)
        guestName = svs.clients[clientNum].name;
    Com_Printf(14, "Guest: %s, connection %d, entity %d, snapshot %d, player mode %d, origin %.1f %.1f %.1f\n",
        guestName, CL_GetLocalClientConnectionState(1), clientNum, guest->snap.valid,
        guest->snap.ps.pm_type, guest->snap.ps.origin[0], guest->snap.ps.origin[1], guest->snap.ps.origin[2]);
}


void CL_SplitGuestFrame()
{
#ifdef KISAK_NX
    NX_STALL_SCOPED("P2 network/input frame");
#endif

    if (s_splitGuestConnected && !CL_SplitScreenSessionRequested())
    {
        CL_SplitGuestRemove(true);
        return;
    }
    if (!s_splitGuestConnected && !s_splitGuestAutoAttempted
        && CL_SplitGuestExpectedPlayers() == 2 && com_sv_running->current.enabled
        && clients && clientConnections && cl_maxLocalClients >= 2
        && CL_GetLocalClientConnectionState(0) >= CA_PRIMED)
    {
        CL_SplitScreenSessionActivated();
        CL_SplitGuestAdd_f();
        if (s_splitGuestConnected)
            CL_SplitGuestLog("Automatic initial local guest join requested");
    }
    if (!s_splitGuestConnected)
        return;
    if (!com_sv_running->current.enabled || CL_GetLocalClientConnectionState(0) < CA_PRIMED)
    {
        CL_SplitGuestLog("Host stopped or left active play");
        CL_SplitGuestRemove(false);
        return;
    }
    clientActive_t *guest = &clients[1];
    clientConnection_t *connection = &clientConnections[1];
    const connstate_t state = CL_GetLocalClientConnectionState(1);
    static int lastReportTime;
    static int lastState = -1;
    static int lastPlayerMode = -1;
    if (state != lastState || (guest->snap.valid && guest->snap.ps.pm_type != lastPlayerMode)
        || static_cast<unsigned int>(cls.realtime - lastReportTime) >= 1000)
    {
        CL_SplitGuestLog("Guest progress");
        lastReportTime = cls.realtime;
        lastState = state;
        lastPlayerMode = guest->snap.valid ? guest->snap.ps.pm_type : -1;
    }
    if (state < CA_CONNECTED)
    {
        if (static_cast<unsigned int>(cls.realtime - s_splitGuestStartTime) > 30000)
        {
            CL_SplitGuestLog("Join timed out");
            Com_PrintWarning(14, "Local guest connection timed out.\n");
            CL_SplitGuestRemove(false);
            return;
        }
        CL_CheckForResend(1);
        return;
    }
    if (guest->serverId && guest->serverId != clients[0].serverId)
    {
        CL_SplitGuestLog("Host map/restart changed");
        Com_Printf(14, "Host map/restart changed; remove the guest and add it again once the host is ready.\n");
        CL_SplitGuestRemove(true);
        return;
    }
    if (connection->lastPacketTime > 0
        && cls.realtime - connection->lastPacketTime > cl_timeout->current.value * 1000.0f)
    {
        CL_SplitGuestLog("Snapshot timeout");
        Com_PrintWarning(14, "Local guest stopped receiving snapshots; removing it without dropping the host.\n");
        CL_SplitGuestRemove(true);
        return;
    }
    // Consume this connection's reliable sequence without running UI/CSC
    // callbacks against the host. The native disconnect opcode is 'w'.
    const int firstCommand = connection->lastExecutedServerCommand + 1;
    for (int sequence = firstCommand > connection->serverCommandSequence - 127
        ? firstCommand : connection->serverCommandSequence - 127;
        sequence <= connection->serverCommandSequence; ++sequence)
    {
        const char *text = connection->serverCommands[sequence & 0x7F];
#ifndef KISAK_SP
        CL_SplitGuestHandleMenuCommand(text);
#endif
        // Native G_SelectWeaponIndex sends opcode 'a'. Handle this private
        // gameplay state without dispatching a second UI/clientscript lifecycle.
        if (text[0] == 'a' && (text[1] == ' ' || text[1] == '\t'))
        {
            char *end = nullptr;
            const long weapon = std::strtol(text + 2, &end, 10);
            if (end != text + 2 && weapon >= 0 && weapon < BG_GetNumWeapons())
            {
                CL_SplitGuestSelectWeapon(static_cast<unsigned int>(weapon));
                CL_SplitGuestLog("Server requested guest weapon selection");
            }
        }
        if (text[0] == 'w' && (!text[1] || text[1] == ' '))
        {
            CL_SplitGuestLog("Server sent a reliable disconnect");
            Com_PrintWarning(14, "Server removed the local guest: %s\n", text + 1);
            CL_SplitGuestRemove(false);
            return;
        }
    }
    connection->lastExecutedServerCommand = connection->serverCommandSequence;
    CL_CheckUserinfo(1);
    if (state >= CA_PRIMED)
    {
        if (state == CA_PRIMED && guest->snap.valid && !(guest->snap.snapFlags & 2))
        {
            CL_SetLocalClientConnectionState(1, CA_ACTIVE);
            CL_SplitGuestLog("First active snapshot received");
            Com_Printf(14, "Local guest received its first active snapshot, entity %d.\n", connection->clientNum);
        }
        // The first command admits the client to the world; waiting for a
        // valid snapshot here would deadlock before the server sends one.
        // Snapshot headers can be ahead of the displayed clock. Using their
        // maximum pins P2's commands/angles to the server's ~20 Hz tick rate.
        // Match the primary's continuous command clock instead.
        guest->serverTime = clients[0].serverTime;
        usercmd_s command;
        MSG_SetDefaultUserCmd(&guest->snap.ps, &command);
        command.serverTime = guest->serverTime;
        command.button_bits.array[0] = command.button_bits.array[1] = 0;
        command.forwardmove = command.rightmove = 0;
        CL_SplitGuestBuildInput(&command, &guest->snap.ps, cls.realtime, cls.frametime,
            guest->snap.valid && state >= CA_ACTIVE);
        // A command each displayed frame, without duplicates while the server
        // clock is paused. Replay needs the preceding command in the same ring.
        if (!guest->cmdNumber || command.serverTime > guest->cmds[guest->cmdNumber & 0x7F].serverTime)
            guest->cmds[++guest->cmdNumber & 0x7F] = command;
        CL_SplitGuestUpdatePrediction();
        cg_s *game = CG_GetLocalClientGlobals(1);
        game->clientNum = connection->clientNum;
#ifdef KISAK_SP
        game->lastAppliedAnimCommandTime = guest->snap.serverTime;
#endif
        guest->newSnapshots = false;
    }
    CL_SendCmd(1);
}

static cmd_function_s s_splitGuestRemoveCommand;
static cmd_function_s s_splitGuestStatusCommand;

void CL_SplitGuestRegisterCommands()
{
    CL_SplitGuestRegisterInputDvars();
    _Dvar_RegisterBool("splitguest_predict", true, 1, "Predict player two movement and viewmodel from its pending commands");
    _Dvar_RegisterBool("splitguest_view", true, 0, "Show the experimental guest snapshot camera after spawning");
    CL_SplitGuestLog("Experimental guest commands registered; connection states 5=joining, 9=primed, 10=active");
    Cmd_AddCommandInternal("splitguest_remove", CL_SplitGuestRemove_f, &s_splitGuestRemoveCommand);
    Cmd_AddCommandInternal("splitguest_status", CL_SplitGuestStatus_f, &s_splitGuestStatusCommand);
}
#endif
