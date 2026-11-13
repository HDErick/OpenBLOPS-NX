#pragma once
#include "actor.h"
#include <bgame/bg_weapons.h>

void __cdecl Actor_FillWeaponParms(actor_s *self, weaponParms *wp);
void __cdecl Actor_InitWeaponParms(actor_s *self, unsigned int weapon, weaponParms *wp);
#ifdef KISAK_SP
void __cdecl Actor_StopShoot_SP(actor_s *self);
void __cdecl Actor_Shoot_SP(actor_s *self, float accuracyMod, const float *target, int fireEvent);
#endif
gentity_s *__cdecl Actor_Melee(actor_s *self, const float *direction);
