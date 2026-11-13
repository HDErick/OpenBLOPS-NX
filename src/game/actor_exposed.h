#pragma once
#include "actor.h"

void __fastcall Actor_Exposed_FindReacquireNode(actor_s *self);
pathnode_t *__fastcall Actor_Exposed_GetReacquireNode(actor_s *self);
char __fastcall Actor_Exposed_UseReacquireNode(actor_s *self, pathnode_t *pNode);
char __fastcall Actor_Exposed_ReacquireStepMove(actor_s *self, float fDist);
void __fastcall Actor_Exposed_FindReacquireDirectPath(actor_s *self, bool ignoreSuppression);
void __fastcall Actor_Exposed_FindReacquireProximatePath(actor_s *self, bool ignoreSuppression);
char __fastcall Actor_Exposed_StartReacquireMove(actor_s *self);
void __fastcall Actor_Exposed_FlashBanged(actor_s *self);
int __cdecl Path_IsValidClaimNode(const pathnode_t *node);
void __fastcall Actor_Exposed_Touch(actor_s *self, gentity_s *pOther);
#ifdef KISAK_SP
bool __fastcall Actor_Exposed_Start_SP(actor_s *self, ai_state_t ePrevState);
void __fastcall Actor_Exposed_Finish_SP(actor_s *self, ai_state_t eNextState);
bool __fastcall Actor_Exposed_Resume_SP(actor_s *self, ai_state_t ePrevState);
actor_think_result_t __fastcall Actor_Exposed_Think_SP(actor_s *self);
actor_think_result_t __fastcall Actor_Human_Exposed_Think_SP(actor_s *self);
void G_RegisterHumanCoverDvars_SP();
void Actor_Human_FindPathToGoal_SP(actor_s *self);
bool Actor_EnemyPosRecentlyKnown_SP(actor_s *self, int hadPath);
void Actor_Human_NodeAngles_SP(const pathnode_t *node, float *sevenAngles);
bool Actor_Human_NodeFacingPoint_SP(const float *point, const pathnode_t *node, const float *sevenAngles);
bool Actor_Human_NodeAnim_SP(actor_s *self, const pathnode_t *node, scr_animscript_t **anim);
bool Actor_Human_NodeAnimUsable_SP(actor_s *self, const pathnode_t *node, bool regardlessVisible, scr_animscript_t **anim);
bool Actor_Human_AtClaimedNode_SP(actor_s *self);
bool Actor_Human_CoverUsable_SP(actor_s *self, pathnode_t *node);
pathnode_t *Actor_Human_FindBestCoverNode_SP(actor_s *self);
bool Actor_Human_UseCoverNode_SP(actor_s *self, pathnode_t *node);
int Actor_CheckCoverExitPosWithPath_SP(actor_s *self, const float *exitPos);
void Actor_Human_ExposedCombatAnim_SP(actor_s *self);
float Actor_Human_GrenadeCoverScore_SP(actor_s *self, const pathnode_t *node);
extern const dvar_t *ai_showSuppression;
#endif
