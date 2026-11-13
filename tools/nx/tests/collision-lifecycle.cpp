#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#define __int8 char
#define __int16 short
#undef __declspec
#define __declspec(x)
#define __debugbreak() throw std::runtime_error("engine assertion")
struct cplane_s;
struct cNode_t;
struct cbrush_t;
#include "collision-types.inc"

static clipMap_t cm;
static clipMap_t *varclipMap_t = &cm;
static MapEnts **varMapEntsPtr, *varMapEnts;
static cStaticModel_s *varcStaticModel_t;
static DynEntityDef *varDynEntityDef;
static PhysConstraint *varPhysConstraint;
static unsigned dependencyVisits, bspStages;
static char bspName[128];
static struct { struct { bool enabled = true; } current; } fastFile;
static auto *useFastFile = &fastFile;

template<class... T> bool Assert_MyHandler(T...) { return false; }
void Com_Memset(void *data, int value, size_t size) { memset(data, value, size); }
void Sys_Error(char *) { throw std::runtime_error("collision still in use"); }
void Mark_MapEntsAsset(MapEnts *) { ++dependencyVisits; }
void Mark_cStaticModel_tArray(int) { ++dependencyVisits; }
void Mark_DynEntityDefArray(int) { ++dependencyVisits; }
void Mark_PhysConstraintArray(int) { ++dependencyVisits; }

static void require(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
static void requireReset() {
    require(!cm.mapEnts, "shutdown left a partially cleared map-entity pointer");
    const auto *bytes = reinterpret_cast<const unsigned char *>(&cm);
    for (size_t i = sizeof(cm.name); i < sizeof(cm); ++i)
        require(bytes[i] == 0, "collision reset left stale state");
}
unsigned char *CM_Hunk_Alloc(unsigned size, const char *, int) {
    require(size <= sizeof(bspName), "oversized fixture name");
    requireReset();
    return reinterpret_cast<unsigned char *>(bspName);
}
unsigned Com_GetBspVersion() { return 45; }
static void bspStage() { requireReset(); ++bspStages; }
void CMod_LoadMaterials() { bspStage(); }
void CMod_LoadPlanes() { bspStage(); }
void CMod_LoadBrushRelated(unsigned, bool) { bspStage(); }
void CMod_LoadNodes() { bspStage(); }
void CMod_LoadLeafSurfaces() { bspStage(); }
void CMod_LoadCollisionVerts() { bspStage(); }
void CMod_LoadCollisionTriangles() { bspStage(); }
void CMod_LoadPartitionIndices() { bspStage(); }
void CMod_LoadCollisionEdgeWalkable() { bspStage(); }
void CMod_LoadCollisionBorders() { bspStage(); }
void CMod_LoadCollisionPartitions() { bspStage(); }
void CMod_LoadVisibility() { bspStage(); }
void CMod_LoadEntityString() { bspStage(); }
enum { LUMP_VISIBILITY, ERR_DROP };
bool Com_BspHasLump(int) { return false; }
void Com_Error(int, const char *) { throw std::runtime_error("unexpected BSP visibility"); }
void ProfLoad_Begin(const char *) {}
void ProfLoad_End() {}
#include "collision-functions.inc"

int main() {
    try {
        static_assert(sizeof(void *) == 8);
        static_assert(sizeof(clipMap_t) == 568);
        static_assert(offsetof(clipMap_t, mapEnts) == 328);
        const char *names[] = {"maps/frontend.d3dbsp", "maps/zombie_theater.d3dbsp"};
        for (unsigned cycle = 0; cycle < 100; ++cycle) {
            const char *name = names[cycle % 2];
            memset(&cm, 0xa5, sizeof(cm));
            cm.name = name;
            cm.mapEnts = reinterpret_cast<MapEnts *>(uintptr_t(0x326123450));
            cm.isInUse = 1;
            CM_Shutdown();
            require(cm.name == name, "shutdown lost the registered asset name");
            requireReset();
            CM_Unload();
            Mark_clipMap_t();
            require(!dependencyVisits, "asset traversal visited unloaded dependencies");
            CM_Shutdown();
            requireReset();
            require(cm.name == name, "repeated shutdown lost the asset name");

            memset(&cm, 0xa5, sizeof(cm));
            CM_LoadMapFromBsp(name, cycle % 2);
            require(!strcmp(cm.name, name), "BSP load lost the new map name");
            requireReset();
        }
        require(bspStages == 1250, "BSP loading stages were skipped");
        puts("PASS: 100 map reset/reload cycles; full native state reset, name preservation and post-unload asset traversal");
    } catch (const std::exception &error) {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
