#pragma once
#include "cl_main.h"
#include <client_mp/cl_main_mp.h>


inline connstate_t __cdecl CL_GetLocalClientConnectionState(int localClientNum)
{
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);
    return clientUIActives[localClientNum].connectionState;
}

extern clientActive_t *clients;
inline clientActive_t *__cdecl CL_GetLocalClientGlobals(int localClientNum)
{
    ///iassert(clients); // WOW! WEIRD
    bcassert(static_cast<unsigned int>(localClientNum), MAX_LOCAL_CLIENTS);

    return clients ? &clients[localClientNum] : nullptr;
}