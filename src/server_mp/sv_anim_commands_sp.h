#pragma once

#ifdef KISAK_SP
struct HunkUser;

void __cdecl SV_AllocateCachedAnimCommandSnapshotMemory_SP(HunkUser *hunk, int maxClients);
void __cdecl SV_AllocateAnimCommandSnapshotMemory_SP(HunkUser *hunk, int maxClients);
void __cdecl SV_FreeCachedAnimCommandSnapshotMemory_SP(HunkUser *hunk);
void __cdecl SV_FreeAnimCommandSnapshotMemory_SP(HunkUser *hunk, int maxClients);
#endif
