#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace customclasses
{
constexpr unsigned ChunkBytes = 960, Window = 4;
constexpr uint32_t Rejected = 0xffffffffu;
struct Header { uint32_t qport, challenge, schema, size, checksum, index; };
inline uint32_t Hash(const unsigned char *data, size_t size)
{
    uint32_t hash = 2166136261u;
    while (size--) hash = (hash ^ *data++) * 16777619u;
    return hash;
}
inline bool Same(const Header &a, const Header &b)
{
    return a.qport == b.qport && a.challenge == b.challenge && a.schema == b.schema
        && a.size == b.size && a.checksum == b.checksum;
}
inline unsigned Count(uint32_t size) { return (size + ChunkBytes - 1) / ChunkBytes; }
inline std::vector<unsigned char> Encode(bool ack, const Header &h, const unsigned char *data = nullptr, size_t size = 0)
{
    const char *tag = ack ? "obclassAck\n" : "obclasses\n";
    const size_t prefix = strlen(tag);
    std::vector<unsigned char> result(prefix + 24 + size);
    memcpy(result.data(), tag, prefix);
    const uint32_t words[] = { h.qport, h.challenge, h.schema, h.size, h.checksum, h.index };
    for (unsigned i = 0; i < 6; ++i)
        for (unsigned b = 0; b < 4; ++b) result[prefix + i * 4 + b] = (unsigned char)(words[i] >> (8 * b));
    if (size) memcpy(result.data() + prefix + 24, data, size);
    return result;
}
inline bool Decode(bool ack, const unsigned char *packet, size_t size, Header &h, const unsigned char *&data, size_t &bytes)
{
    const char *tag = ack ? "obclassAck\n" : "obclasses\n";
    const size_t prefix = strlen(tag);
    if (size < prefix + 24 || memcmp(packet, tag, prefix)) return false;
    uint32_t words[6] = {};
    for (unsigned i = 0; i < 6; ++i)
        for (unsigned b = 0; b < 4; ++b) words[i] |= uint32_t(packet[prefix + i * 4 + b]) << (8 * b);
    h = { words[0], words[1], words[2], words[3], words[4], words[5] };
    data = packet + prefix + 24; bytes = size - prefix - 24;
    return !ack || !bytes;
}
// Pack exact DDL bits, zeroing padding so adjacent profile fields never travel.
inline std::vector<unsigned char> Pack(const unsigned char *source, unsigned startBit, unsigned bits)
{
    std::vector<unsigned char> result((bits + 7) / 8);
    for (unsigned i = 0; i < bits; ++i)
        result[i / 8] |= ((source[(startBit + i) / 8] >> ((startBit + i) % 8)) & 1) << (i % 8);
    return result;
}
inline void Unpack(unsigned char *destination, unsigned startBit, unsigned bits, const unsigned char *source)
{
    for (unsigned i = 0; i < bits; ++i)
    {
        unsigned char &byte = destination[(startBit + i) / 8];
        const unsigned char mask = 1u << ((startBit + i) % 8);
        byte = (byte & ~mask) | (((source[i / 8] >> (i % 8)) & 1) ? mask : 0);
    }
}
struct Sender
{
    Header header{};
    std::vector<unsigned char> data;
    unsigned next = 0, sent = 0;
    int64_t lastSend = -1000;
    void Begin(std::vector<unsigned char> snapshot, uint32_t qport, uint32_t challenge, uint32_t schema)
    {
        data = std::move(snapshot);
        header = { qport, challenge, schema, uint32_t(data.size()), Hash(data.data(), data.size()), 0 };
        next = sent = 0; lastSend = -1000;
    }
    bool Done() const { return !data.empty() && next == Count(header.size); }
    template<class Send> void Pump(int64_t now, Send send)
    {
        if (data.empty() || Done() || now - lastSend < 250) return;
        lastSend = now;
        const unsigned end = std::min(next + Window, Count(header.size));
        for (unsigned i = next; i < end; ++i)
        {
            Header h = header; h.index = i;
            const unsigned offset = i * ChunkBytes;
            auto packet = Encode(false, h, data.data() + offset, std::min(ChunkBytes, h.size - offset));
            send(packet); sent = std::max(sent, i + 1);
        }
    }
    bool Acknowledge(const Header &h)
    {
        if (!Same(header, h) || h.index < next || h.index > sent) return false;
        if (h.index > next) { next = h.index; lastSend = -1000; }
        return true;
    }
};
struct Receiver
{
    Header header{};
    std::vector<unsigned char> data, received;
    unsigned next = 0;
    bool complete = false, failed = false;
    bool Receive(const Header &h, const unsigned char *payload, size_t bytes, unsigned expectedSize)
    {
        if (!h.size || h.size != expectedSize || h.index >= Count(h.size)) return false;
        const unsigned offset = h.index * ChunkBytes;
        if (bytes != std::min(ChunkBytes, h.size - offset)) return false;
        if (data.empty()) { header = h; data.resize(h.size); received.resize(Count(h.size)); }
        if (!Same(header, h)) return false;
        if (complete || failed) return true;
        if (received[h.index])
        {
            if (memcmp(data.data() + offset, payload, bytes)) failed = true;
            return true;
        }
        memcpy(data.data() + offset, payload, bytes); received[h.index] = 1;
        while (next < received.size() && received[next]) ++next;
        if (next == received.size())
        {
            complete = Hash(data.data(), data.size()) == header.checksum;
            failed = !complete;
        }
        return true;
    }
    Header Ack() const { Header h = header; h.index = failed ? Rejected : next; return h; }
};
}
