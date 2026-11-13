#pragma once

#include "nx_kbz_storage.h"

inline bool NX_ExpandKbzWeaponModels(NxKbzStorage &storage, XModel **&models, const uint16_t *hideTags)
{
    if (!models || static_cast<const void *>(models) != hideTags)
        return true;

    bool contained = false;
    const uintptr_t address = reinterpret_cast<uintptr_t>(hideTags);
    for (unsigned i = 0; i < storage.blockCount; ++i) {
        const uintptr_t base = reinterpret_cast<uintptr_t>(storage.blocks[i]);
        if (storage.blocks[i] && address >= base && address - base <= storage.sizes[i]
            && 32 * sizeof(*hideTags) <= storage.sizes[i] - (address - base))
            contained = true;
    }
    if (!contained)
        return false;
    for (unsigned i = 0; i < 32; ++i)
        if (hideTags[i])
            return false;

    // Legacy KBZs reuse 64 zero bytes of hide tags for 16 x86 model pointers.
    models = storage.emptyWeaponModels;
    return true;
}
