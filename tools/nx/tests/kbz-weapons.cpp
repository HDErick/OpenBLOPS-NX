#include "../../../src/nx/nx_kbz_weapons.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <unordered_set>

struct XModel { const char *name; };
static XModel **varXModelPtr, *varXModel;
static std::unordered_set<XModel *> registeredModels;
static void Mark_XModelAsset(XModel *model) {
    if (!registeredModels.count(model)) throw std::runtime_error("invalid model reference during unload");
    assert(model->name);
}
static void Mark_XModel() {}
#include "mark-models.inc"

template<class T> static T read(const void *p) { T value; memcpy(&value, p, sizeof(value)); return value; }

int main(int argc, char **argv) {
    try {
        alignas(8) unsigned char bytes[192]{};
        NxKbzStorage fixture{};
        fixture.blockCount = 1; fixture.blocks[0] = bytes; fixture.sizes[0] = sizeof(bytes);
        auto *tags = reinterpret_cast<uint16_t *>(bytes + 2);
        auto **models = reinterpret_cast<XModel **>(tags);
        XModel sentinel{"following allocation"};
        auto *sentinelPointer = &sentinel;
        memcpy(bytes + 72, &sentinelPointer, sizeof(sentinelPointer));
        varXModelPtr = models;
        bool reproduced = false;
        try { Mark_XModelPtrArray(16); } catch (const std::runtime_error &) { reproduced = true; }
        assert(reproduced);
        assert(NX_ExpandKbzWeaponModels(fixture, models, tags));
        assert(models == fixture.emptyWeaponModels);
        varXModelPtr = models; Mark_XModelPtrArray(16);
        assert(read<XModel *>(bytes + 72) == &sentinel);
        assert(NX_ExpandKbzWeaponModels(fixture, models, tags));
        models = reinterpret_cast<XModel **>(tags); tags[0] = 1;
        assert(!NX_ExpandKbzWeaponModels(fixture, models, tags));
        tags[0] = 0; fixture.sizes[0] = 65;
        assert(!NX_ExpandKbzWeaponModels(fixture, models, tags));
        XModel *valid[16]{}; valid[15] = &sentinel; models = valid;
        assert(NX_ExpandKbzWeaponModels(fixture, models, tags) && models == valid);

        unsigned expanded = 0, visited = 0;
        for (int arg = 1; arg < argc; ++arg) {
            NxKbzStorage *storage = nullptr;
            assert(NX_ReadKbz(argv[arg], &storage) == 1);
            const unsigned char *assets = storage->metadata + storage->relocCount * 10;
            registeredModels.clear();
            for (unsigned i = 0; i < storage->relocCount; ++i) {
                auto *r = storage->metadata + i * 10;
                void *target = storage->blocks[r[5]] + read<uint32_t>(r + 6);
                memcpy(storage->blocks[r[0]] + read<uint32_t>(r + 1), &target, sizeof(target));
            }
            for (unsigned i = 0; i < storage->assetCount; ++i) {
                const auto *a = assets + i * 9;
                if (read<uint32_t>(a) == 5)
                    registeredModels.insert(reinterpret_cast<XModel *>(storage->blocks[a[4]] + read<uint32_t>(a + 5)));
            }
            for (unsigned i = 0; i < storage->assetCount; ++i) {
                const auto *a = assets + i * 9;
                if (read<uint32_t>(a) != 24) continue;
                const auto *weapon = storage->blocks[a[4]] + read<uint32_t>(a + 5);
                auto *definition = read<unsigned char *>(weapon + 16);
                if (!definition) continue;
                const auto *hideTags = read<uint16_t *>(weapon + 48);
                for (unsigned field : {8u, 1120u}) {
                    auto **array = read<XModel **>(definition + field);
                    if (!array) continue;
                    auto **original = array;
                    assert(NX_ExpandKbzWeaponModels(*storage, array, hideTags));
                    expanded += array != original;
                    memcpy(definition + field, &array, sizeof(array));
                    varXModelPtr = array; Mark_XModelPtrArray(16);
                    ++visited;
                }
            }
            NX_FreeKbzStorage(storage);
        }
        printf("PASS: reproduced unload overread; bounds/nonzero rejection; %u model arrays marked, %u legacy aliases expanded across %d zones\n", visited, expanded, argc - 1);
    } catch (const std::exception &error) {
        fprintf(stderr, "FAIL: %s\n", error.what()); return 1;
    }
}
