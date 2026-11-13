#pragma once

#include "live_stats_layout.h"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace openblops
{
// Only portable blob bytes and bounded names go on disk, never native pointers
// or persistentStats metadata. Both current MP targets use the same DDL layout.
struct OfflineProfileSnapshot
{
    static constexpr size_t NameBytes = 64;
    static constexpr size_t PayloadBytes = 3 * STATS_BUFFER_SIZE + 10 * NameBytes;
    std::vector<unsigned char> bytes = std::vector<unsigned char>(PayloadBytes);
    unsigned char *Stats(size_t index) { return bytes.data() + index * STATS_BUFFER_SIZE; }
    const unsigned char *Stats(size_t index) const { return bytes.data() + index * STATS_BUFFER_SIZE; }
    char *Name(size_t index) { return reinterpret_cast<char *>(bytes.data() + 3 * STATS_BUFFER_SIZE + index * NameBytes); }
    const char *Name(size_t index) const { return reinterpret_cast<const char *>(bytes.data() + 3 * STATS_BUFFER_SIZE + index * NameBytes); }
};

class OfflineProfileStore
{
public:
    enum class LoadResult { Fresh, Loaded, Unreadable };
    using Validator = bool (*)(OfflineProfileSnapshot &);

    LoadResult Load(const char *first, const char *second, uint32_t version,
        uint32_t schemaBits, OfflineProfileSnapshot &snapshot, Validator validate)
    {
        paths[0] = first;
        paths[1] = second;
        schemaVersion = version;
        schemaSize = schemaBits;
        activeSlot = -1;
        generation = 0;
        blocked = false;
        lastSaved.clear();
        bool found = false;
        for (int slot = 0; slot < 2; ++slot)
        {
            OfflineProfileSnapshot candidate;
            uint64_t sequence = 0;
            bool exists = false;
            const bool valid = Read(paths[slot].c_str(), candidate, sequence, exists)
                && (!validate || validate(candidate));
            found = found || exists;
            if (valid && (activeSlot < 0 || sequence > generation))
            {
                snapshot = candidate;
                activeSlot = slot;
                generation = sequence;
            }
        }
        if (activeSlot >= 0)
        {
            lastSaved = snapshot.bytes;
            return LoadResult::Loaded;
        }
        // Preserve unsupported/corrupt files when neither generation is usable.
        blocked = found;
        return found ? LoadResult::Unreadable : LoadResult::Fresh;
    }

    bool Save(const OfflineProfileSnapshot &snapshot)
    {
        if (blocked || paths[0].empty() || generation == UINT64_MAX
            || snapshot.bytes.size() != OfflineProfileSnapshot::PayloadBytes)
            return false;
        if (snapshot.bytes == lastSaved)
            return true;
        const int target = activeSlot == 0 ? 1 : 0;
        std::vector<unsigned char> file(HeaderBytes + snapshot.bytes.size() + 4);
        std::memcpy(file.data(), "OBMPPRF1", 8);
        Put32(file.data() + 8, 1);
        Put32(file.data() + 12, schemaVersion);
        Put32(file.data() + 16, schemaSize);
        Put32(file.data() + 20, STATS_BUFFER_SIZE);
        Put64(file.data() + 24, generation + 1);
        std::memcpy(file.data() + HeaderBytes, snapshot.bytes.data(), snapshot.bytes.size());
        Put32(file.data() + file.size() - 4, Hash(file.data(), file.size() - 4));
        FILE *stream = std::fopen(paths[target].c_str(), "wb");
        if (!stream)
            return false;
        bool ok = std::fwrite(file.data(), 1, file.size(), stream) == file.size();
        ok = std::fflush(stream) == 0 && ok;
        ok = std::fclose(stream) == 0 && ok;
        if (!ok)
            return false;
        // The previous valid slot is untouched, even if this write fails partway.
        activeSlot = target;
        ++generation;
        lastSaved = snapshot.bytes;
        return true;
    }

private:
    static constexpr size_t HeaderBytes = 32;
    std::string paths[2];
    std::vector<unsigned char> lastSaved;
    uint32_t schemaVersion = 0, schemaSize = 0;
    uint64_t generation = 0;
    int activeSlot = -1;
    bool blocked = false;

    static uint32_t Get32(const unsigned char *p)
    {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    }
    static uint64_t Get64(const unsigned char *p) { return Get32(p) | (uint64_t(Get32(p + 4)) << 32); }
    static void Put32(unsigned char *p, uint32_t value)
    {
        for (int i = 0; i < 4; ++i) p[i] = static_cast<unsigned char>(value >> (8 * i));
    }
    static void Put64(unsigned char *p, uint64_t value) { Put32(p, uint32_t(value)); Put32(p + 4, uint32_t(value >> 32)); }
    static uint32_t Hash(const unsigned char *p, size_t length)
    {
        uint32_t value = 2166136261u;
        for (size_t i = 0; i < length; ++i) value = (value ^ p[i]) * 16777619u;
        return value;
    }
    bool Read(const char *path, OfflineProfileSnapshot &snapshot, uint64_t &sequence, bool &exists)
    {
        errno = 0;
        FILE *stream = std::fopen(path, "rb");
        exists = stream || errno != ENOENT;
        if (!stream)
            return false;
        std::vector<unsigned char> file(HeaderBytes + OfflineProfileSnapshot::PayloadBytes + 4);
        bool ok = std::fread(file.data(), 1, file.size(), stream) == file.size();
        ok = std::fgetc(stream) == EOF && !std::ferror(stream) && ok;
        ok = std::fclose(stream) == 0 && ok;
        if (!ok || std::memcmp(file.data(), "OBMPPRF1", 8)
            || Get32(file.data() + 8) != 1 || Get32(file.data() + 12) != schemaVersion
            || Get32(file.data() + 16) != schemaSize || Get32(file.data() + 20) != STATS_BUFFER_SIZE
            || Get32(file.data() + file.size() - 4) != Hash(file.data(), file.size() - 4))
            return false;
        sequence = Get64(file.data() + 24);
        if (!sequence)
            return false;
        std::memcpy(snapshot.bytes.data(), file.data() + HeaderBytes, snapshot.bytes.size());
        for (size_t index = 0; index < 10; ++index)
            if (!std::memchr(snapshot.Name(index), 0, OfflineProfileSnapshot::NameBytes))
                return false;
        return true;
    }
};
}
