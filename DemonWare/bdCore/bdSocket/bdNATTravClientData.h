#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdSocket/bdNATTravListener.h>
#include <DemonWare/bdCore/bdTiming/bdStopwatch.h>
#include <cstddef>

class bdNATTravClientData
{
public:
    unsigned int m_stage;
    bdReference<bdCommonAddr> m_localAddr;
    bdReference<bdCommonAddr> m_remoteAddr;
    bdNATTravListener *m_listener;
    bdNATTravListener *m_secondaryListener;
    unsigned int m_retryCount;
    bdStopwatch m_lastSent;
    bool m_throttled;
    bdStopwatch m_age;

    bdNATTravClientData()
        : m_stage(0), m_listener(nullptr), m_secondaryListener(nullptr),
          m_retryCount(0), m_throttled(false) {}

    bdNATTravClientData(bdReference<bdCommonAddr> localAddr,
        bdReference<bdCommonAddr> remoteAddr, bdNATTravListener *listener)
        : m_stage(1), m_localAddr(localAddr), m_remoteAddr(remoteAddr),
          m_listener(listener), m_secondaryListener(nullptr), m_retryCount(0),
          m_throttled(false) {}

    bdNATTravClientData(const bdNATTravClientData &other)
        : m_stage(other.m_stage), m_localAddr(other.m_localAddr),
          m_remoteAddr(other.m_remoteAddr), m_listener(other.m_listener),
          m_secondaryListener(other.m_secondaryListener), m_retryCount(other.m_retryCount),
          m_lastSent(other.m_lastSent), m_throttled(other.m_throttled), m_age(other.m_age) {}

    void success(bdReference<bdCommonAddr> addr, const bdAddr &realAddr)
    {
        m_listener->onNATAddrDiscovery(addr, realAddr);
        if (m_secondaryListener)
            m_secondaryListener->onNATAddrDiscovery(addr, realAddr);
    }

    void failure(bdReference<bdCommonAddr> addr)
    {
        m_listener->onNATAddrDiscoveryFailed(addr);
        if (m_secondaryListener)
            m_secondaryListener->onNATAddrDiscoveryFailed(addr);
    }
};
static_assert(sizeof(void *) != 4 || sizeof(bdNATTravClientData) == 0x30);
static_assert(sizeof(void *) != 4 || offsetof(bdNATTravClientData, m_lastSent) == 0x18);
static_assert(sizeof(void *) != 4 || offsetof(bdNATTravClientData, m_age) == 0x28);
#endif
