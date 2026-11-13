#include "actor.h"

#include "actor_dog_exposed.h"
#include "actor_generic.h"
#include "actor_exposed.h"
#ifdef KISAK_SP
#include "actor_turret.h"
#endif
#ifdef KISAK_SP
#include "actor_grenade.h"
#endif
#include "actor_death.h"
#include "actor_pain.h"
#include "actor_negotiation.h"
#ifdef KISAK_SP
#include "actor_animapi.h"
#include "actor_events.h"
#include <array>
#include "actor_orientation.h"
#include "actor_state.h"
#include "sentient.h"
#include <game_mp/actor_mp.h>
#include <game_mp/g_main_mp.h>
#include <game_mp/g_scr_main_mp.h>
#include <game_mp/g_spawn_mp.h>
#include <clientscript/scr_const.h>
#include <universal/com_math.h>

static bool __fastcall Actor_CoverArrival_Start_SP(actor_s *self, ai_state_t)
{
  Actor_SetOrientMode(self, AI_ORIENT_DONT_CHANGE);
  self->eAnimMode = AI_ANIM_USE_BOTH_DELTAS_ZONLY_PHYSICS;
  self->bUseGoalWeight = true;
  return true;
}

static void __fastcall Actor_CoverArrival_Finish_SP(actor_s *, ai_state_t)
{
}

static bool __fastcall Actor_CoverArrival_Resume_SP(actor_s *, ai_state_t)
{
  return false;
}

static actor_think_result_t __fastcall Actor_CoverArrival_Think_SP(actor_s *self)
{
  Actor_SetAnimScript(self, &g_animScriptTable[self->species]->cover_arrival,
      AI_MOVE_STOP, self->eAnimMode, AI_ANIM_FUNCTION_STOP);
  self->pushable = false;
  if (!Actor_IsAnimScriptAlive(self))
  {
    Actor_SetState(self, AIS_EXPOSED);
    Actor_SetAnimScript(self, Actor_GetStopAnim(self),
        AI_MOVE_STOP, AI_ANIM_MOVE_CODE, AI_ANIM_FUNCTION_STOP);
    Actor_ClearPath(self);
    return ACTOR_THINK_REPEAT;
  }
  self->pszDebugInfo = "cover_arrival";
  Actor_PreThink(self);
  float origin[3];
  float remainder[3];
  if (Vec3AddPartialOffset(self->ent->r.currentOrigin,
      self->arrivalInfo.animscriptOverrideOriginError, 0.25f, origin, remainder))
  {
    for (int i = 0; i < 3; ++i)
      self->ent->r.currentOrigin[i] = origin[i];
    for (int i = 0; i < 3; ++i)
      self->arrivalInfo.animscriptOverrideOriginError[i] = remainder[i];
  }
  Actor_UpdateOriginAndAngles(self);
  return ACTOR_THINK_DONE;
}

static bool __fastcall Actor_CustomAnim_Start_SP(actor_s *self, ai_state_t)
{
  Scr_Notify(self->ent, scr_const.begin_custom_anim, 0);
  return true;
}

static actor_think_result_t __fastcall Actor_CustomAnim_Think_SP(actor_s *self)
{
  self->pszDebugInfo = "animcustom";
  if ( !self->keepNodeDuringScriptedAnim )
  {
    Actor_ClearKeepClaimedNode(self);
    Sentient_ClaimNode(self->sentient, NULL);
  }
  Actor_ClearPath(self);
  Actor_AnimSpecific(self, &self->AnimScriptSpecific, AI_ANIM_USE_BOTH_DELTAS, true);
  if ( !Actor_IsAnimScriptAlive(self) )
  {
    Actor_PopState(self);
    return ACTOR_THINK_REPEAT;
  }
  Actor_PreThink(self);
  Actor_UpdateOriginAndAngles(self);
  return ACTOR_THINK_DONE;
}

// Retail state 9 uses the same callbacks in all four SP species tables. Its
// start/finish bodies toggle eFlags2 bit 1; its think body runs the species'
// scripted animscript, keeps the actor fixed to the scripted scene, and pops
// the state when either the GSC thread or per-entity scripted anim ends.
static bool __fastcall Actor_ScriptedAnim_Start_SP(actor_s *self, ai_state_t)
{
  self->ent->s.lerp.eFlags2 |= 2u;
  return true;
}

static void __fastcall Actor_ScriptedAnim_Finish_SP(actor_s *self, ai_state_t)
{
  self->ent->s.lerp.eFlags2 &= ~2u;
  GScr_LogScriptedAnimTransition_SP(self->ent, "state-finish");
  GScr_ClearScriptedAnim_SP(self->ent);
}

static actor_think_result_t __fastcall Actor_ScriptedAnim_Think_SP(actor_s *self)
{
  self->pszDebugInfo = "animscripted";
  Actor_ClearKeepClaimedNode(self);
  Sentient_ClaimNode(self->sentient, NULL);
  Actor_ClearPath(self);
  Actor_AnimSpecific(self, &g_animScriptTable[self->species]->scripted, AI_ANIM_USE_BOTH_DELTAS, true);
  self->pushable = false;

  if ( !Actor_IsAnimScriptAlive(self) )
  {
    if ( self->eSimulatedState[self->simulatedStateLevel] == AIS_SCRIPTEDANIM )
      Actor_PopState(self);
    return ACTOR_THINK_REPEAT;
  }

  GScr_UpdateScriptedAnim_SP(self->ent);
  if ( !GScr_IsScriptedAnimActive_SP(self->ent) )
  {
    Actor_PopState(self);
    return ACTOR_THINK_REPEAT;
  }

  Actor_PreThink(self);
  Actor_SetDesiredAngles(&self->CodeOrient, self->ent->r.currentAngles[0], self->ent->r.currentAngles[1]);
  self->Physics.vVelocity[0] = 0.0f;
  self->Physics.vVelocity[1] = 0.0f;
  self->Physics.vVelocity[2] = 0.0f;
  self->Physics.vWishDelta[0] = 0.0f;
  self->Physics.vWishDelta[1] = 0.0f;
  self->Physics.vWishDelta[2] = 0.0f;
  return ACTOR_THINK_DONE;
}
#endif

const ai_funcs_t AIDogFuncTable[12] =
{
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  {
    Actor_Dog_Exposed_Start,
    Actor_Dog_Exposed_Finish,
    Actor_Dog_Exposed_Suspend,
    Actor_Generic_Resume,
    Actor_Dog_Exposed_Think,
    Actor_Exposed_Touch,
    Actor_Generic_Pain
  },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  {
    Actor_BadPlace_Flee_Start,
    Actor_BadPlace_Flee_Finish,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_BadPlace_Flee_Think,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  {
    Actor_Death_Start,
    Actor_BadPlace_Flee_Finish,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_Death_Think,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  },
  {
    Actor_Pain_Start,
    Actor_Pain_Finish,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_Pain_Think,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#ifdef KISAK_SP
  {
    Actor_ScriptedAnim_Start_SP,
    Actor_ScriptedAnim_Finish_SP,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_ScriptedAnim_Think_SP,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  },
#else
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#endif
#ifdef KISAK_SP
  {
    Actor_CustomAnim_Start_SP,
    Actor_BadPlace_Flee_Finish,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_CustomAnim_Think_SP,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  },
#else
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#endif
  {
    Actor_Negotiation_Start,
    Actor_BadPlace_Flee_Finish,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_Negotiation_Think,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  }
};

#ifdef KISAK_SP
// Retail's species pointer table is at 0x00b750d8 and contains four entries:
// human 0x00a519e8, dog 0x00a51e68, zombie 0x00a51b68 and zombie-dog
// 0x00a51ce8.  Each retail table has 12 state rows.  State 1 in the zombie
// table uses the shared exposed start/finish/suspend/resume callbacks plus the
// zombie-specific think body at 0x005591f0; its remaining populated rows match
// the callbacks already reconstructed in AIDogFuncTable.  Keeping a distinct
// table here prevents AI_SPECIES_ZOMBIE (2) from indexing beyond the old MP
// one-entry AIFuncTable, which was the 0x007d7558 runtime fault.
const ai_funcs_t AIZombieFuncTable_SP[12] =
{
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  {
    Actor_Exposed_Start_SP,
    Actor_Exposed_Finish_SP,
    Actor_Generic_Suspend,
    Actor_Exposed_Resume_SP,
    Actor_Exposed_Think_SP,
    Actor_Exposed_Touch,
    Actor_Generic_Pain
  },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  {
    Actor_BadPlace_Flee_Start,
    Actor_BadPlace_Flee_Finish,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_BadPlace_Flee_Think,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  {
    Actor_Death_Start,
    Actor_BadPlace_Flee_Finish,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_Death_Think,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  },
  {
    Actor_Pain_Start,
    Actor_Pain_Finish,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_Pain_Think,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  },
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
  {
    Actor_ScriptedAnim_Start_SP,
    Actor_ScriptedAnim_Finish_SP,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_ScriptedAnim_Think_SP,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  },
#ifdef KISAK_SP
  {
    Actor_CustomAnim_Start_SP,
    Actor_BadPlace_Flee_Finish,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_CustomAnim_Think_SP,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  },
#else
  { NULL, NULL, NULL, NULL, NULL, NULL, NULL },
#endif
  {
    Actor_Negotiation_Start,
    Actor_BadPlace_Flee_Finish,
    Actor_Generic_Suspend,
    Actor_Generic_Resume,
    Actor_Negotiation_Think,
    Actor_Generic_Touch,
    Actor_Generic_Pain
  }
};

static const std::array<ai_funcs_t, 12> AIHumanFuncTable_SP = []
{
  std::array<ai_funcs_t, 12> table;
  for (int i = 0; i < 12; ++i)
  {
    table[i] = AIZombieFuncTable_SP[i];
    if (i)
      table[i].pfnReact = Actor_StateReact_SP;
  }
  table[AIS_EXPOSED].pfnThink = Actor_Human_Exposed_Think_SP;
  table[AIS_TURRET] = { Actor_Turret_Start_SP, Actor_Turret_Finish_SP, Actor_Turret_Suspend_SP,
      Actor_Generic_Resume, Actor_Turret_Think_SP, Actor_Exposed_Touch, Actor_Turret_Pain_SP, Actor_StateReact_SP };
  table[AIS_GRENADE_RESPONSE] = { Actor_Grenade_Start_SP, Actor_Grenade_Finish_SP,
      Actor_Grenade_Finish_SP, Actor_Grenade_Resume_SP, Actor_Grenade_Think_SP,
      Actor_Generic_Touch, Actor_Grenade_Pain_SP, Actor_Grenade_React_SP };
  table[AIS_COVERARRIVAL] = { Actor_CoverArrival_Start_SP, Actor_CoverArrival_Finish_SP,
      Actor_Generic_Suspend, Actor_CoverArrival_Resume_SP, Actor_CoverArrival_Think_SP,
      Actor_Generic_Touch, Actor_Generic_Pain, Actor_StateReact_SP };
  table[AIS_REACT] = { Actor_React_Start_SP, Actor_React_Finish_SP,
      Actor_Generic_Suspend, Actor_Generic_Resume, Actor_React_Think_SP,
      Actor_Generic_Touch, Actor_Generic_Pain, Actor_StateReact_SP };
  return table;
}();

const ai_funcs_t *AIFuncTable[MAX_AI_SPECIES] =
{
  AIHumanFuncTable_SP.data(),
  AIDogFuncTable,
  AIZombieFuncTable_SP,
  AIDogFuncTable
};
#else
const ai_funcs_t *AIFuncTable[MAX_AI_SPECIES] = { AIDogFuncTable };
#endif

