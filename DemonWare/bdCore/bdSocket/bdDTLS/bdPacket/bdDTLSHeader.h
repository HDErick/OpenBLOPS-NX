#if defined(KISAK_SP) && !defined(KISAK_DW)
#pragma once
#include <DemonWare/bdCore/bdMemory/bdMemory.h>
#include <DemonWare/bdCore/bdUtilities/bdBytePacker.h>

class bdDTLSHeader
{
public:
    unsigned char m_type;
    unsigned char m_version;
    unsigned short m_verificationTag;
    unsigned short m_sequenceNumber;

    bdDTLSHeader() : m_type(0), m_version(0), m_verificationTag(0), m_sequenceNumber(0) {}
    bdDTLSHeader(unsigned char type, unsigned short verificationTag, unsigned short sequenceNumber)
        : m_type(type), m_version(2), m_verificationTag(verificationTag), m_sequenceNumber(sequenceNumber) {}
    virtual ~bdDTLSHeader() = default;
    static void operator delete(void *memory) { bdMemory::deallocate(memory); }

    virtual bool serialize(unsigned char *data, unsigned int dataSize, unsigned int offset,
        unsigned int *newOffset)
    {
        *newOffset = offset;
        unsigned char type = m_type;
        bool success = bdBytePacker::appendBuffer(data, dataSize, *newOffset, newOffset, &type, sizeof(type));
        unsigned char version = m_version;
        success = success && bdBytePacker::appendBuffer(data, dataSize, *newOffset, newOffset,
            &version, sizeof(version));
        unsigned short tag = m_verificationTag;
        success = success && bdBytePacker::appendBuffer(data, dataSize, *newOffset, newOffset,
            reinterpret_cast<unsigned char *>(&tag), sizeof(tag));
        unsigned short sequence = m_sequenceNumber;
        success = success && bdBytePacker::appendBuffer(data, dataSize, *newOffset, newOffset,
            reinterpret_cast<unsigned char *>(&sequence), sizeof(sequence));
        if (!success)
            *newOffset = offset;
        return success;
    }

    virtual bool deserialize(unsigned char *data, unsigned int dataSize, unsigned int offset,
        unsigned int *newOffset)
    {
        *newOffset = offset;
        unsigned char type;
        bool success = bdBytePacker::removeBuffer(data, dataSize, *newOffset, newOffset, &type, sizeof(type));
        if (success)
            m_type = type;
        unsigned char version;
        success = success && bdBytePacker::removeBuffer(data, dataSize, *newOffset, newOffset,
            &version, sizeof(version));
        if (success)
            m_version = version;
        unsigned short tag;
        success = success && bdBytePacker::removeBuffer(data, dataSize, *newOffset, newOffset,
            reinterpret_cast<unsigned char *>(&tag), sizeof(tag));
        if (success)
            m_verificationTag = tag;
        unsigned short sequence;
        success = success && bdBytePacker::removeBuffer(data, dataSize, *newOffset, newOffset,
            reinterpret_cast<unsigned char *>(&sequence), sizeof(sequence));
        if (success)
            m_sequenceNumber = sequence;
        if (!success)
            *newOffset = 0;
        return success;
    }

    unsigned short getVerificationTag() const { return m_verificationTag; }
};
static_assert(sizeof(void *) != 4 || sizeof(bdDTLSHeader) == 0xC);
#endif
