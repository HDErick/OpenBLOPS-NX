#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

#define __int8 char
#define __int32 int
#define __int64 long long
#define __debugbreak() throw std::runtime_error("engine assertion")
#define HIDWORD(x) (static_cast<uint64_t>(x) >> 32)
#include "../../../src/universal/physicalmemory.h"

PhysicalMemory g_mem;
static int freeTot, g_overAllocatedSize;
void PMem_Init() {}
template<class... T> bool Assert_MyHandler(T...) { return false; }
int I_stricmp(const char *a, const char *b) { return strcmp(a, b); }
void track_flush_physical_alloc(const char *, EMemTrack) {}
void track_physical_alloc(int, const char *, EMemTrack, int) {}
void track_PrintInfo() {}
void tlPrintf(const char *, ...) {}
void Com_Printf(int, const char *, ...) {}
void Com_PrintError(int, const char *, ...) {}
enum { ERR_FATAL };
void Com_Error(int, const char *) { throw std::runtime_error("non-LIFO free"); }
void Sys_OutOfMemErrorInternal(const char *, int) { throw std::runtime_error("arena exhausted"); }
#include "physical-functions.inc"

static void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }

int main() {
    try {
        std::vector<unsigned char> buffer(8192);
        PMem_InitPhysicalMemory(&g_mem, "test", buffer.data(), unsigned(buffer.size()));
        static char names[16][16];
        for (unsigned i = 0; i < 16; ++i) snprintf(names[i], sizeof(names[i]), "zone%u", i);
        for (unsigned cycle = 0; cycle < 100; ++cycle) {
            for (unsigned side = 0; side < 2; ++side) {
                auto &prim = g_mem.prim[side];
                unsigned saved[16];
                for (unsigned i = 0; i < 16; ++i) {
                    saved[i] = prim.pos;
                    PMem_BeginAllocInPrim(&prim, names[i], TRACK_FASTFILE);
                    auto *memory = _PMem_AllocNamed(97 + i, 64, 0, side, names[i], TRACK_FASTFILE);
                    memset(memory, int(i), 97 + i);
                    PMem_EndAllocInPrim(&prim, names[i]);
                }
                for (unsigned i = 16; i-- > 0;) {
                    PMem_FreeInPrim(&prim, names[i], 0);
                    require(prim.pos == saved[i], "unwind restored an invalid arena position");
                    require(prim.allocListCount == i, "unwind discarded a live allocation");
                    if (i) require(prim.allocList[i - 1].name == names[i - 1], "previous allocation was modified");
                }
            }
            require(g_mem.prim[0].pos == 0 && g_mem.prim[1].pos == buffer.size(), "arena did not recover its capacity");
        }
        for (unsigned side = 0; side < 2; ++side) {
            auto &prim = g_mem.prim[side];
            const unsigned start = prim.pos;
            for (unsigned i = 0; i < 3; ++i) {
                PMem_BeginAllocInPrim(&prim, names[i], TRACK_FASTFILE);
                _PMem_AllocNamed(64, 64, 0, side, names[i], TRACK_FASTFILE);
                PMem_EndAllocInPrim(&prim, names[i]);
            }
            prim.allocList[1].name = nullptr;
            PMem_FreeInPrim(&prim, names[2], 0);
            require(prim.allocListCount == 1, "unwind failed to skip an already released entry");
            PMem_FreeInPrim(&prim, names[0], 0);
            require(prim.pos == start, "unwind failed to restore arena after a released entry");
        }
        puts("PASS: 100 low/high arena reuse cycles, live predecessor preservation and released-entry unwinding");
    } catch (const std::exception &e) {
        fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
