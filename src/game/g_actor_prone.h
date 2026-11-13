#pragma once

struct actor_s;

void __cdecl G_InitActorProneInfo(actor_s *actor);
#ifdef KISAK_SP
struct DObj;
struct XAnimTree_s;
int __cdecl G_ActorProneDuration_SP(double seconds);
void __cdecl G_EnterActorProne_SP(actor_s *actor, int duration);
void __cdecl G_ExitActorProne_SP(actor_s *actor, int duration);
void __cdecl G_UpdateActorProne_SP(actor_s *actor, bool performTrace);
void __cdecl G_ActorProneAnimGoal_SP(actor_s *actor, DObj *obj, XAnimTree_s *tree,
    unsigned int animIndex, float weight, float goalTime, float rate, bool complete);
#endif
