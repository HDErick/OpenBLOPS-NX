// Engine and driver test doubles for the KBZ loader.
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))
struct XModel;
struct WeaponDef { XModel **gunXModel, **worldModel; };
struct WeaponVariantDef { WeaponDef *weapDef; uint16_t *hideTags; };
struct Texture { unsigned refs = 1; int format; };
struct GfxImageLoadDef { uint8_t levelCount, flags; int format, resourceSize; uint8_t data[1]; };
union GfxTexture { Texture *basemap; GfxImageLoadDef *loadDef; };
struct GfxImage {
    GfxTexture texture;
    uint8_t mapType, semantic, category; bool delayLoadPixels;
    uint8_t picmip[2]; bool noPicmip; uint8_t track;
    int cardMemory[2]; uint16_t width, height, depth;
    uint8_t levelCount, streaming; uint32_t baseSize;
    uint8_t *pixels; uint32_t loadedSize; uint8_t skippedMipLevels;
    const char *name; uint32_t hash;
};
static_assert(sizeof(GfxImage) == 72 && offsetof(GfxImage, name) == 56);
static_assert(offsetof(GfxImageLoadDef, data) == 12);
struct Shader { struct { int loadDef; void *vs, *ps; } prog; };
struct Decl { bool isLoaded; };
struct MaterialPass { Shader *vertexShader, *pixelShader; Decl *vertexDecl; };
struct MaterialTechnique { unsigned passCount; MaterialPass *passArray; };
struct MaterialTechniqueSet { MaterialTechnique *techniques[130]; };
struct MaterialTextureDef { unsigned semantic; union { GfxImage *image; } u; };
struct Material { struct { const char *name; } info; MaterialTextureDef *textureTable; unsigned textureCount; };
struct GfxWorldVertex { float data[11]; };
struct GfxWorldDraw {
    struct { GfxWorldVertex *vertices; void *worldVb; } vd;
    struct { uint8_t *data; void *layerVb; } vld;
    unsigned vertexCount, vertexLayerDataSize;
};
struct GfxWorld { GfxWorldDraw draw; };
struct Menu { struct Item { Menu *parent; }; int itemCount; Item **items; };
enum XAssetType { ASSET_TYPE_MATERIAL=6, ASSET_TYPE_TECHNIQUE_SET=7, ASSET_TYPE_IMAGE=8,
    ASSET_TYPE_SOUND=9, ASSET_TYPE_SOUND_PATCH=10, ASSET_TYPE_CLIPMAP=11,
    ASSET_TYPE_CLIPMAP_PVS=12, ASSET_TYPE_GFXWORLD=17, ASSET_TYPE_MENU=22, ASSET_TYPE_WEAPON=24, ASSET_TYPE_FX=28,
    ASSET_TYPE_COUNT=43 };
struct NxKbzStorage;
struct XZone { NxKbzStorage *kbz = nullptr; };
inline int DB_GetXAssetTypeSize(XAssetType type) { return type == ASSET_TYPE_IMAGE ? sizeof(GfxImage) : 1; }
union XAssetHeader { void *data; GfxImage *image; MaterialTechniqueSet *techniqueSet;
    Material *material; GfxWorld *gfxWorld; Menu *menu; void *sound, *soundPatch; };
struct XAsset { XAssetType type; XAssetHeader header; };
struct { void *device = reinterpret_cast<void *>(1); } dx;
struct Dvar { struct { bool enabled = true; } current; } renderer;
const Dvar *r_loadForRenderer = &renderer;
constexpr int ERR_DROP=1, SCRIPTINSTANCE_SERVER=0;
volatile int g_totalSize=0; volatile unsigned g_loadedSize=0; int g_trackLoadProgress=0;
inline void Com_Printf(int, const char *, ...) {}
inline void Com_PrintWarning(int, const char *, ...) {}
inline void Com_PrintError(int, const char *, ...) {}
[[noreturn]] inline void Com_Error(int, const char *fmt, ...) {
    char message[512]; va_list ap; va_start(ap,fmt); vsnprintf(message,sizeof(message),fmt,ap); va_end(ap);
    throw std::runtime_error(message);
}
inline void SND_AddBank(void *) {}
inline void SND_AddPatch(void *) {}
inline void Load_CreateMaterialVertexShader(int *, Shader *) {}
inline void Load_CreateMaterialPixelShader(int *, Shader *) {}
inline void Load_BuildVertexDecl(Decl **) {}
inline void Load_VertexBuffer(void **, uint8_t *, int) {}
inline int SL_GetString(const char *, unsigned, int) { return 0; }
inline XAssetHeader DB_FindXAssetHeader(XAssetType, char *, int, int) { return {}; }
inline const char *DB_GetXAssetNameNoAssert(const XAsset *asset) { return asset->header.image->name; }
extern "C" inline void nx_normalize_path(const char *in, char *out, size_t size) { snprintf(out,size,"%s",in); }
extern "C" inline const char *nx_get_install_dir() { return "."; }

inline std::unordered_map<const void *, std::unique_ptr<Texture>> liveTextures;
inline std::unordered_map<std::string, std::unique_ptr<GfxImage>> active;
inline std::vector<std::unique_ptr<GfxImage>> pending, inactive;
inline std::vector<std::pair<GfxImage *, int>> uploadQueue;
inline bool queueUploads=false, keepExisting=false;
inline unsigned uploads=0;
extern "C" inline bool NX_D3D_IsTexture(const void *p) { return liveTextures.count(p) != 0; }
inline void upload(GfxImage *image, int format) {
    auto texture=std::make_unique<Texture>(); texture->format=format;
    image->texture.basemap=texture.get(); liveTextures.emplace(texture.get(),std::move(texture)); ++uploads;
}
inline void Load_Texture(GfxTexture *definition, GfxImage *image) {
    const int format=definition->loadDef->format;
    image->texture.basemap=nullptr;
    if (!renderer.current.enabled) return;
    if (queueUploads) uploadQueue.emplace_back(image,format);
    else upload(image,format);
}
inline void RB_Resource_Flush() {
    for (auto [image,format] : uploadQueue) upload(image,format);
    uploadQueue.clear();
}
inline bool R_DuplicateTexture(GfxImage *dst, const GfxImage *src) {
    if (!NX_D3D_IsTexture(src->texture.basemap)) throw std::runtime_error("alias is not a texture");
    dst->texture=src->texture; ++dst->texture.basemap->refs; return true;
}
inline XAssetHeader DB_AddXAsset(XAssetType type, XAssetHeader input) {
    if (type != ASSET_TYPE_IMAGE) throw std::runtime_error("image-only fixture required");
    const std::string name=input.image->name;
    if (name[0] == ',') return XAssetHeader{active.at(name.substr(1)).get()};
    // DB_CloneXAssetInternal copies the header. DB_AddXAsset returns the OLD
    // active entry even while retaining the new copy for DB_PostLoadXZone.
    auto copy=std::make_unique<GfxImage>(*input.image);
    auto it=active.find(name);
    if (it == active.end()) {
        auto *p=copy.get(); active.emplace(name,std::move(copy)); return XAssetHeader{p};
    }
    if (keepExisting) inactive.push_back(std::move(copy));
    else pending.push_back(std::move(copy));
    return XAssetHeader{it->second.get()};
}
inline void DB_EnumXAssets(XAssetType, void (*visit)(XAssetHeader,void *), void *data, bool overrides) {
    for (const auto &item : active) visit(XAssetHeader{item.second.get()},data);
    if (overrides) for (const auto &item : inactive) visit(XAssetHeader{item.get()},data);
}
inline void publishOverrides() {
    for (auto &image : pending) {
        std::swap(*active.at(image->name),*image);
        inactive.push_back(std::move(image));
    }
    pending.clear();
}
