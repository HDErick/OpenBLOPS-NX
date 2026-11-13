#pragma once

#ifdef KISAK_SP
#include <xanim/animcommand.h>
#include <vector>
#include <cstdint>
struct gentity_s;
struct XAnimTree_s;
#include <universal/dvar.h>
extern const dvar_t *g_oldAnimCmdNetwork;

void __cdecl G_InitAnimCommands_SP();
AnimCommand_SP *__cdecl G_GetAnimCommandsBase_SP();
void __cdecl G_SetLastAnimCommandSnapshotTime_SP(int time);
void __cdecl G_ResetAnimCommandSequences_SP();
void __cdecl G_AddAnimCommandReference_SP(short commandIndex, short infoIndex, int expiry);
void __cdecl G_AddAnimCommandDependent_SP(short commandIndex, short infoIndex);
void __cdecl G_UpdateAnimCommandReferences_SP();

int __cdecl G_StoreAnimCommandDefault_SP(
    const gentity_s *ent, int type, unsigned int animIndex,
    unsigned int rootAnimIndex, float weight, float goalTime, float rate, int flags);
void __cdecl G_ReleaseAnimCommandsForEntDefault_SP(const gentity_s *ent);
void __cdecl G_PruneUnreferencedAnimCommandsDefault_SP();
int __cdecl G_StoreAnimCommand_SP(const gentity_s *ent, const XAnimTree_s *tree,
    int type, unsigned int animIndex, unsigned int rootAnimIndex, float weight,
    float goalTime, float rate, int flags);
void __cdecl G_ReleaseAnimCommandsForEnt_SP(const gentity_s *ent);
void __cdecl G_PruneUnreferencedAnimCommands_SP();
struct MemoryFile;
void G_CheckpointCaptureAnimCommands_SP(std::vector<std::uint8_t>& bytes);
void G_CheckpointApplyAnimCommands_SP(const std::vector<std::uint8_t>& bytes,const std::vector<unsigned short>& remap,bool apply);
void __cdecl G_WriteAnimCommandState_SP(MemoryFile *memFile);
void __cdecl G_ReadAnimCommandState_SP(MemoryFile *memFile);

#endif
