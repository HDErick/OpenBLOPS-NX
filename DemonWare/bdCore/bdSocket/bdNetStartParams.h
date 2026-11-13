#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdSocket/bdAddr.h>
#include <DemonWare/bdCore/bdContainers/bdNetworkArray.h>
#include <cstddef>

struct bdSocket;
struct bdGetHostByNameConfig
{
    bdGetHostByNameConfig() : m_timeout(10.0f) {}
    float m_timeout;
};

struct bdUPnPConfig
{
    bdUPnPConfig() : m_discoveryTimeout(3.0f), m_timeout04(10.0f), m_timeout08(5.0f),
        m_value0C(0), m_value10(1), m_flag14(false), m_gatewayAddr() {}
    float m_discoveryTimeout;
    float m_timeout04;
    float m_timeout08;
    unsigned int m_value0C;
    unsigned int m_value10;
    bool m_flag14;
    unsigned char m_padding15[3];
    bdInetAddr m_gatewayAddr;
};

struct bdNetStartParams
{
    bdNetStartParams();
    bdNetStartParams &operator=(const bdNetStartParams &other);
    bool m_onlineGame;
    unsigned char m_padding01;
    unsigned short m_gamePort;
    bdSocket *m_socket;
    bdArray<bdString> m_natTravHosts;
    unsigned short m_natTravPort;
    unsigned char m_padding16[2];
    bdArray<bdInetAddr> m_localAddresses;
    bdGetHostByNameConfig m_hostNameLookupConfig;
    bdUPnPConfig m_UPnPConfig;
    bool m_useAnyIP;
    unsigned char m_padding45[3];
};
static_assert(sizeof(bdUPnPConfig) == 0x1C);
static_assert(sizeof(void *) != 4 || sizeof(bdNetStartParams) == 0x48);
static_assert(sizeof(void *) != 4 || offsetof(bdNetStartParams, m_natTravHosts) == 8);
static_assert(sizeof(void *) != 4 || offsetof(bdNetStartParams, m_localAddresses) == 0x18);
static_assert(sizeof(void *) != 4 || offsetof(bdNetStartParams, m_UPnPConfig) == 0x28);
static_assert(sizeof(void *) != 4 || offsetof(bdNetStartParams, m_useAnyIP) == 0x44);
#endif
