#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <cstdint>
#include <cstring>

struct bdBandwidthArbitrator
{
    std::int32_t m_quota;
    bool m_sent;

    bdBandwidthArbitrator() { reset(); }

    void reset()
    {
        m_quota = 0;
        m_sent = false;
    }

    void addSlice(std::int32_t slice)
    {
        if (m_quota / slice < 2 && (m_quota <= 0 || m_sent))
        {
            const std::uint32_t quota = static_cast<std::uint32_t>(m_quota)
                + static_cast<std::uint32_t>(slice);
            memcpy(&m_quota, &quota, sizeof(m_quota));
        }
        m_sent = false;
    }

    bool allowedSend(unsigned int) const { return m_quota > 0; }

    void send(unsigned int size)
    {
        const std::uint32_t quota = static_cast<std::uint32_t>(m_quota) - size;
        memcpy(&m_quota, &quota, sizeof(m_quota));
        m_sent = true;
    }
};
static_assert(sizeof(bdBandwidthArbitrator) == 8);
#endif
