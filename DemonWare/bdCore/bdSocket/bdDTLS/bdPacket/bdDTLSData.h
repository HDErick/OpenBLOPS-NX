#if defined(KISAK_SP) && !defined(KISAK_DW)
#pragma once
#include <DemonWare/bdCore/bdSocket/bdDTLS/bdPacket/bdDTLSHeader.h>
#include <DemonWare/bdCore/bdSocket/bdAddr.h>
#include <DemonWare/bdCore/bdCrypto/bdCypher.h>
#include <DemonWare/bdCore/bdCrypto/bdHashTiger192.h>
#include <DemonWare/bdPlatform/bdPlatformLog/bdPlatformLog.h>
#include <cstddef>
#include <cstdint>
#include <cstring>

class bdDTLSData : public bdDTLSHeader
{
public:
    unsigned char m_securityID[8];

    bdDTLSData() {}
    bdDTLSData(unsigned short verificationTag, unsigned short sequenceNumber)
        : bdDTLSHeader(6, verificationTag, sequenceNumber) {}
    ~bdDTLSData() override = default;

    bool serialize(unsigned char *data, unsigned int dataSize, unsigned int offset,
        unsigned int *newOffset) override
    {
        *newOffset = offset;
        bool success = bdDTLSHeader::serialize(data, dataSize, *newOffset, newOffset);
        success = success && bdBytePacker::appendBuffer(data, dataSize, *newOffset, newOffset,
            m_securityID, sizeof(m_securityID));
        if (!success)
            *newOffset = offset;
        return success;
    }

    bool deserialize(unsigned char *data, unsigned int dataSize, unsigned int offset,
        unsigned int *newOffset) override
    {
        *newOffset = offset;
        bool success = bdDTLSHeader::deserialize(data, dataSize, *newOffset, newOffset);
        success = success && bdBytePacker::removeBuffer(data, dataSize, *newOffset, newOffset,
            m_securityID, sizeof(m_securityID));
        if (!success)
            *newOffset = offset;
        return success;
    }

    bool serialize(unsigned char *data, unsigned int dataSize, unsigned int offset,
        unsigned int *newOffset, bdCypher *cypher, const bdSecurityID &securityID,
        const unsigned char *payload, unsigned int payloadSize, bdHashTiger192 *hash,
        const unsigned char *sharedKey)
    {
        unsigned int payloadOffset = 0;
        unsigned short length = 0;
        bool success = bdBytePacker::removeBuffer(const_cast<unsigned char *>(payload), payloadSize,
            0, &payloadOffset, reinterpret_cast<unsigned char *>(&length), sizeof(length));
        success = success && serialize(data, dataSize, offset, newOffset);
        success = success && bdBytePacker::appendBuffer(data, dataSize, *newOffset, newOffset,
            reinterpret_cast<unsigned char *>(&length), sizeof(length));
        const unsigned int paddedLength = (static_cast<unsigned int>(length) + 7) & ~7u;
        const unsigned int trailerLength = payloadSize - 2 - length;
        const unsigned int finalOffset = *newOffset + paddedLength + trailerLength;
        if (payloadSize < payloadOffset || payloadSize < payloadOffset + length ||
            payloadSize < static_cast<unsigned int>(length) + 2 ||
            payloadSize < static_cast<unsigned int>(length) + 2 + trailerLength)
        {
            logWarning("bdDTLSData::serialize", 71, "Packet format is invalid.");
            *newOffset = offset;
            return false;
        }
        if (!success || dataSize < (*newOffset - offset) + paddedLength + trailerLength)
        {
            logWarning("bdDTLSData::serialize", 79, "Insufficient space in the destination buffer.");
            *newOffset = offset;
            return false;
        }
        unsigned char *destination = data + *newOffset;
        std::memcpy(destination, payload + payloadOffset, length);
        std::memset(destination + length, 1, paddedLength - length);
        std::memcpy(destination + paddedLength, payload + payloadOffset + length, trailerLength);
        for (unsigned int i = 0; i < paddedLength; i += 8)
            for (unsigned int j = 0; j < 8; j += 4)
            {
                std::uint32_t word, mask;
                std::memcpy(&word, destination + i + j, sizeof(word));
                std::memcpy(&mask, sharedKey + j, sizeof(mask));
                word ^= mask;
                std::memcpy(destination + i + j, &word, sizeof(word));
            }
        std::memcpy(m_securityID, securityID.ab, sizeof(m_securityID));
        success = success && serialize(data, dataSize, offset, newOffset);
        *newOffset = success ? finalOffset : offset;
        return success;
    }

    bool deserialize(unsigned char *data, unsigned int dataSize, unsigned int offset,
        unsigned int *newOffset, bdCypher *cypher, const bdSecurityID &securityID,
        unsigned char *payload, unsigned int payloadCapacity, unsigned int *payloadSize,
        bdHashTiger192 *hash, const unsigned char *sharedKey)
    {
        unsigned short length = 0;
        bool success = deserialize(data, dataSize, offset, newOffset);
        success = success && bdBytePacker::removeBuffer(data, dataSize, *newOffset, newOffset,
            reinterpret_cast<unsigned char *>(&length), sizeof(length));
        if (!success)
        {
            logWarning("bdDTLSData::deserialize", 184, "Failed to deserialize header.");
            *newOffset = offset;
            *payloadSize = 0;
            return false;
        }
        const unsigned int paddedLength = (static_cast<unsigned int>(length) + 7) & ~7u;
        if (dataSize < *newOffset + paddedLength)
        {
            logWarning("bdDTLSData::deserialize", 195, "Truncated packet.");
            *newOffset = offset;
            *payloadSize = 0;
            return false;
        }
        const unsigned int trailerLength = dataSize - (*newOffset + paddedLength);
        *payloadSize = length + 2 + trailerLength;
        if (payloadCapacity < *payloadSize + 8)
        {
            logWarning("bdDTLSData::deserialize", 215, "Insufficient space in the destination buffer.");
            *newOffset = offset;
            *payloadSize = 0;
            return false;
        }
        if (std::memcmp(m_securityID, securityID.ab, sizeof(m_securityID)))
        {
            *newOffset = offset;
            *payloadSize = 0;
            return false;
        }
        const unsigned char *source = data + *newOffset;
        for (unsigned int i = 0; i < paddedLength; i += 8)
            for (unsigned int j = 0; j < 8; j += 4)
            {
                std::uint32_t word, mask;
                std::memcpy(&word, source + i + j, sizeof(word));
                std::memcpy(&mask, sharedKey + j, sizeof(mask));
                word ^= mask;
                std::memcpy(payload + 2 + i + j, &word, sizeof(word));
            }
        std::memcpy(payload + 2 + length, source + paddedLength, trailerLength);
        unsigned int payloadOffset = 0;
        success = success && bdBytePacker::appendBuffer(payload, payloadCapacity, 0, &payloadOffset,
            reinterpret_cast<unsigned char *>(&length), sizeof(length));
        if (!success)
        {
            logWarning("bdDTLSData::deserialize", 301, "Decryption failed.");
            *newOffset = offset;
            *payloadSize = 0;
        }
        return success;
    }

private:
    static void logWarning(const char *method, unsigned int line, const char *message)
    {
        bdLogMessage(BD_LOG_WARNING, "warn/", "bdSocket/dtls",
            "C:\\projects_pc\\cod\\codsrc\\DemonWare\\bdSocket\\bdDTLS\\bdPacket\\bdDTLSData.cpp",
            method, line, message);
    }
};
static_assert(sizeof(void *) != 4 || sizeof(bdDTLSData) == 0x14);
static_assert(sizeof(void *) != 4 || offsetof(bdDTLSData, m_securityID) == 0xC);
#endif
