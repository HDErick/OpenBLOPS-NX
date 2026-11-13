#include "kbz-image-shim.h"
#include NX_KBZ_SOURCE
#include <fstream>
#include <iterator>

static void require(bool ok, const char *message) {
    if (!ok) throw std::runtime_error(message);
}
#ifdef NX_KBZ_HAS_STORAGE
static std::vector<XZone> zones;
#endif
static void load(const std::string &directory, const char *name) {
    const std::string path=directory+"/"+name+".kbz";
#ifdef NX_KBZ_HAS_STORAGE
    zones.emplace_back();
    const std::string ff=directory+"/"+name+".ff";
    require(NX_TryLoadKbz(name,ff.c_str(),&zones.back())==1,"fixture load failed");
    require(zones.back().kbz && !zones.back().kbz->metadata,"zone must retain blocks, not metadata");
#else
    std::ifstream file(path,std::ios::binary);
    require(bool(file),"missing fixture");
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),{});
    require(loadKbzImage(path.c_str(),data.data(),long(data.size()))==1,"fixture load failed");
#endif
}
static void checkImages() {
    for (const auto &image : pending)
        require(NX_D3D_IsTexture(image->texture.basemap),"delayed replacement was registered without a texture");
    publishOverrides();
    for (const auto &item : active)
        require(NX_D3D_IsTexture(item.second->texture.basemap),"active image is not a texture");
    for (const auto &image : inactive)
        require(NX_D3D_IsTexture(image->texture.basemap),"inactive override is not a texture");
}
int main(int argc, char **argv) {
    try {
        require(argc==2,"usage: kbz-image-overrides <fixtures>");
        const std::string dir=argv[1];
        load(dir,"patch"); checkImages();
        auto *identity=active.at("skin_pore_detail_test").get();
        Texture *original=identity->texture.basemap;
        load(dir,"frontend"); checkImages();
        require(identity==active.at("skin_pore_detail_test").get(),"override changed database identity");
        require(identity->texture.basemap!=original,"replacement was not uploaded");
        load(dir,"common_zombie"); checkImages();
        require(identity->texture.basemap->format==0x35545844,"retail image is not DXT5");
        const unsigned beforeStub=uploads;
        load(dir,"zombie_theater"); checkImages();
        require(uploads==beforeStub,"stub duplicated the existing texture");
        // A lower-priority zone must also own a built texture: it can be
        // promoted into the active slot when the higher-priority zone unloads.
        keepExisting=true;
        load(dir,"frontend"); checkImages(); keepExisting=false;
        auto *fallback=inactive.back().get();
        std::swap(*identity,*fallback); checkImages();
        queueUploads=true;
        const unsigned beforeAlias=uploads;
        load(dir,"aliases"); checkImages();
        auto *first=active.at("alias_first")->texture.basemap;
        auto *second=active.at("alias_second")->texture.basemap;
        require(first==second && first->refs==2,"shared load definition lost texture ownership");
        require(uploads==beforeAlias+1,"shared pixels uploaded more than once");
        require(uploadQueue.empty(),"texture creation not flushed before registration");
#ifdef NX_KBZ_HAS_IMAGE_AUDIT
        NX_KBZ_ValidateImages();
        GfxImageLoadDef bad{};
        identity->texture.loadDef=&bad;
        bool rejected=false;
        try { NX_KBZ_ValidateImages(); } catch (const std::runtime_error &) { rejected=true; }
        require(rejected,"post-override check accepted a raw load definition");
        identity->texture.basemap=original;
        dx.device=nullptr;
        rejected=false;
        try { load(dir,"patch"); } catch (const std::runtime_error &) { rejected=true; }
        require(rejected,"renderer without device published an image");
        renderer.current.enabled=false;
        load(dir,"disabled"); publishOverrides();
        require(active.at("disabled")->texture.basemap==nullptr,"disabled renderer retained raw pixels");
#endif
#ifdef NX_KBZ_HAS_STORAGE
        active.clear(); pending.clear(); inactive.clear(); liveTextures.clear();
        for (auto &zone : zones) {
            NX_FreeKbz(&zone);
            require(!zone.kbz,"zone retained ownership after unload");
            NX_FreeKbz(&zone);
        }
#endif
        puts("PASS: retail Zombies overrides, stable identities, inactive fallback, aliases, queued uploads and image validation");
        return 0;
    } catch (const std::exception &error) {
        fprintf(stderr,"FAIL: %s\n",error.what()); return 1;
    }
}
