#pragma once
#include "actor.h"

#ifdef KISAK_SP
bool Actor_Grenade_GetMagicVelocity_SP(actor_s *self, const float *origin,
    const float *target, float *velocity);
void Actor_Grenade_Predict_SP(gentity_s *grenade);
void Actor_Grenade_Pickup_SP(actor_s *self);
void Actor_Grenade_Cleanup_SP(actor_s *self);
void Actor_Grenade_Throw_SP(actor_s *self);
bool Actor_Grenade_CheckThrow_SP(actor_s *self, const float *origin,
    const float *armOffset, const float *target, unsigned method,
    float *start, float *velocity, float randomRange, bool allowHeld);
bool Actor_Grenade_CheckThrowEnemy_SP(actor_s *self, const float *origin,
    const float *armOffset, unsigned method, float *start, float *velocity,
    float randomRange, bool allowHeld);
void G_SpawnInfoGrenadeHint_SP(gentity_s *self, SpawnVar *spawnVar);
extern const dvar_t *ai_debugGrenadeHintArc;
void Actor_Grenade_Ping_SP(actor_s *self, gentity_s *grenade);
bool __fastcall Actor_Grenade_Start_SP(actor_s *self, ai_state_t previous);
void __fastcall Actor_Grenade_Finish_SP(actor_s *self, ai_state_t next);
bool __fastcall Actor_Grenade_Resume_SP(actor_s *self, ai_state_t previous);
actor_think_result_t __fastcall Actor_Grenade_Think_SP(actor_s *self);
void __fastcall Actor_Grenade_Pain_SP(actor_s *self, gentity_s *attacker, int damage,
    const float *point, int mod, const float *direction, hitLocation_t hit);
void __fastcall Actor_Grenade_React_SP(actor_s *self, gentity_s *other, const float *point);
#endif
