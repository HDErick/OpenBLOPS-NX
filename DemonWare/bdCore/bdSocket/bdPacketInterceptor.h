#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdSocket/bdSocket.h>

struct bdPacketInterceptor
{
    virtual ~bdPacketInterceptor() = default;
    virtual bool acceptPacket(bdSocket *, bdAddr, void *, unsigned int, unsigned char) = 0;
};
#endif
