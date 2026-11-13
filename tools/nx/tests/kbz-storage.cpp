#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>
#include "../../../src/nx/nx_kbz_storage.h"

static std::map<void *, size_t> allocations;
static size_t liveBytes, peakBytes, allocationCalls, failAllocation, reads, failRead;
static void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
static void *allocate(size_t count, size_t size, bool zero) {
    if (++allocationCalls == failAllocation) return nullptr;
    void *p = zero ? calloc(count, size) : malloc(count * size);
    if (p) { allocations[p] = count * size; liveBytes += count * size; peakBytes = std::max(peakBytes, liveBytes); }
    return p;
}
static void *trackedMalloc(size_t size) { return allocate(1, size, false); }
static void *trackedCalloc(size_t count, size_t size) { return allocate(count, size, true); }
static void trackedFree(void *p) {
    if (!p) return;
    require(allocations.count(p), "double free or foreign allocation");
    liveBytes -= allocations.at(p); allocations.erase(p); free(p);
}
static size_t trackedRead(void *p, size_t size, size_t count, FILE *file) {
    if (++reads == failRead) return 0;
    return fread(p, size, count, file);
}
#define malloc trackedMalloc
#define calloc trackedCalloc
#define free trackedFree
#define fread trackedRead
#include "../../../src/nx/nx_kbz_storage.cpp"
#undef malloc
#undef calloc
#undef free
#undef fread

#define __int8 char
#define __int16 short
#define __debugbreak() throw std::runtime_error("engine assertion")
#include "zone-types.inc"
static XZone g_zones[32];
static int g_zoneCount;
static struct { char name[64]; } g_zoneNames[33];
static unsigned pmemFrees;
static void Com_Printf(int, const char *, ...) {}
static bool Assert_MyHandler(const char *, int, int, const char *, ...) { return false; }
static void PMem_Free(const char *) { ++pmemFrees; }
static void DB_FreeXBlocks(XBlock *blocks, int) {
    for (int i = 0; i < 7; ++i)
        require(!blocks[i].data && !blocks[i].size, "KBZ blocks passed to the wrong allocator");
}
#include "zone-unload.inc"

template<typename T> static void append(std::vector<uint8_t> &data, T value) {
    const auto *p = reinterpret_cast<const uint8_t *>(&value);
    data.insert(data.end(), p, p + sizeof(value));
}
static std::vector<uint8_t> fixture(uint32_t version) {
    std::vector<uint8_t> data{'K','B','Z','1'};
    for (auto value : {version, 2u, 600000u, 32u, 1u, 0u}) append(data, value);
    data.resize(data.size() + 600000, 0x5a);
    data[28 + 16] = 0; data[28 + 17] = 0;
    append<uint8_t>(data, 0); append<uint32_t>(data, 0);
    append<uint8_t>(data, 1); append<uint32_t>(data, 32); // One-past pointer is legal.
    if (version >= 2) {
        append<uint32_t>(data, 1); append<uint16_t>(data, 4);
        data.insert(data.end(), {'t','e','s','t'});
        append<uint32_t>(data, 1); append<uint8_t>(data, 0); append<uint32_t>(data, 16);
    }
    if (version >= 3) { append<uint32_t>(data, 1); append<uint8_t>(data, 0); append<uint32_t>(data, 24); }
    return data;
}
static size_t previousProgress;
static void progress(size_t completed, size_t total) {
    require(completed <= total && (!completed || completed >= previousProgress), "invalid read progress");
    previousProgress = completed;
}
static void write(const std::filesystem::path &path, const std::vector<uint8_t> &data) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char *>(data.data()), data.size());
}
int main(int argc, char **argv) {
    try {
        require(argc >= 2, "missing output directory");
        auto path = std::filesystem::path(argv[1]) / "storage.kbz";
        NxKbzStorage *storage = nullptr;
        for (uint32_t version = 1; version <= 3; ++version) {
            auto data = fixture(version); write(path, data);
            peakBytes = allocationCalls = reads = 0;
            require(NX_ReadKbz(path.string().c_str(), &storage, progress) == 1, "valid fixture rejected");
            const size_t successfulAllocations = allocationCalls, successfulReads = reads;
            require(previousProgress == data.size(), "progress did not reach EOF");
            require(storage->blocks[0][599999] == 0x5a, "resident block bytes changed");
            for (unsigned i = 0; i < 32; ++i) require(storage->blocks[1][i] == 0, "runtime block not zeroed");
            require(peakBytes == sizeof(NxKbzStorage) + 600000 + 32 + storage->metadataSize,
                    "loader allocated an extra copy of the zone");
            NX_FreeKbzStorage(storage); require(!liveBytes, "unload leaked memory");
            for (size_t i = 1; i <= successfulAllocations; ++i) {
                allocationCalls = 0; failAllocation = i;
                require(NX_ReadKbz(path.string().c_str(), &storage) == -1 && !storage && !liveBytes,
                        "allocation failure leaked or published a zone");
            }
            failAllocation = 0;
            for (size_t i = 1; i <= successfulReads; ++i) {
                reads = 0; failRead = i;
                require(NX_ReadKbz(path.string().c_str(), &storage) == -1 && !storage && !liveBytes,
                        "short read leaked or published a zone");
            }
            failRead = 0;
            for (size_t length : {size_t(0), size_t(11), size_t(27), size_t(300000), data.size()-1}) {
                write(path, {data.begin(), data.begin() + length});
                require(NX_ReadKbz(path.string().c_str(), &storage) == -1 && !liveBytes, "truncated zone accepted");
            }
            auto bad = data; bad[4] = 99; write(path, bad);
            require(NX_ReadKbz(path.string().c_str(), &storage) == -1 && !liveBytes, "unknown version accepted");
            bad = data; std::memset(bad.data() + 600028 + 1, 0xff, 4); write(path, bad);
            require(NX_ReadKbz(path.string().c_str(), &storage) == -1 && !liveBytes, "overflowing relocation accepted");
        }
        require(NX_ReadKbz((path.string()+".missing").c_str(), &storage) == 0 && !storage, "missing file not distinguished");
        write(path, fixture(3));
        for (unsigned i = 0; i < 3; ++i) {
            g_zones[i].index = i + 1;
            strcpy(g_zoneNames[i + 1].name, i == 0 ? "code" : "map");
            require(NX_ReadKbz(path.string().c_str(), &g_zones[i].kbz) == 1, "zone setup failed");
        }
        g_zoneCount = 3;
        const size_t oneZone = liveBytes / 3;
        auto *retained = g_zones[2].kbz;
        DB_UnloadXAssetsMemory(&g_zones[1]);
        require(g_zoneCount == 2 && g_zones[1].kbz == retained && liveBytes == 2 * oneZone,
                "zone compaction lost ownership or freed a surviving zone");
        require(!g_zoneNames[2].name[0] && g_zoneNames[3].name[0], "wrong zone name released");
        for (unsigned cycle = 0; cycle < 100; ++cycle) {
            g_zoneCount = 3;
            g_zones[2] = {}; g_zones[2].index = 2; g_zones[2].flags = 0x40000000;
            const auto previousFrees = pmemFrees;
            require(NX_ReadKbz(path.string().c_str(), &g_zones[2].kbz) == 1, "reload failed");
            DB_UnloadXAssetsMemory(&g_zones[2]);
            require(liveBytes == 2 * oneZone && !g_zones[2].kbz && pmemFrees == previousFrees,
                    "reload leaked or used the PMem allocator for a heap zone");
        }
        DB_UnloadXAssetsMemory(&g_zones[1]); DB_UnloadXAssetsMemory(&g_zones[0]);
        require(!g_zoneCount && !liveBytes, "zone shutdown leaked memory");
        for (int i = 2; i < argc; ++i) {
            require(NX_ReadKbz(argv[i], &storage) == 1, argv[i]);
            NX_FreeKbzStorage(storage); require(!liveBytes, "retail zone leaked");
        }
        puts("PASS: KBZ direct reads, allocation/I/O failures, bounds, progress, zone compaction and 100 unload/reload cycles");
    } catch (const std::exception &error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
