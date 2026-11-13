#include "g_actor_prone.h"
#include "actor.h"
#include <universal/assertive.h>

void __cdecl G_InitActorProneInfo(actor_s *actor)
{
    if ( !actor && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\g_actor_prone.cpp", 65, 0, "%s", "actor") )
        __debugbreak();
    *(unsigned int *)&actor->ProneInfo.bCorpseOrientation = 0;
    actor->ProneInfo.iProneTime = 0;
    actor->ProneInfo.iProneTrans = 0;
    actor->ProneInfo.fBodyHeight = 0.0;
    actor->ProneInfo.fTorsoPitch = 0.0;
    actor->ProneInfo.fWaistPitch = 0.0;
}

#ifdef KISAK_SP
#include <bit>
#include <cstdint>
#include <limits>
#include <game_mp/g_main_mp.h>
#include <game_mp/actor_mp.h>
#include <game_mp/g_anim_commands_sp.h>
#include <bgame/bg_misc.h>
#include <qcommon/dobj_management.h>
#include <qcommon/common.h>
#include <xanim/xanim.h>

static int G_ProneTimeSubtract_SP(int time, int delta)
{
    return std::bit_cast<int>(static_cast<uint32_t>(time) - static_cast<uint32_t>(delta));
}

static int G_ProneTruncate_SP(double value)
{
    return value >= -2147483648.0 && value < 2147483648.0
        ? static_cast<int>(value) : (std::numeric_limits<int>::min)();
}

int __cdecl G_ActorProneDuration_SP(double seconds)
{
    return G_ProneTruncate_SP(seconds * 1000.0);
}

void __cdecl G_ActorProneAnimGoal_SP(actor_s *actor, DObj *obj, XAnimTree_s *tree,
    unsigned int animIndex, float weight, float goalTime, float rate, bool complete)
{
    if (g_dumpAnimsCommands->current.integer == actor->ent->s.number)
    {
        const XAnim_s *anims = XAnimGetAnims(tree);
        Com_Printf(19,
            "%s (tree=%s anim=%s root=%s weight=%f time=%f rate=%f level time=%d)\n",
            "Actor SetAnim", anims->debugName, XAnimGetAnimDebugName(anims, animIndex),
            "none", weight, goalTime, rate, level.time);
    }
    const int command = G_StoreAnimCommand_SP(actor->ent, tree, 3, animIndex, 0,
        weight, goalTime, rate, complete ? 1 : 0);
    if (complete)
        XAnimSetCompleteGoalWeight(obj, animIndex, weight, goalTime, rate, 0, 0, 0, command);
    else
        XAnimSetGoalWeight(obj, animIndex, weight, goalTime, rate, 0, 0, 0, command);
}

void __cdecl G_EnterActorProne_SP(actor_s *actor, int duration)
{
    actor_prone_info_s *prone = &actor->ProneInfo;
    if (BG_ActorIsProne(prone, level.time))
    {
        if (!prone->iProneTrans || duration == prone->iProneTrans)
            return;
        const float fraction = static_cast<float>(BG_GetActorProneFraction(prone, level.time));
        if (fraction < 1.0f)
        {
            const float elapsed = static_cast<float>(duration) * fraction;
            prone->iProneTime = G_ProneTimeSubtract_SP(level.time, G_ProneTruncate_SP(elapsed));
        }
        prone->iProneTrans = duration;
        return;
    }
    prone->bCorpseOrientation = false;
    prone->iProneTime = level.time;
    prone->prone = true;
    actor->Physics.prone = true;
    prone->iProneTrans = duration;
    actor->bProneOK = BG_CheckProne(nullptr, actor->ent->s.number,
        actor->ent->r.currentOrigin, 15.0f, 48.0f, actor->ent->r.currentAngles[1],
        &prone->fTorsoPitch, &prone->fWaistPitch, false, true, true, 1,
        PCT_ACTOR, 50.0f);
}

void __cdecl G_ExitActorProne_SP(actor_s *actor, int duration)
{
    actor_prone_info_s *prone = &actor->ProneInfo;
    if (!BG_ActorIsProne(prone, level.time))
        return;
    if (prone->iProneTrans && duration != prone->iProneTrans)
    {
        const float fraction = static_cast<float>(BG_GetActorProneFraction(prone, level.time));
        const float elapsed = static_cast<float>(duration) * fraction;
        prone->iProneTime = G_ProneTimeSubtract_SP(level.time, G_ProneTruncate_SP(elapsed));
    }
    else
        prone->iProneTime = level.time;
    prone->iProneTrans = std::bit_cast<int>(0u - static_cast<uint32_t>(duration));
}

static float G_PronePitchNormalize_SP(float pitch)
{
    const float turns = pitch * 0.00277777784503996372222900390625f;
    const float shifted = turns + 0.5f;
    const uint32_t sign = std::bit_cast<uint32_t>(shifted) & 0x80000000u;
    const float magic = std::bit_cast<float>(0x4b000000u | sign);
    const float rounded = static_cast<float>(shifted + magic) - magic;
    const float difference = rounded - shifted;
    const float signedZero = std::bit_cast<float>(sign);
    const float correction = !(difference <= signedZero) ? 1.0f : 0.0f;
    const float floorValue = rounded - correction;
    return (turns - floorValue) * 360.0f;
}

void __cdecl G_UpdateActorProne_SP(actor_s *actor, bool performTrace)
{
    actor_prone_info_s *prone = &actor->ProneInfo;
    if (performTrace)
        actor->bProneOK = BG_CheckProne(nullptr, actor->ent->s.number,
            actor->ent->r.currentOrigin, 15.0f, 48.0f, actor->ent->r.currentAngles[1],
            &prone->fTorsoPitch, &prone->fWaistPitch, true, true, true, 1,
            PCT_ACTOR, 50.0f);
    else
        actor->fProneLastDiff = 360.0f;
    const float difference = G_PronePitchNormalize_SP(prone->fWaistPitch);
    if (actor->fProneLastDiff == difference)
        return;
    float lowWeight = 0.0f, highWeight = 0.0f;
    float weight;
    if (difference < 0.0f)
    {
        weight = actor->fInvProneAnimLowPitch * difference;
        if (weight > 0.99f)
            weight = 0.99f;
        lowWeight = weight;
    }
    else
    {
        weight = actor->fInvProneAnimHighPitch * difference;
        if (weight > 0.99f)
            weight = 0.99f;
        highWeight = weight;
    }
    const float levelWeight = 1.0f - weight;
    DObj *obj = Com_GetServerDObj(actor->ent->s.number);
    XAnimTree_s *tree = G_GetActorAnimTree(actor);
    G_ActorProneAnimGoal_SP(actor, obj, tree, actor->proneAnimLow,
        lowWeight, 0.05f, 1.0f, false);
    G_ActorProneAnimGoal_SP(actor, obj, tree, actor->proneAnimLevel,
        levelWeight, 0.05f, 1.0f, false);
    G_ActorProneAnimGoal_SP(actor, obj, tree, actor->proneAnimHigh,
        highWeight, 0.05f, 1.0f, false);
    actor->fProneLastDiff = difference;
}
#endif

