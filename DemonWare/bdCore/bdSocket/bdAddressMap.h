#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdSocket/bdAddr.h>
#include <DemonWare/bdCore/bdContainers/bdNetworkArray.h>

struct bdAddressMap : bdNetworkArray<bdReference<bdAddrHandle>>
{
    bool getAddrHandle(bdReference<bdCommonAddr> addr, const bdSecurityID &id, bdReference<bdAddrHandle> &out);
    bool findAddrHandle(bdReference<bdCommonAddr> addr, const bdSecurityID &id, bdReference<bdAddrHandle> &out);
    bool addrToCommonAddr(const bdReference<bdAddrHandle> &handle, bdReference<bdCommonAddr> &addr, bdSecurityID &id);
    bool unregisterAddr(const bdReference<bdAddrHandle> &handle);
    bool unregisterRealAddr(bdAddr addr);
private:
    void eraseAddrHandle(unsigned int index);
};
static_assert(sizeof(void *) != 4 || sizeof(bdAddressMap) == 12);
#endif
