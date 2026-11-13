#pragma once

struct centity_s;
struct actorInfo_t;
struct cg_s;
struct cgs_t;

void __cdecl CG_ActorProcessSnapshot(int localClientNum, centity_s *cent);
void __cdecl CG_UpdateActorDObj(int localClientNum, centity_s *cent, actorInfo_t *ai);
cgs_t *__cdecl CG_GetLocalClientStaticGlobals(int localClientNum);
void __cdecl CG_ResetActorEntity(int localClientNum, cg_s *cgameGlob, centity_s *cent);
void __cdecl CG_Actor_PreControllers(int localClientNum, centity_s *cent);
void __cdecl CG_Actor(int localClientNum, centity_s *cent);
bool __cdecl CG_EntityNeedsScriptThread(int localClientNum, centity_s *cent);
void __cdecl CG_ActorCorpse(int localClientNum, centity_s *cent);

#ifdef KISAK_SP
struct dvar_s;
extern const dvar_s *ai_useServerAnims;
extern const dvar_s *ai_useServerAnimsEntity;
void CG_RegisterActorAnimationSyncDvars_SP();
void __cdecl CG_UpdateActorDObj_SP(int localClientNum, centity_s *cent);
int __cdecl CG_GetActorScriptedAnim_SP(int localClientNum, int entityNum);
#endif
