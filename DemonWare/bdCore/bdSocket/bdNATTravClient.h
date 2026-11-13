#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdSocket/bdPacketInterceptor.h>
#include <DemonWare/bdCore/bdSocket/bdBandwidthArbitrator.h>
#include <DemonWare/bdCore/bdSocket/bdNATTravClientData.h>
#include <DemonWare/bdCore/bdSocket/bdNATTravPacket.h>
#include <DemonWare/bdCore/bdContainers/bdHashMap.h>
#include <DW/dwUtils_pc.h>

struct bdNATTravClient : bdPacketInterceptor
{
    using RequestMap = bdHashMap<unsigned int, bdNATTravClientData, bdHashingClass>;
    bdBandwidthArbitrator *m_bandwidth;
    bdArray<bdAddr> m_introducers;
    bdSocket *m_socket;
    RequestMap m_requests;
    bdStopwatch m_lastKeepAlive;
    unsigned char m_key[28];
    unsigned int m_status;
    bdReference<bdCommonAddr> m_localAddr;
    inline static unsigned int m_statistics[2][4];

    bdNATTravClient() : m_requests(4, 0.75f), m_status(0)
    {
        for (unsigned int i = 0; i < 4; ++i)
        {
            m_statistics[1][i] = 0;
            m_statistics[0][i] = 0;
        }
    }

    ~bdNATTravClient() override
    {
        RequestMap::Node *iterator = m_requests.getIterator();
        while (iterator)
        {
            bdNATTravClientData data(iterator->m_value);
            char addr[1024];
            bdCommonAddrToString(data.m_remoteAddr, addr, sizeof(addr));
            bdLogMessage(BD_LOG_INFO, "info/", "bdSocket/nat",
                "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdNAT\\bdNATTravClient.cpp",
                "bdNATTravClient::~bdNATTravClient", 56, "NAT traversal to %s failed.", addr);
            data.failure(data.m_remoteAddr);
            m_requests.getNext(&iterator);
        }
        RequestMap::releaseIterator(iterator);
        m_requests.clear();
    }

    bool init(bdSocket *socket, bdBandwidthArbitrator *bandwidth,
        bdReference<bdCommonAddr> localAddr)
    {
        const char *message = nullptr;
        unsigned int line = 0;
        if (m_status)
        {
            message = "Cannot call init multiple times.";
            line = 76;
        }
        else if (!socket)
        {
            message = "A valid socket pointer is required by this class";
            line = 81;
        }
        else if (!bandwidth)
        {
            message = "A valid service bandwidth arbitrator pointer is required by this class";
            line = 86;
        }
        else if (!localAddr.m_ptr)
        {
            message = "A valid local common addr ref is required by this class";
            line = 91;
        }
        if (message)
        {
            bdLogMessage(BD_LOG_ERROR, "err/", "bdSocket/nat",
                "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdNAT\\bdNATTravClient.cpp",
                "bdNATTravClient::init", line, message);
            return false;
        }
        m_socket = socket;
        m_bandwidth = bandwidth;
        m_localAddr = localAddr;
        const bool result = sendKeepAlive();
        bdTrulyRandomImpl::getInstance()->getRandomUByte8(m_key, sizeof(m_key));
        m_status = 1;
        return result;
    }

    bool sendKeepAlive()
    {
        m_lastKeepAlive.start();
        if (!m_introducers.m_size)
            return true;
        bdLogMessage(BD_LOG_INFO, "info/", "bdSocket/nat",
            "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdNAT\\bdNATTravClient.cpp",
            "bdNATTravClient::sendKeepAlive", 595, "Sending keep alive to introducer(s).");
        const unsigned char type = 14;
        bdNATTravPacket packet(&type, 0, bdAddr(), bdAddr());
        unsigned char buffer[1288];
        unsigned int written;
        if (!packet.serialize(buffer, sizeof(buffer), 0, &written))
            return false;
        bool result = true;
        for (unsigned int i = 0; i < m_introducers.m_size; ++i)
        {
            if (m_socket->sendTo(m_introducers.m_data[i], buffer, written)
                != static_cast<int>(written))
            {
                bdLogMessage(BD_LOG_WARNING, "warn/", "bdSocket/nat",
                    "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdNAT\\bdNATTravClient.cpp",
                    "bdNATTravClient::sendKeepAlive", 612, "Failed to send. Invalid socket?");
                result = false;
            }
        }
        return result;
    }

    bool acceptPacket(bdSocket *, bdAddr, void *, unsigned int, unsigned char) override;
    void pump();
};
static_assert(sizeof(void *) != 4 || sizeof(bdNATTravClient) == 0x60);
#endif
