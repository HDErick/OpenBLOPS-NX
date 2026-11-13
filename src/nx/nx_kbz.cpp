// Load prelinked LP64 zones and construct their runtime assets.
#ifdef KISAK_NX
#include "nx_kbz.h"
#include "nx_kbz_storage.h"
#include "nx_kbz_weapons.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <utility>
#include <vector>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include <database/database.h>
#include <bgame/bg_weapons.h>
#include <database/db_assetnames.h>
#include <database/db_registry.h>
#include <qcommon/common.h>
#include <universal/q_shared.h>
#include <universal/com_memory.h>
#include <gfx_d3d/r_material.h>
#include <gfx_d3d/r_image.h>
#include <gfx_d3d/r_init.h>
#include <gfx_d3d/r_dvars.h>
#include <gfx_d3d/rb_resource.h>
#include <gfx_d3d/r_image_load_obj.h>
#include <database/db_file_load.h>
#include <sound/snd_bank.h>
#include <ui/ui_shared.h>
#include <gfx_d3d/r_bsp.h>
#include <gfx_d3d/r_buffers.h>
extern volatile int g_totalSize;
extern volatile unsigned int g_loadedSize;
extern int g_trackLoadProgress;
#include <clientscript/cscr_stringlist.h>

extern "C" void nx_normalize_path(const char *in, char *out, size_t outSize);
extern "C" const char *nx_get_install_dir(void);
extern "C" bool NX_D3D_IsTexture(const void *p);
extern "C" void NX_D3D_HoldDeviceLock(bool hold);
#include "nx_load_boost.h"

static void kbzProgress(size_t completed, size_t total)
{
    if (!g_trackLoadProgress || total < 0x100000) return;
    constexpr size_t unit = 0x40000;
    if (!completed) {
        g_totalSize = int((total + unit - 1) / unit);
        __atomic_store_n(&g_loadedSize, 0u, __ATOMIC_SEQ_CST);
    } else {
        __atomic_store_n(&g_loadedSize, unsigned(completed == total
            ? (total + unit - 1) / unit : completed / unit), __ATOMIC_SEQ_CST);
    }
}

void NX_FreeKbz(XZone *zone)
{
    NX_FreeKbzStorage(zone->kbz);
    zone->kbz = nullptr;
}
static XAssetHeader registerAsset(XAssetType type, XAssetHeader h)
{
    if (type == ASSET_TYPE_IMAGE)
        RB_Resource_Flush();
    if (type == ASSET_TYPE_CLIPMAP)
        type = ASSET_TYPE_CLIPMAP_PVS;

    XAssetHeader given = h;
    XAssetHeader added = DB_AddXAsset(type, h);

    switch (type) {
    case ASSET_TYPE_SOUND:
        SND_AddBank(added.sound);
        break;
    case ASSET_TYPE_SOUND_PATCH:
        SND_AddPatch(added.soundPatch);
        break;
    case ASSET_TYPE_MENU:
        if (given.menu)
            for (int i = 0; i < given.menu->itemCount; ++i)
                if (given.menu->items[i])
                    given.menu->items[i]->parent = added.menu;
        break;
    default:
        break;
    }
    return added;
}
struct NxPendingFile {
    NxImageFileData file;
    int  externalDataSize;
    bool read;
};
struct NxBuildCtx {
    uint8_t *const *block;
    const uint32_t *blockSize;
    uint32_t        nblk;
    std::vector<std::pair<const void *, GfxImage *>> builtLoadDefs;
    std::vector<GfxImage *> pendingImages;
    std::vector<NxPendingFile> pendingFiles;
    std::vector<std::pair<GfxImage *, GfxImage *>> pendingDuplicates;
    size_t   pendingFileBytes = 0;
    unsigned imagesBuilt = 0;

    bool inBlocks(const void *ptr) const
    {
        const uint8_t *b = (const uint8_t *)ptr;
        if (!b) return false;
        for (uint32_t i = 0; i < nblk; ++i)
            if (block[i] && b >= block[i] && b < block[i] + blockSize[i])
                return true;
        return false;
    }
};
typedef unsigned (*NxAssetBuilder)(XAssetHeader, NxBuildCtx &);

struct NxBuildStep {
    XAssetType     type;
    NxAssetBuilder build;
    const char    *what;   // plural noun, for the summary line
};
struct NxRegistered {
    XAssetType   type;
    XAssetHeader header;
};
static unsigned buildTechniqueSet(XAssetHeader h, NxBuildCtx &)
{
    MaterialTechniqueSet *techSet = h.techniqueSet;
    if (!techSet) return 0;

    unsigned built = 0;
    for (unsigned t = 0; t < ARRAY_COUNT(techSet->techniques); ++t) {
        MaterialTechnique *tech = techSet->techniques[t];
        if (!tech) continue;
        for (unsigned pass_i = 0; pass_i < tech->passCount; ++pass_i) {
            MaterialPass *pass = &tech->passArray[pass_i];

            if (pass->vertexShader && !pass->vertexShader->prog.vs) {
                Load_CreateMaterialVertexShader(&pass->vertexShader->prog.loadDef,
                                                pass->vertexShader);
                ++built;
            }
            if (pass->pixelShader && !pass->pixelShader->prog.ps) {
                Load_CreateMaterialPixelShader(&pass->pixelShader->prog.loadDef,
                                               pass->pixelShader);
                ++built;
            }
            if (pass->vertexDecl && !pass->vertexDecl->isLoaded) {
                Load_BuildVertexDecl(&pass->vertexDecl);
                ++built;
            }
        }
    }
    return built;
}
static const size_t   kFileBatchBytes  = 32u << 20;
static const unsigned kFileBatchImages = 32;

static const unsigned kReaderThreads = 2;

static const size_t   kBackgroundBatchBytes  = 4u << 20;
static const unsigned kBackgroundBatchImages = 4;
static const unsigned kBackgroundReaderThreads = 1;

static bool loadingBehindMenu()
{
#ifdef KISAK_SP
    return false;
#else
    return NX_LoadBoostBackgroundLoading();
#endif
}

namespace {
class NxImageReadPool {
public:
    void read(NxPendingFile *files, size_t count, unsigned helpers)
    {
        std::lock_guard<std::mutex> readLock(m_readMutex);
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_stopping) {
                lock.unlock();
                for (size_t i = 0; i < count; ++i)
                    files[i].read = Image_NX_ReadFileAnyThread(files[i].file.image, &files[i].file);
                return;
            }
            if (m_threads.empty())
                for (unsigned i = 0; i < kReaderThreads; ++i)
                    m_threads.emplace_back(&NxImageReadPool::worker, this, i);
            m_files = files;
            m_count = count;
            m_helpers = helpers;
            m_next.store(0);
            m_busy = unsigned(m_threads.size());
            m_inflight = true;
            ++m_generation;
        }
        m_wake.notify_all();
        take();
        std::unique_lock<std::mutex> lock(m_mutex);
        m_done.wait(lock, [this] { return m_busy == 0; });
        m_files = nullptr;
        m_count = 0;
        m_inflight = false;
        lock.unlock();
        m_done.notify_all();
    }

    void stop()
    {
        std::lock_guard<std::mutex> stopLock(m_stopMutex);
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_stopping = true;
            m_done.wait(lock, [this] { return !m_inflight; });
            m_quit = true;
        }
        m_wake.notify_all();
        for (std::thread &thread : m_threads)
            thread.join();
        m_threads.clear();
    }

private:
    void take()
    {
        for (size_t i; (i = m_next.fetch_add(1)) < m_count; )
            m_files[i].read = Image_NX_ReadFileAnyThread(m_files[i].file.image, &m_files[i].file);
    }
    void worker(unsigned index)
    {
        uint64_t seen = 0;
        for (;;) {
            bool active;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_wake.wait(lock, [&] { return m_generation != seen || m_quit; });
                if (m_quit)
                    return;
                seen = m_generation;
                active = index < m_helpers;
            }
            if (active)
                take();
            std::lock_guard<std::mutex> lock(m_mutex);
            if (--m_busy == 0)
                m_done.notify_all();
        }
    }

    std::mutex m_readMutex, m_stopMutex, m_mutex;
    std::condition_variable m_wake, m_done;
    std::vector<std::thread> m_threads;
    NxPendingFile *m_files = nullptr;
    size_t m_count = 0;
    unsigned m_helpers = 0;
    bool m_stopping = false, m_quit = false, m_inflight = false;
    std::atomic<size_t> m_next{0};
    unsigned m_busy = 0;
    uint64_t m_generation = 0;
};
NxImageReadPool &imageReadPool()
{
    static NxImageReadPool &pool = *new NxImageReadPool;
    return pool;
}
}

void NX_KBZ_StopReaders()
{
    imageReadPool().stop();
}

static thread_local NxBuildCtx *s_imageBatch = nullptr;

void NX_KBZ_AbortImageBatch()
{
    NxBuildCtx *ctx = s_imageBatch;
    s_imageBatch = nullptr;
    if (!ctx)
        return;
    for (NxPendingFile &pending : ctx->pendingFiles)
        Image_NX_FreeFileData(&pending.file);
    ctx->pendingFiles.clear();
    ctx->pendingImages.clear();
    ctx->pendingDuplicates.clear();
    ctx->builtLoadDefs.clear();
    ctx->pendingFileBytes = 0;
}

static void flushImages(NxBuildCtx &ctx)
{
    if (!ctx.pendingFiles.empty())
        imageReadPool().read(ctx.pendingFiles.data(), ctx.pendingFiles.size(),
                             loadingBehindMenu() ? kBackgroundReaderThreads : kReaderThreads);
    s_imageBatch = &ctx;
    if (!ctx.pendingFiles.empty()) {
        for (size_t i = 0; i < ctx.pendingFiles.size(); ++i) {
            NxPendingFile &pending = ctx.pendingFiles[i];
            if (!pending.read && !Image_NX_ReadFile(pending.file.image, &pending.file)) {
                for (size_t j = 0; j < ctx.pendingFiles.size(); ++j)
                    Image_NX_FreeFileData(&ctx.pendingFiles[j].file);
                GfxImage *image = pending.file.image;
                ctx.pendingFiles.clear();
                ctx.pendingImages.clear();
                ctx.pendingFileBytes = 0;
                Com_Error(ERR_DROP, "Couldn't load image '%s'\n", image->name);
                return;
            }
            DB_LoadedExternalData(pending.externalDataSize);
        }
        Image_NX_LockImageLoad(true);
        NX_D3D_HoldDeviceLock(true);
        for (size_t i = 0; i < ctx.pendingFiles.size(); ++i)
            Image_NX_CreateFromFileData(&ctx.pendingFiles[i].file);
        NX_D3D_HoldDeviceLock(false);
        Image_NX_LockImageLoad(false);
        ctx.pendingFiles.clear();
        ctx.pendingFileBytes = 0;
    }
    for (size_t i = 0; i < ctx.pendingImages.size(); ++i) {
        GfxImage *image = ctx.pendingImages[i];
        if (!image->texture.basemap)
            continue;
        if (!NX_D3D_IsTexture(image->texture.basemap)) {
            Com_Error(ERR_DROP, "NX_KBZ: image '%s' upload returned a non-texture %p\n",
                      image->name, (void *)image->texture.basemap);
            return;
        }
        ++ctx.imagesBuilt;
    }
    ctx.pendingImages.clear();
    s_imageBatch = nullptr;
}
static void finishImages(NxBuildCtx &ctx)
{
    flushImages(ctx);
    for (size_t i = 0; i < ctx.pendingDuplicates.size(); ++i)
        if (R_DuplicateTexture(ctx.pendingDuplicates[i].first, ctx.pendingDuplicates[i].second))
            ++ctx.imagesBuilt;
    ctx.pendingDuplicates.clear();
}
static bool queueFileImage(GfxImage *image, NxBuildCtx &ctx)
{
    if (image->texture.loadDef->resourceSize || image->category != 3
        || !r_loadForRenderer || !r_loadForRenderer->current.enabled)
        return false;

    image->texture.basemap = 0;
    image->delayLoadPixels = 0;
    const int externalDataSize = image->cardMemory.platform[0];
    image->cardMemory.platform[0] = 0;
    image->cardMemory.platform[1] = 0;
    NxPendingFile pending = {};
    pending.file.image = image;
    pending.externalDataSize = externalDataSize;
    ctx.pendingFiles.push_back(pending);
    ctx.pendingImages.push_back(image);
    ctx.pendingFileBytes += externalDataSize > 0 ? size_t(externalDataSize) : 0;
    const bool behindMenu = loadingBehindMenu();
    if (ctx.pendingFileBytes >= (behindMenu ? kBackgroundBatchBytes : kFileBatchBytes)
        || ctx.pendingFiles.size() >= (behindMenu ? kBackgroundBatchImages : kFileBatchImages))
        flushImages(ctx);
    return true;
}
static void queueImage(XAssetHeader h, NxBuildCtx &ctx)
{
    GfxImage *image = h.image;
    if (!image)
        return;

    const void *loadDef = image->texture.loadDef;
    if (!ctx.inBlocks(loadDef))
        return;
    if (r_loadForRenderer && r_loadForRenderer->current.enabled && !dx.device) {
        Com_Error(ERR_DROP, "NX_KBZ: cannot upload image '%s': no D3D device\n", image->name);
        return;
    }
    for (size_t i = 0; i < ctx.builtLoadDefs.size(); ++i)
        if (ctx.builtLoadDefs[i].first == loadDef) {
            ctx.pendingDuplicates.push_back(std::make_pair(image, ctx.builtLoadDefs[i].second));
            return;
        }
    ctx.builtLoadDefs.push_back(std::make_pair(loadDef, image));
    if (queueFileImage(image, ctx))
        return;
    const bool holdDevice = image->texture.loadDef->resourceSize != 0 || image->category == 5;
    if (holdDevice)
        NX_D3D_HoldDeviceLock(true);
    Load_Texture(&image->texture, image);
    if (holdDevice)
        NX_D3D_HoldDeviceLock(false);
    ctx.pendingImages.push_back(image);
}
static unsigned buildGfxWorld(XAssetHeader h, NxBuildCtx &)
{
    GfxWorld *world = h.gfxWorld;
    if (!world)
        return 0;
    GfxWorldDraw *draw = &world->draw;
    unsigned built = 0;
    if (draw->vd.vertices && draw->vertexCount && !draw->vd.worldVb) {
        Load_VertexBuffer(&draw->vd.worldVb, (unsigned char *)draw->vd.vertices,
                          (int)(sizeof(GfxWorldVertex) * draw->vertexCount));
        built += draw->vd.worldVb != 0;
    }
    if (draw->vld.data && draw->vertexLayerDataSize && !draw->vld.layerVb) {
        Load_VertexBuffer(&draw->vld.layerVb, draw->vld.data, (int)draw->vertexLayerDataSize);
        built += draw->vld.layerVb != 0;
    }
    return built;
}

static const NxBuildStep kBuildSteps[] = {
    { ASSET_TYPE_TECHNIQUE_SET, buildTechniqueSet, "shader/vertex-decl objects" },
    { ASSET_TYPE_GFXWORLD,      buildGfxWorld,     "world vertex buffers" },
};
static void buildRuntimeObjects(const char *path,
                                const std::vector<NxRegistered> &registered,
                                NxBuildCtx &ctx)
{
    for (unsigned step = 0; step < ARRAY_COUNT(kBuildSteps); ++step) {
        const NxBuildStep *bs = &kBuildSteps[step];
        unsigned built = 0, visited = 0;

        for (size_t i = 0; i < registered.size(); ++i) {
            if (registered[i].type != bs->type) continue;
            built += bs->build(registered[i].header, ctx);
            ++visited;
        }

        if (visited)
            Com_Printf(16, "NX_KBZ: '%s' built %u %s over %u assets of type %d\n",
                       path, built, bs->what, visited, (int)bs->type);
    }
}
static void auditMaterialImages(const char *path, const std::vector<NxRegistered> &registered,
                                NxBuildCtx &ctx)
{
    unsigned checked = 0, bad = 0;
    for (size_t i = 0; i < registered.size(); ++i) {
        if (registered[i].type != ASSET_TYPE_MATERIAL)
            continue;
        Material *m = registered[i].header.material;
        if (!m || !m->textureTable)
            continue;
        for (unsigned t = 0; t < m->textureCount; ++t) {
            const MaterialTextureDef *td = &m->textureTable[t];
            if (td->semantic == 11)   // water: u.water, not an image
                continue;
            GfxImage *img = td->u.image;
            if (!img)
                continue;
            ++checked;
            const void *tex = img->texture.basemap;
            if (!tex || NX_D3D_IsTexture(tex))
                continue;
            if (++bad <= 12)
                printf("[nx-kbz] audit '%s': material '%s' texture %u image '%s' holds %p, not a "
                       "texture | image is %s, what it holds is %s\n",
                       path, m->info.name ? m->info.name : "?", t,
                       img->name ? img->name : "?", tex,
                       ctx.inBlocks(img) ? "a BLOCK copy" : "a pool entry",
                       ctx.inBlocks(tex) ? "in this zone's blocks" : "outside this zone");
        }
    }
    printf("[nx-kbz] audit '%s': %u material textures checked, %u not textures\n",
           path, checked, bad);
    fflush(stdout);
}
struct NxImageAudit { unsigned checked = 0, bad = 0; };

static void auditDatabaseImage(XAssetHeader h, void *data)
{
    NxImageAudit &audit = *static_cast<NxImageAudit *>(data);
    const GfxImage *image = h.image;
    if (!image) return;
    ++audit.checked;
    if (image->texture.basemap && !NX_D3D_IsTexture(image->texture.basemap)) {
        if (++audit.bad <= 16)
            Com_PrintError(10, "[nx-kbz] invalid image after overrides: '%s' texture=%p\n",
                           image->name, (void *)image->texture.basemap);
    }
}

extern "C" void NX_KBZ_ValidateImages()
{
    NxImageAudit audit;
    DB_EnumXAssets(ASSET_TYPE_IMAGE, auditDatabaseImage, &audit, true);
    Com_Printf(16, "[nx-kbz] image check after overrides: %u entries, %u invalid\n",
               audit.checked, audit.bad);
    if (audit.bad)
        Com_Error(ERR_DROP, "NX_KBZ: %u unbuilt images after zone overrides; see kisakblack.log\n", audit.bad);
}
class AssetSlots
{
public:
    void collect(const uint8_t *assetTable, const uint8_t *end, uint32_t assetCount,
                 const uint8_t *relocTable, uint32_t relocCount,
                 uint8_t *block[], const uint32_t blockSize[], uint32_t nblk)
    {
        std::vector<const void *> headers;
        const uint8_t *p = assetTable;
        for (uint32_t i = 0; i < assetCount && p + 9 <= end; ++i) {
            p += 4;                             // type: every type moves, so it does not matter
            uint8_t  ab = *p++; uint32_t ao; memcpy(&ao, p, 4); p += 4;
            if (ab < nblk && block[ab] && ao < blockSize[ab])
                headers.push_back(block[ab] + ao);
        }
        if (headers.empty())
            return;
        std::sort(headers.begin(), headers.end());

        p = relocTable;
        for (uint32_t i = 0; i < relocCount && p + 10 <= end; ++i) {
            uint8_t  sb = *p++; uint32_t so; memcpy(&so, p, 4); p += 4;
            uint8_t  tb = *p++; uint32_t to; memcpy(&to, p, 4); p += 4;
            if (sb >= nblk || tb >= nblk || !block[sb] || !block[tb]) continue;
            if (so + sizeof(void *) > blockSize[sb] || to > blockSize[tb]) continue;
            const void *tgt = block[tb] + to;
            if (std::binary_search(headers.begin(), headers.end(), tgt))
                m_slots.emplace_back(tgt, block[sb] + so);
        }
        std::sort(m_slots.begin(), m_slots.end());
    }
    uint32_t redirect(const void *given, void *added)
    {
        if (given == added)
            return 0;
        auto it = std::lower_bound(m_slots.begin(), m_slots.end(),
                                   std::make_pair(given, (uint8_t *)nullptr));
        uint32_t n = 0;
        for (; it != m_slots.end() && it->first == given; ++it, ++n)
            memcpy(it->second, &added, sizeof(void *));
        m_moved += n;
        if (n)
            ++m_assetsMoved;
        else
            ++m_assetsUnreferenced;   // a top-level asset nothing in the zone points at
        return n;
    }

    uint32_t slotCount() const { return (uint32_t)m_slots.size(); }
    uint32_t moved() const { return m_moved; }
    uint32_t assetsMoved() const { return m_assetsMoved; }
    uint32_t assetsUnreferenced() const { return m_assetsUnreferenced; }

private:
    uint32_t m_moved = 0;
    uint32_t m_assetsMoved = 0;
    uint32_t m_assetsUnreferenced = 0;
    std::vector<std::pair<const void *, uint8_t *>> m_slots; // asset header -> slot
};
static const uint8_t *remapScriptStrings(const char *path, const uint8_t *p, const uint8_t *end,
                                         uint8_t **block, const uint32_t *blockSize, uint32_t nblk)
{
    uint32_t count;
    if (p + 4 > end) return nullptr;
    memcpy(&count, p, 4); p += 4;
    std::vector<uint16_t> ids(count, 0);
    char buf[1024];
    for (uint32_t i = 0; i < count; ++i) {
        uint16_t len;
        if (p + 2 > end) return nullptr;
        memcpy(&len, p, 2); p += 2;
        if (p + len > end) return nullptr;
        if (len) {
            uint32_t n = len < sizeof(buf) - 1 ? len : (uint32_t)sizeof(buf) - 1;
            memcpy(buf, p, n);
            buf[n] = 0;
            ids[i] = (uint16_t)SL_GetString(buf, 4u, SCRIPTINSTANCE_SERVER);
        }
        p += len;
    }
    uint32_t slots, bad = 0;
    if (p + 4 > end) return nullptr;
    memcpy(&slots, p, 4); p += 4;
    for (uint32_t i = 0; i < slots && p + 5 <= end; ++i) {
        uint8_t b = *p++; uint32_t off; memcpy(&off, p, 4); p += 4;
        if (b >= nblk || !block[b] || off + 2 > blockSize[b]) { ++bad; continue; }
        uint16_t v; memcpy(&v, block[b] + off, 2);
        if (v >= count) { ++bad; v = 0; }
        else v = ids[v];
        memcpy(block[b] + off, &v, 2);
    }
    Com_Printf(16, "NX_KBZ: '%s' %u script strings, %u slots remapped (%u out of range)\n",
               path, count, slots, bad);
    return p;
}
static void resolveFxRefs(const char *path, const uint8_t *p, const uint8_t *end,
                          uint8_t **block, const uint32_t *blockSize, uint32_t nblk)
{
    uint32_t count, bad = 0, resolved = 0;
    if (!p || p + 4 > end) return;
    memcpy(&count, p, 4); p += 4;
    for (uint32_t i = 0; i < count && p + 5 <= end; ++i) {
        uint8_t b = *p++; uint32_t off; memcpy(&off, p, 4); p += 4;
        if (b >= nblk || !block[b] || off + sizeof(void *) > blockSize[b]) { ++bad; continue; }
        const char *name;
        memcpy(&name, block[b] + off, sizeof(name));
        if (!name)
            continue;
        XAssetHeader h = DB_FindXAssetHeader(ASSET_TYPE_FX, (char *)name, 1, -1);
        memcpy(block[b] + off, &h.data, sizeof(void *));
        ++resolved;
    }
    Com_Printf(16, "NX_KBZ: '%s' %u effect refs resolved (%u slots, %u bad)\n", path, resolved, count, bad);
}

static int loadKbzImage(const char *path, NxKbzStorage &storage)
{
    uint8_t **block = storage.blocks;
    const uint32_t *blockSize = storage.sizes;
    const uint32_t nblk = storage.blockCount;
    const uint32_t relocCount = storage.relocCount;
    const uint32_t assetCount = storage.assetCount;
    const uint8_t *p = storage.metadata;
    const uint8_t *end = p + storage.metadataSize;

#ifdef KISAK_NX
    for (uint32_t i = 0; i < nblk; ++i)
        if (block[i])
            printf("[nx-kbz] '%s' block %u = [%p, %p) %u bytes\n",
                   path, i, (void *)block[i], (void *)(block[i] + blockSize[i]), blockSize[i]);
    fflush(stdout);
#endif
    const uint8_t *relocTable = p;
    for (uint32_t i = 0; i < relocCount; ++i) {
        if (p + 10 > end) break;
        uint8_t  sb = *p++; uint32_t so; memcpy(&so, p, 4); p += 4;
        uint8_t  tb = *p++; uint32_t to; memcpy(&to, p, 4); p += 4;
        if (sb >= nblk || tb >= nblk || !block[sb] || !block[tb]) continue;
        if (so + sizeof(void *) > blockSize[sb] || to > blockSize[tb]) continue;
        void *tgt = block[tb] + to;
        memcpy(block[sb] + so, &tgt, sizeof(void *));
    }
    Com_Printf(16, "NX_KBZ: relocs applied, registering %u assets\n", assetCount);
    const uint8_t *assetTable = p;
    for (uint32_t i = 0; i < assetCount; ++i) {
        const uint8_t *entry = assetTable + size_t(i) * 9;
        uint32_t type, offset;
        memcpy(&type, entry, 4);
        memcpy(&offset, entry + 5, 4);
        if (type != ASSET_TYPE_WEAPON) continue;
        auto *weapon = reinterpret_cast<WeaponVariantDef *>(block[entry[4]] + offset);
        if (weapon->weapDef
            && (!NX_ExpandKbzWeaponModels(storage, weapon->weapDef->gunXModel, weapon->hideTags)
                || !NX_ExpandKbzWeaponModels(storage, weapon->weapDef->worldModel, weapon->hideTags)))
            return -1;
    }
    const uint8_t *fxRefSection = nullptr;
    if (storage.version >= 2 && assetTable + (size_t)assetCount * 9 <= end)
        fxRefSection = remapScriptStrings(path, assetTable + (size_t)assetCount * 9, end, block, blockSize, nblk);
    if (storage.version < 3)
        fxRefSection = nullptr;
    AssetSlots slots;
    slots.collect(assetTable, end, assetCount, relocTable, relocCount, block, blockSize, nblk);
    NxBuildCtx ctx;
    ctx.block = block;
    ctx.blockSize = blockSize;
    ctx.nblk = nblk;
    unsigned imagesVisited = 0;
    for (uint32_t i = 0; i < assetCount; ++i) {
        const uint8_t *e = p + (size_t)i * 9;
        if (e + 9 > end) break;
        uint32_t type; memcpy(&type, e, 4);
        uint8_t  ab = e[4]; uint32_t ao; memcpy(&ao, e + 5, 4);
        if (type != ASSET_TYPE_IMAGE || ab >= nblk || !block[ab] || ao >= blockSize[ab]) continue;
        XAssetHeader h;
        h.data = block[ab] + ao;
        ++imagesVisited;
        queueImage(h, ctx);
    }
    finishImages(ctx);
    const unsigned imagesBuilt = ctx.imagesBuilt;
    std::vector<NxRegistered> registered;
    registered.reserve(assetCount);
    for (uint32_t i = 0; i < assetCount; ++i) {
        if (p + 9 > end) break;
        uint32_t type; memcpy(&type, p, 4); p += 4;
        uint8_t  ab = *p++; uint32_t ao; memcpy(&ao, p, 4); p += 4;
        if (ab >= nblk || !block[ab] || ao >= blockSize[ab]) continue;
        XAssetHeader h;
        h.data = block[ab] + ao;
        XAssetHeader added = registerAsset((XAssetType)type, h);
        slots.redirect(h.data, added.data);
        NxRegistered rec = { (XAssetType)type, added };
        registered.push_back(rec);
    }
    if (imagesVisited)
        Com_Printf(16, "NX_KBZ: '%s' built %u textures over %u images BEFORE registration (including overrides)\n",
                   path, imagesBuilt, imagesVisited);
    if (slots.slotCount())
        Com_Printf(16, "NX_KBZ: '%s' redirected %u of %u reference slots to pool entries"
                       " (%u assets moved, %u referenced by nothing)\n",
                   path, slots.moved(), slots.slotCount(),
                   slots.assetsMoved(), slots.assetsUnreferenced());
    resolveFxRefs(path, fxRefSection, end, block, blockSize, nblk);
    buildRuntimeObjects(path, registered, ctx);
    auditMaterialImages(path, registered, ctx);

    Com_Printf(16, "NX_TryLoadKbz: loaded '%s' (%u assets, %u relocs)\n",
               path, assetCount, relocCount);
    return 1;
}
int NX_TryLoadKbz(const char *zoneName, const char *ffFilename, XZone *zone)
{
    char cand[2][512];
    int nCand = 0;
    snprintf(cand[nCand++], 512, "%s/kbz/%s.kbz", nx_get_install_dir(), zoneName);
    {
        char *dst = cand[nCand];
        nx_normalize_path(ffFilename, dst, 512);
        size_t len = strlen(dst);
        if (len >= 3 && !strcmp(dst + len - 3, ".ff")) { strcpy(dst + len - 3, ".kbz"); nCand++; }
        else if (len + 5 < 512)                        { strcat(dst, ".kbz"); nCand++; }
    }

    for (int i = 0; i < nCand; ++i) {
        NxKbzStorage *storage = nullptr;
        const int result = NX_ReadKbz(cand[i], &storage, kbzProgress);
        if (!result) continue;
        if (result < 0) {
            Com_PrintError(10, "NX_TryLoadKbz: cannot read '%s'\n", cand[i]);
            return -1;
        }
        const uint8_t *asset = storage->metadata + size_t(storage->relocCount) * 10;
        for (uint32_t j = 0; j < storage->assetCount; ++j, asset += 9) {
            uint32_t type, offset;
            memcpy(&type, asset, 4);
            memcpy(&offset, asset + 5, 4);
            if (type >= ASSET_TYPE_COUNT || uint32_t(DB_GetXAssetTypeSize(XAssetType(type)))
                > storage->sizes[asset[4]] - offset) {
                NX_FreeKbzStorage(storage);
                Com_PrintError(10, "NX_TryLoadKbz: invalid asset in '%s'\n", cand[i]);
                return -1;
            }
        }
        zone->kbz = storage;
        const int loaded = loadKbzImage(cand[i], *storage);
        free(storage->metadata);
        storage->metadata = nullptr;
        storage->metadataSize = 0;
        return loaded;
    }
    return 0;
}

#endif // KISAK_NX
