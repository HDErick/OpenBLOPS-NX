#include "sv_anim_commands_sp.h"

#ifdef KISAK_SP
#include "sv_main_mp.h"
#include <universal/mem_userhunk.h>
#include <cstring>

void __cdecl SV_AllocateCachedAnimCommandSnapshotMemory_SP(HunkUser *hunk, int maxClients)
{
    const int size = 1024 * maxClients * sizeof(AnimCommand_SP);
    svs.cachedSnapshotAnimCommands = static_cast<AnimCommand_SP *>(
        Hunk_UserAlloc(hunk, size, 128, "svs.cachedSnapshotAnimCmds"));
    memset(svs.cachedSnapshotAnimCommands, 0, size);
}

void __cdecl SV_AllocateAnimCommandSnapshotMemory_SP(HunkUser *hunk, int maxClients)
{
    svs.numSnapshotAnimCommands = 2048;
    for (int clientNum = 0; clientNum < maxClients; ++clientNum)
    {
        const int size = svs.numSnapshotAnimCommands * sizeof(AnimCommand_SP);
        svs.snapshotAnimCommands[clientNum] = static_cast<AnimCommand_SP *>(
            Hunk_UserAlloc(hunk, size, 4, "svs.snapshotAnimCmds"));
        memset(svs.snapshotAnimCommands[clientNum], 0, svs.numSnapshotAnimCommands * sizeof(AnimCommand_SP));
    }
}

void __cdecl SV_FreeCachedAnimCommandSnapshotMemory_SP(HunkUser *hunk)
{
    if (svs.cachedSnapshotAnimCommands)
        Hunk_UserFree(hunk, svs.cachedSnapshotAnimCommands);
    svs.cachedSnapshotAnimCommands = nullptr;
}

void __cdecl SV_FreeAnimCommandSnapshotMemory_SP(HunkUser *hunk, int maxClients)
{
    for (int clientNum = 0; clientNum < maxClients; ++clientNum)
    {
        if (svs.snapshotAnimCommands[clientNum])
            Hunk_UserFree(hunk, svs.snapshotAnimCommands[clientNum]);
        svs.snapshotAnimCommands[clientNum] = nullptr;
    }
}
#endif
