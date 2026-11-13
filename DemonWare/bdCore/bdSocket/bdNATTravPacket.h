#pragma once

#if defined(KISAK_SP) && !defined(KISAK_DW)
#include <DemonWare/bdCore/bdSocket/bdAddr.h>
#include <DemonWare/bdCore/bdUtilities/bdBytePacker.h>
#include <cstddef>
#include <cstdint>

struct bdNATTravPacket
{
    std::uint8_t m_type;
    std::uint16_t m_version;
    std::uint8_t m_hmac[10];
    std::uint32_t m_identifier;
    bdAddr m_srcAddr;
    bdAddr m_destAddr;

    bdNATTravPacket() : m_type(0), m_version(0), m_identifier(0) {}

    bdNATTravPacket(const std::uint8_t *type, std::uint32_t identifier,
        const bdAddr &srcAddr, const bdAddr &destAddr)
        : m_type(*type), m_version(2), m_identifier(identifier),
          m_srcAddr(&srcAddr), m_destAddr(&destAddr) {}

    bool serialize(unsigned char *dst, unsigned int capacity,
        unsigned int offset, unsigned int *newOffset)
    {
        *newOffset = offset;
        unsigned char type = m_type;
        std::uint16_t version = m_version;
        std::uint32_t identifier = m_identifier;
        const bool result =
            bdBytePacker::appendBuffer(dst, capacity, *newOffset, newOffset, &type, 1)
            && bdBytePacker::appendBuffer(dst, capacity, *newOffset, newOffset,
                reinterpret_cast<unsigned char *>(&version), 2)
            && bdBytePacker::appendBuffer(dst, capacity, *newOffset, newOffset, m_hmac, 10)
            && bdBytePacker::appendBuffer(dst, capacity, *newOffset, newOffset,
                reinterpret_cast<unsigned char *>(&identifier), 4)
            && m_srcAddr.serialize(dst, capacity, *newOffset, newOffset)
            && m_destAddr.serialize(dst, capacity, *newOffset, newOffset);
        if (!result)
            *newOffset = offset;
        return result;
    }
};
static_assert(sizeof(bdNATTravPacket) == 0x24);
static_assert(offsetof(bdNATTravPacket, m_identifier) == 0x10);
static_assert(offsetof(bdNATTravPacket, m_srcAddr) == 0x14);
#endif
