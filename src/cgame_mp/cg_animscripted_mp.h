#pragma once

struct DObj;
struct centity_s;

#ifdef KISAK_SP
#include <xanim/animcommand.h>
struct clientInfo_t;
struct msg_t;
struct clSnapshot_t;
struct snapshot_s;
struct client_t;
struct clientSnapshot_t;
struct XAnim_s;
namespace SpAnimSnapshot { struct Entity; }
void SV_WriteAnimSnapshot_SP(msg_t *msg, int serverTime, int firstEntity, int numEntities, const client_t *client, const clientSnapshot_t *selected);
void CL_ReadAnimSnapshot_SP(msg_t *msg, int serverTime, int messageNum, clSnapshot_t *snapshot);
void CG_ResetRemoteAnimSnapshots_SP();
void CG_CaptureRemoteAnimTrees_SP();
void CG_ClearRemoteAnimTrees_SP();
const SpAnimSnapshot::Entity *CG_GetRemoteAnimEntity_SP(int localClientNum, int entNum);
bool CG_HasRemoteAnimSnapshot_SP(int localClientNum);
XAnim_s *CG_GetRemoteAnimations_SP(int localClientNum, int entNum);
bool CG_ApplyRemoteAnimSnapshot_SP(int localClientNum, int entNum, DObj *obj, bool force);
void __cdecl CG_RegisterAnimCommandDvars_SP();
void CG_UpdateRootInfo_SP(int localClientNum, centity_s *cent);
void CG_ApplyRootInfo_SP(int localClientNum, centity_s *cent);
bool CG_ActorLinked_SP(int localClientNum, int entityNum);
void CG_ActorLinkTransform_SP(int localClientNum, int entityNum, float *origin, float *angles);
void CG_TransformEntityRoot_SP(int localClientNum, centity_s *cent, const float (*rootAxis)[3], float *origin, float *angles);
void __cdecl CG_InitializeSelectedAnimCommandPointers_SP(snapshot_s *snapshot);
int __cdecl CG_ProcessAnimCommands_SP(int localClientNum, snapshot_s *snapshot);
void __cdecl CG_ProcessSnapshotAnimCommands_SP(int localClientNum, snapshot_s *current, snapshot_s *next);
int __cdecl CG_ReplayAnimCommandsForEntity_SP(int localClientNum, snapshot_s *snapshot, int entityNum);
void __cdecl CG_InitializeEntityAnimCommandTime_SP(int localClientNum, int entityNum);
void CG_ApplySelectedAnimCommand_SP(int localClientNum, DObj *obj, struct XAnimTree_s *tree, const AnimCommand_SP *command, int serverTime);
void __cdecl CG_ApplyPendingAnimCommands_SP(int localClientNum);
void __cdecl CG_ApplyPendingAnimCommandsForDObj_SP(int localClientNum, int entNum, DObj *obj);
#endif

void __cdecl CG_GetTagMatrix(int localClientNum, int linkEntNum, unsigned __int16 tagName, float (*resultTagMat)[3]);
DObj *__cdecl GetLinkEntDObj(int localClientNum, centity_s *centLink);
void __cdecl CG_CalcTagParentAxis(int localClientNum, centity_s *cent, float (*parentAxis)[3]);
void __cdecl CG_LinkTransformForEntity(int localClientNum, centity_s *cent, float *resultOrigin, float *resultAngles);
void __cdecl CG_GenerateLinkInfo(int localClientNum, centity_s *cent, int attachedEntNum, int attachedTagIndex);
void __cdecl CG_UpdateEntityLink(int localClientNum, centity_s *cent);
void __cdecl CG_UpdateFakeEntityLink(int localClientNum, centity_s *cent, int parentNum, int tagIndex);
void __cdecl CG_UpdateFakeEntityLink(int localClientNum, centity_s *cent);
bool __cdecl CG_EntityLinked(int localClientNum, centity_s *cent);
centity_s *__cdecl CG_EntGetLinkToParent(int localClientNum, centity_s *cent);

#ifdef KISAK_SP
clientInfo_t *__cdecl CG_GetClientInfoForPlayerEnt_SP(int localClientNum, int entNum);
int CG_GetLinkToParent_SP(const centity_s *cent);
int CG_GetEntityLinkParent_SP(int localClientNum, const centity_s *cent);
#endif
