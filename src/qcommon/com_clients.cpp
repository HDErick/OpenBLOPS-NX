#include "com_clients.h"

#include <cstring>
#include <universal/com_memory.h>
#include <client/cl_main.h>
#include <client/splitscreen.h>
#include <client_mp/cl_main_mp.h>
#include <cgame_mp/cg_local_mp.h>

ClientGameState clientGameStates[MAX_LOCAL_CLIENTS];

void __cdecl Com_InitClientGameStates()
{
    for (int client = 0; client < MAX_LOCAL_CLIENTS; ++client)
    {
        ClientGameState &state = clientGameStates[client];
        state.flags = 0;
        state.localClientNum = client;
        state.controllerIndex = client;
        state.uiContextIndex = 0;
        state.networkID = static_cast<netsrc_t>(client);
    }
}

void __cdecl Com_LocalClients_CompressClients()
{
    int testIndex; // [esp+60h] [ebp-18h]
    int swapIndex; // [esp+64h] [ebp-14h]
    int localClientNum; // [esp+68h] [ebp-10h]
    int activeIndex; // [esp+6Ch] [ebp-Ch]
    int cgsIndex; // [esp+70h] [ebp-8h]
    int lastUsedIndex; // [esp+74h] [ebp-4h]

    lastUsedIndex = -1;
    for (cgsIndex = 0; cgsIndex < MAX_LOCAL_CLIENTS; ++cgsIndex)
    {
        if (Com_LocalClient_IsBeingUsed(cgsIndex))
        {
            lastUsedIndex = cgsIndex;
        }
        else
        {
            for (activeIndex = cgsIndex + 1; activeIndex < MAX_LOCAL_CLIENTS; ++activeIndex)
            {
                if (Com_LocalClient_IsBeingUsed(activeIndex))
                {
                    SwapClients(cgsIndex, activeIndex);
                    lastUsedIndex = cgsIndex;
                    break;
                }
            }
        }
    }
    for (localClientNum = 0; localClientNum <= lastUsedIndex; ++localClientNum)
    {
        if (!Com_LocalClient_IsBeingUsed(localClientNum)
            && !Assert_MyHandler(
                "C:\\projects_pc\\cod\\codsrc\\src\\qcommon\\com_clients.cpp",
                108,
                0,
                "%s",
                "Com_LocalClient_IsBeingUsed(cgsIndex)"))
        {
            __debugbreak();
        }
        swapIndex = localClientNum;
        for (testIndex = localClientNum + 1; testIndex <= lastUsedIndex; ++testIndex)
        {
            if (clientGameStates[swapIndex].controllerIndex > clientGameStates[testIndex].controllerIndex)
                swapIndex = testIndex;
        }
        if (swapIndex != localClientNum)
            SwapClients(localClientNum, swapIndex);
    }
}

void __cdecl SwapClients(int clientA, int clientB)
{
    clientUIActive_t *v2; // eax
    clientUIActive_t *v3; // eax
    clientUIActive_t *v4; // ecx
    ClientGameState *v5; // ecx
    ClientGameState *v6; // edx
    int clientGamestateTemp; // [esp+0h] [ebp-30h]
    int clientGamestateTemp_4; // [esp+4h] [ebp-2Ch]
    int clientGamestateTemp_8; // [esp+8h] [ebp-28h]
    int clientGamestateTemp_12; // [esp+Ch] [ebp-24h]
    netsrc_t clientGamestateTemp_16; // [esp+10h] [ebp-20h]
    int uiActiveTemp; // [esp+14h] [ebp-1Ch]
    int uiActiveTemp_4; // [esp+18h] [ebp-18h]
    connstate_t uiActiveTemp_8; // [esp+1Ch] [ebp-14h]
    int uiActiveTemp_12; // [esp+20h] [ebp-10h]
    unsigned __int8 *clientConnectionTemp; // [esp+24h] [ebp-Ch]
    LargeLocal clientConnectionTemp_large_local(sizeof(clientConnection_t)); // [esp+28h] [ebp-8h] BYREF

    //LargeLocal::LargeLocal(&clientConnectionTemp_large_local, 720712);
    //clientConnectionTemp = LargeLocal::GetBuf(&clientConnectionTemp_large_local);
    clientConnectionTemp = clientConnectionTemp_large_local.GetBuf();
    v2 = &clientUIActives[clientA];
    uiActiveTemp = v2->flags;
    uiActiveTemp_4 = v2->keyCatchers;
    uiActiveTemp_8 = v2->connectionState;
    uiActiveTemp_12 = v2->nextScrollTime;
    v3 = &clientUIActives[clientB];
    v4 = &clientUIActives[clientA];
    v4->flags = v3->flags;
    v4->keyCatchers = v3->keyCatchers;
    v4->connectionState = v3->connectionState;
    v4->nextScrollTime = v3->nextScrollTime;
    v3->flags = uiActiveTemp;
    v3->keyCatchers = uiActiveTemp_4;
    v3->connectionState = uiActiveTemp_8;
    v3->nextScrollTime = uiActiveTemp_12;
    v5 = &clientGameStates[clientA];
    clientGamestateTemp = v5->flags;
    clientGamestateTemp_4 = v5->localClientNum;
    clientGamestateTemp_8 = v5->controllerIndex;
    clientGamestateTemp_12 = v5->uiContextIndex;
    clientGamestateTemp_16 = v5->networkID;
    v6 = &clientGameStates[clientB];
    v5->flags = v6->flags;
    v5->localClientNum = v6->localClientNum;
    v5->controllerIndex = v6->controllerIndex;
    v5->uiContextIndex = v6->uiContextIndex;
    v5->networkID = v6->networkID;
    v6->flags = clientGamestateTemp;
    v6->localClientNum = clientGamestateTemp_4;
    v6->controllerIndex = clientGamestateTemp_8;
    v6->uiContextIndex = clientGamestateTemp_12;
    v6->networkID = clientGamestateTemp_16;
    clientGameStates[clientA].localClientNum = clientA;
    clientGameStates[clientB].localClientNum = clientB;
    if (clientConnections)
    {
        memcpy(clientConnectionTemp, (unsigned __int8 *)&clientConnections[clientA], sizeof(clientConnection_t));
        memcpy(
            (unsigned __int8 *)&clientConnections[clientA],
            (unsigned __int8 *)&clientConnections[clientB],
            sizeof(clientConnection_t));
        memcpy((unsigned __int8 *)&clientConnections[clientB], clientConnectionTemp, sizeof(clientConnection_t));
    }
    //LargeLocal::~LargeLocal(&clientConnectionTemp_large_local);
}

void __cdecl Com_LocalClients_AssignUIContextsForInGame()
{
    int cgsIndex; // [esp+0h] [ebp-8h]
    int contextIndex; // [esp+4h] [ebp-4h]

    contextIndex = 0;
    for (cgsIndex = 0; cgsIndex < MAX_LOCAL_CLIENTS; ++cgsIndex)
        clientGameStates[cgsIndex].uiContextIndex = contextIndex++;
}

void __cdecl Com_LocalClients_AssignUIContextsForFrontEnd()
{
    int cgsIndex; // [esp+0h] [ebp-8h]
    int contextIndex; // [esp+4h] [ebp-4h]

    contextIndex = 0;
    for (cgsIndex = 0; cgsIndex < MAX_LOCAL_CLIENTS; ++cgsIndex)
    {
        clientGameStates[cgsIndex].uiContextIndex = 0;
        ++contextIndex;
    }
}

int __cdecl Com_LocalClient_GetUIContextIndex(int localClientNum)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
#ifdef KISAK_EXPERIMENTAL_SPLITSCREEN
    // The guest has its own game state, but borrows the one loaded PC UI.
    // Map startup still assigns game slot 1 context index 1; no UI storage or
    // menu runtime exists at that index. Keep UI queries on the shared context.
    if (CL_IsSplitGuest(localClientNum))
        return clientGameStates[0].uiContextIndex;
#endif
    return clientGameStates[localClientNum].uiContextIndex;
}

int __cdecl Com_LocalClient_GetControllerIndex(int localClientNum)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
    return clientGameStates[localClientNum].controllerIndex;
}

netsrc_t __cdecl Com_LocalClient_GetNetworkID(int localClientNum)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
    return clientGameStates[localClientNum].networkID;
}

void __cdecl Com_LocalClient_SetControllerIndex(int localClientNum, int controllerIndex)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
    bcassert(static_cast<unsigned int>(controllerIndex), MAX_LOCAL_CLIENTS);
    clientGameStates[localClientNum].controllerIndex = controllerIndex;
}

void __cdecl Com_LocalClient_SetPrimary(int localClientNum, bool primary)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
    if (primary)
        Com_LocalClient_ClearAllPrimary();
    if (primary)
        clientGameStates[localClientNum].flags |= 2;
    else
        clientGameStates[localClientNum].flags &= ~2;
}

int Com_LocalClient_ClearAllPrimary()
{
    int result = 0; // eax
    int i; // [esp+0h] [ebp-4h]

    for ( i = 0; i < MAX_LOCAL_CLIENTS; ++i )
    {
        clientGameStates[i].flags &= ~2u;
        result = i + 1;
    }
    return result;
}

int __cdecl Com_LocalClients_GetPrimary()
{
    int i; // [esp+0h] [ebp-4h]

    for ( i = 0; i < MAX_LOCAL_CLIENTS; ++i )
    {
        if ( Com_LocalClient_IsPrimary(i) )
            return i;
    }

    if ( !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\qcommon\\com_clients.cpp",
                    261,
                    0,
                    "%s\n\tCom_LocalClient_IsPrimarys",
                    "0",
                    "Someone asked for the primary client and it was not set\n") )
        __debugbreak();

    return -1;
}

bool __cdecl Com_LocalClient_IsPrimary(int localClientNum)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
    return (clientGameStates[localClientNum].flags & 2) != 0;
}

bool __cdecl Com_LocalClient_IsBeingUsed(int localClientNum)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
    return (clientGameStates[localClientNum].flags & 1) != 0;
}

void __cdecl Com_LocalClient_SetBeingUsed(int localClientNum, bool beingUsed)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
    if (beingUsed)
        clientGameStates[localClientNum].flags |= 1;
    else
        clientGameStates[localClientNum].flags &= ~1;
}

int __cdecl Com_LocalClients_GetUsedControllerCount()
{
    int i; // [esp+0h] [ebp-8h]
    int count; // [esp+4h] [ebp-4h]

    i = 0;
    count = 0;
    while ( i < MAX_LOCAL_CLIENTS )
    {
        if ( Com_LocalClient_IsBeingUsed(i) )
            ++count;
        ++i;
    }
    return count;
}

int __cdecl Com_ControllerIndex_GetLocalClientNum(int controllerIndex)
{
    bcassert(static_cast<unsigned int>(controllerIndex), MAX_LOCAL_CLIENTS);
    for (int client = 0; client < MAX_LOCAL_CLIENTS; ++client)
    {
        if (clientGameStates[client].controllerIndex == controllerIndex)
            return clientGameStates[client].localClientNum;
    }
    return -1;
}

int __cdecl Com_ControllerIndex_GetUIContextIndex(int controllerIndex)
{
    bcassert(static_cast<unsigned int>(controllerIndex), MAX_LOCAL_CLIENTS);
    for (int client = 0; client < MAX_LOCAL_CLIENTS; ++client)
    {
        if (clientGameStates[client].controllerIndex == controllerIndex)
            return clientGameStates[client].uiContextIndex;
    }
    return -1;
}

int __cdecl Com_ControllerIndexes_GetPrimary()
{
#ifdef KISAK_SP
    for (int primaryClient = 0; primaryClient < MAX_LOCAL_CLIENTS; ++primaryClient)
    {
        if ((clientGameStates[primaryClient].flags & 2) == 0)
            continue;

        for (int client = 0; client < MAX_LOCAL_CLIENTS; ++client)
        {
            if (clientGameStates[client].localClientNum == primaryClient)
                return clientGameStates[client].controllerIndex;
        }
        return -1;
    }
    return -1;
#else
    // There is some extra logic here for IsDedicatedServer() but I believe it will always return 0 unless we enable splitscreen (KISAKTODO)
    return 0;
#endif
}

