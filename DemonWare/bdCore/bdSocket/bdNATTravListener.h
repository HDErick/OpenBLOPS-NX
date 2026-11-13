#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdSocket/bdAddr.h>

struct bdNATTravListener
{
    virtual ~bdNATTravListener() = default;
    virtual void onNATAddrDiscovery(bdReference<bdCommonAddr>, const bdAddr &) {}
    virtual void onNATAddrDiscoveryFailed(bdReference<bdCommonAddr>) {}
};
#endif
