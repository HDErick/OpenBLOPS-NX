#pragma once
#ifdef KISAK_SP
#include "actor.h"
bool Actor_IsUsingTurret_SP(const actor_s *self);
bool Actor_CanUseTurret_SP(actor_s *self, gentity_s *turret);
bool Actor_UseTurret_SP(actor_s *self, gentity_s *turret);
void Actor_UpdateEnemyGoal_SP(actor_s *self);
bool Actor_GrenadeThreatSafe_SP(actor_s *self, const float *point);
bool __fastcall Actor_Turret_Start_SP(actor_s *self, ai_state_t previous);
void __fastcall Actor_Turret_Finish_SP(actor_s *self, ai_state_t next);
void __fastcall Actor_Turret_Suspend_SP(actor_s *self, ai_state_t next);
void __fastcall Actor_Turret_Pain_SP(actor_s *self, gentity_s *attacker, int damage, const float *point, int mod, const float *direction, hitLocation_t location);
actor_think_result_t __fastcall Actor_Turret_Think_SP(actor_s *self);
actor_think_result_t Actor_TurretPositionAndBlend_SP(actor_s *self);
#endif
