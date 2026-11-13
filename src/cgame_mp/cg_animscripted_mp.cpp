#include "cg_animscripted_mp.h"
#include "cg_local_mp.h"
#include "cg_ents_mp.h"
#include <qcommon/dobj_management.h>
#include <universal/com_math_anglevectors.h>
#include <clientscript/cscr_memorytree.h>
#include <client_mp/cl_cgame_mp.h>
#include <clientscript/cscr_stringlist.h>
#ifdef KISAK_SP
#include <xanim/xanim.h>
#include <xanim/dobj.h>
#include <xanim/xanim_clientnotify.h>
#include <qcommon/common.h>
#include <ragdoll/ragdoll.h>
#include <cstring>
#include <cmath>
#include <bgame/bg_misc.h>
#include <clientscript/cscr_vm.h>
#include <clientscript/scr_const.h>
int __cdecl CG_GetActorScriptedAnim_SP(int localClientNum, int entityNum);
#include "cg_sp_anim_snapshot.inl"
#endif

#ifdef KISAK_SP
namespace
{
const dvar_t *cg_defensive_anim_delay;
const dvar_t *ai_useServerAnims;
const dvar_t *ai_useServerAnimsEntity;
}

void __cdecl CG_RegisterAnimCommandDvars_SP()
{
    cg_defensive_anim_delay = _Dvar_RegisterBool("cg_defensive_anim_delay", true, 0, "");
    ai_useServerAnims = _Dvar_RegisterBool("ai_useServerAnims", false, 0, "");
    ai_useServerAnimsEntity = _Dvar_RegisterInt("ai_useServerAnimsEntity", -1, -1, 1023, 0x80, "");
}

static bool CG_IsAnimCommandRagdollRunning_SP(int handle)
{
    if (handle == 0 || handle == -1)
        return false;
    return Ragdoll_HandleBody(handle)->state >= BS_RUNNING;
}

void CG_ApplySelectedAnimCommand_SP(
    int localClientNum, DObj *obj, XAnimTree_s *tree,
    const AnimCommand_SP *command, int)
{
    if (ai_useServerAnims->current.enabled)
        return;
    if (ai_useServerAnimsEntity->current.integer == command->entNum)
        return;
    cg_s *cg = CG_GetLocalClientGlobals(0);
    unsigned int elapsedBits = static_cast<unsigned int>(cg->time)
        - static_cast<unsigned int>(command->serverTime);
    int elapsedMs;
    memcpy(&elapsedMs, &elapsedBits, sizeof(elapsedMs));
    if (elapsedMs < 0)
        elapsedMs = 0;
    centity_s *cent = CG_GetEntity(0, static_cast<int>(obj->entnum) - 1);
    if (cent && cent->pose.isRagdoll
        && CG_IsAnimCommandRagdollRunning_SP(cent->pose.ragdollHandle))
        return;
    XAnimClientNotifyList notifyList;
    DObjSetClientNotifies(&notifyList);
    switch (command->type)
    {
    case 1:
        if (command->flags & 1)
            XAnimClearTreeGoalWeights(tree, command->animIndex, command->goalTime, -1);
        else
            XAnimClearGoalWeight(tree, command->animIndex, command->goalTime, static_cast<unsigned short>(-1));
        break;
    case 2:
        XAnimClearTreeGoalWeightsStrict(tree, command->animIndex, command->goalTime, -1);
        break;
    case 3:
        if (command->flags & 1)
            XAnimSetCompleteGoalWeight(obj, command->animIndex, command->weight,
                command->goalTime, command->rate, 0,
                command->weight > 0.001f ? 2 : 0, (command->flags >> 1) & 1, -1);
        else
            XAnimSetGoalWeight(obj, command->animIndex, command->weight,
                command->goalTime, command->rate, 0,
                command->weight > 0.001f ? 2 : 0, (command->flags >> 1) & 1, -1);
        XAnimApplyClientCommandCatchup_SP(obj, command->animIndex, elapsedMs);
        break;
    case 4:
        if (command->flags & 1)
            XAnimSetCompleteGoalWeightKnob(obj, command->animIndex, command->weight,
                command->goalTime, command->rate, 0,
                command->weight > 0.001f ? 2 : 0, (command->flags >> 1) & 1, -1);
        else
            XAnimSetGoalWeightKnob(obj, command->animIndex, command->weight,
                command->goalTime, command->rate, 0,
                command->weight > 0.001f ? 2 : 0, (command->flags >> 1) & 1, -1);
        XAnimApplyClientCommandCatchup_SP(obj, command->animIndex, elapsedMs);
        break;
    case 5:
        if (command->flags & 1)
            XAnimSetCompleteGoalWeightKnobAll(obj, command->animIndex, command->rootAnimIndex,
                command->weight, command->goalTime, command->rate, 0,
                command->weight > 0.001f ? 2 : 0, (command->flags >> 1) & 1, -1);
        else
            XAnimSetGoalWeightKnobAll(obj, command->animIndex, command->rootAnimIndex,
                command->weight, command->goalTime, command->rate, 0,
                command->weight > 0.001f ? 2 : 0, (command->flags >> 1) & 1, -1);
        XAnimApplyClientCommandCatchup_SP(obj, command->animIndex, elapsedMs);
        break;
    case 6:
        XAnimSetTime(tree, command->animIndex, command->goalTime, 0xFFFFu);
        break;
    }
    if (!cg->clientSaveRestore)
        CG_ProcessFakeEntClientNoteTracks(localClientNum, command->entNum);
    DObjClearClientNotifies();
}

void __cdecl CG_InitializeSelectedAnimCommandPointers_SP(snapshot_s *snapshot)
{
    if (snapshot->animCommandPointersInitialized)
        return;
    for (int commandNum = 0; commandNum < snapshot->numAnimCommands; ++commandNum)
        snapshot->animCommandPointers[commandNum] = &snapshot->animCommands[commandNum];
    snapshot->animCommandPointersInitialized = 1;
}

int CG_ProcessAnimCommands_SP(int localClientNum, snapshot_s *snapshot)
{
    cg_s *cg = CG_GetLocalClientGlobals(localClientNum);
    int applied = 0;
    for (int commandNum = 0; commandNum < snapshot->numAnimCommands; ++commandNum)
    {
        const AnimCommand_SP *command = snapshot->animCommandPointers[commandNum];
        if (snapshot->animCommandProcessed[commandNum])
            continue;
        centity_s *cent = CG_GetEntity(localClientNum, command->entNum);
        if (command->serverTime < cent->animCommandTime)
            cent->animCommandTime = command->serverTime;
        if (cg_defensive_anim_delay->current.enabled)
        {
            CG_GetClientInfoForPlayerEnt_SP(localClientNum, cg->clientNum);
            if (command->serverTime > cg->time)
                continue;
        }
        if (command->serverTime >= cg->lastAppliedAnimCommandTime)
        {
            DObj *obj = Com_GetClientDObj(command->entNum, localClientNum);
            if (!obj)
                continue;
            XAnimTree_s *tree = DObjGetTree(obj);
            if (!tree || static_cast<unsigned int>(command->animIndex) >= XAnimGetAnimTreeSize(tree->anims))
                continue;
            CG_ApplySelectedAnimCommand_SP(localClientNum, obj, tree, command, snapshot->serverTime);
            ++applied;
            cg->lastAppliedAnimCommandTime = command->serverTime;
        }
        snapshot->animCommandProcessed[commandNum] = 1;
    }
    return applied;
}

void CG_ProcessSnapshotAnimCommands_SP(int localClientNum, snapshot_s *current, snapshot_s *next)
{
    cg_s *cg = CG_GetLocalClientGlobals(localClientNum);
    if (current && CG_ProcessAnimCommands_SP(localClientNum, current))
    {
        unsigned int value = static_cast<unsigned int>(cg->lastAppliedAnimCommandTime) + 1;
        memcpy(&cg->lastAppliedAnimCommandTime, &value, sizeof(value));
    }
    if (next && CG_ProcessAnimCommands_SP(localClientNum, next))
    {
        unsigned int value = static_cast<unsigned int>(cg->lastAppliedAnimCommandTime) + 1;
        memcpy(&cg->lastAppliedAnimCommandTime, &value, sizeof(value));
    }
}

int CG_ReplayAnimCommandsForEntity_SP(int localClientNum, snapshot_s *snapshot, int entityNum)
{
    if (!snapshot)
        return 0;
    CG_InitializeSelectedAnimCommandPointers_SP(snapshot);
    DObj *obj = Com_GetClientDObj(entityNum, localClientNum);
    if (!obj)
        return 0;
    XAnimTree_s *tree = DObjGetTree(obj);
    if (!tree)
        return 0;
    int startTime = 0;
    for (int commandNum = 0; commandNum < snapshot->numAnimCommands; ++commandNum)
    {
        const AnimCommand_SP *command = snapshot->animCommandPointers[commandNum];
        if (command->entNum != entityNum)
            continue;
        if (!startTime)
            startTime = command->serverTime;
        if (cg_defensive_anim_delay->current.enabled)
        {
            cg_s *cg = CG_GetLocalClientGlobals(localClientNum);
            CG_GetClientInfoForPlayerEnt_SP(localClientNum, cg->clientNum);
            if (command->serverTime > cg->time)
                continue;
        }
        if (static_cast<unsigned int>(command->animIndex) >= XAnimGetAnimTreeSize(tree->anims))
            continue;
        CG_ApplySelectedAnimCommand_SP(localClientNum, obj, tree, command, snapshot->serverTime);
        snapshot->animCommandProcessed[commandNum] = 1;
    }
    return startTime;
}

void __cdecl CG_InitializeEntityAnimCommandTime_SP(int localClientNum, int entityNum)
{
    centity_s *cent = CG_GetEntity(localClientNum, entityNum);
    const entityType_t type = cent->nextState.eType;
    if (type != ET_SCRIPTMOVER && type != ET_VEHICLE && type != ET_ACTOR && type != ET_ACTOR_CORPSE)
        return;
    cg_s *cg = CG_GetLocalClientGlobals(localClientNum);
    const int current = CG_ReplayAnimCommandsForEntity_SP(localClientNum, cg->snap, entityNum);
    const int next = CG_ReplayAnimCommandsForEntity_SP(localClientNum, cg->nextSnap, entityNum);
    cent->animCommandTime = current;
    if (next < current || current == 0)
        cent->animCommandTime = next;
}

void __cdecl CG_ApplyPendingAnimCommandsForDObj_SP(int localClientNum, int entNum, DObj *obj)
{
    if (CG_ApplyRemoteAnimSnapshot_SP(localClientNum, entNum, obj, true))
        return;
    if (!obj || !obj->localTree)
        return;
    CG_InitializeEntityAnimCommandTime_SP(localClientNum, entNum);
}

void __cdecl CG_ApplyPendingAnimCommands_SP(int localClientNum)
{
    cg_s *cg = CG_GetLocalClientGlobals(localClientNum);
    CG_ProcessSnapshotAnimCommands_SP(localClientNum, cg->snap, cg->nextSnap);
    if (!com_sv_running->current.enabled)
        for (int entNum = 0; entNum < 1023; ++entNum)
            CG_ApplyRemoteAnimSnapshot_SP(localClientNum, entNum,
                Com_GetClientDObj(entNum, localClientNum), false);
}
#endif

void __cdecl CG_GetTagMatrix(int localClientNum, int linkEntNum, unsigned __int16 tagName, float (*resultTagMat)[3])
{
    centity_s *centLink; // [esp+14h] [ebp-8h]
    DObj *objLink; // [esp+18h] [ebp-4h]

    centLink = CG_GetEntity(localClientNum, linkEntNum);
    if ( ((centLink->clientFlags >> 1) & 1) != 0 )
    {
        objLink = GetLinkEntDObj(localClientNum, centLink);
        if ( !objLink || !CG_DObjGetWorldTagMatrix(&centLink->pose, objLink, tagName, resultTagMat, &resultTagMat[3][0]) )
        {
            MatrixIdentity33(resultTagMat);
            resultTagMat[3][0] = 0.0f;
            resultTagMat[3][1] = 0.0f;
            resultTagMat[3][2] = 0.0f;
        }
    }
    else
    {
        Com_PrintWarning(14, "An actor is linking to an entity that was not included in latest snapshot.\n");
        MatrixIdentity33(resultTagMat);
        resultTagMat[3][0] = 0.0f;
        resultTagMat[3][1] = 0.0f;
        resultTagMat[3][2] = 0.0f;
    }
}

DObj *__cdecl GetLinkEntDObj(int localClientNum, centity_s *centLink)
{
    if ( ((centLink->clientFlags >> 1) & 1) == 0
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp",
                    12,
                    0,
                    "%s",
                    "centLink->nextValid") )
    {
        __debugbreak();
    }
    return Com_GetClientDObj(centLink->nextState.number, localClientNum);
}

void __cdecl CG_CalcTagParentAxis(int localClientNum, centity_s *cent, float (*parentAxis)[3])
{
    centity_s *centLink; // [esp+40h] [ebp-8h]
    cLinkInfo_s *linkInfo; // [esp+44h] [ebp-4h]

    if ( !cent
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp", 54, 0, "%s", "cent") )
    {
        __debugbreak();
    }
    linkInfo = cent->linkInfo;
    if ( !linkInfo
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp", 57, 0, "%s", "linkInfo") )
    {
        __debugbreak();
    }
#ifdef KISAK_SP
    if (linkInfo->linkTag < 0)
#else
    if ( linkInfo->linkTag <= 0 )
#endif
    {
        centLink = CG_GetEntity(localClientNum, linkInfo->linkEnt);
        AnglesToAxis(centLink->pose.angles, parentAxis);
        parentAxis[3][0] = centLink->pose.origin[0];
        parentAxis[3][1] = centLink->pose.origin[1];
        parentAxis[3][2] = centLink->pose.origin[2];
    }
    else
    {
#ifdef KISAK_SP
        float tagAxis[4][3];
        CG_GetTagMatrix(localClientNum, linkInfo->linkEnt, (unsigned short)linkInfo->linkTag, tagAxis);
        MatrixMultiply43(linkInfo->axis, tagAxis, parentAxis);
#else
        CG_GetTagMatrix(localClientNum, linkInfo->linkEnt, linkInfo->linkTag, parentAxis);
#endif
    }
    if ( ((LODWORD(parentAxis[0][0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(parentAxis[0][1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(parentAxis[0][2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp",
                    71,
                    0,
                    "%s",
                    "!IS_NAN((parentAxis[0])[0]) && !IS_NAN((parentAxis[0])[1]) && !IS_NAN((parentAxis[0])[2])") )
    {
        __debugbreak();
    }
    if ( ((LODWORD(parentAxis[1][0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(parentAxis[1][1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(parentAxis[1][2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp",
                    72,
                    0,
                    "%s",
                    "!IS_NAN((parentAxis[1])[0]) && !IS_NAN((parentAxis[1])[1]) && !IS_NAN((parentAxis[1])[2])") )
    {
        __debugbreak();
    }
    if ( ((LODWORD(parentAxis[2][0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(parentAxis[2][1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(parentAxis[2][2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp",
                    73,
                    0,
                    "%s",
                    "!IS_NAN((parentAxis[2])[0]) && !IS_NAN((parentAxis[2])[1]) && !IS_NAN((parentAxis[2])[2])") )
    {
        __debugbreak();
    }
    if ( ((LODWORD(parentAxis[3][0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(parentAxis[3][1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(parentAxis[3][2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp",
                    74,
                    0,
                    "%s",
                    "!IS_NAN((parentAxis[3])[0]) && !IS_NAN((parentAxis[3])[1]) && !IS_NAN((parentAxis[3])[2])") )
    {
        __debugbreak();
    }
}

#ifdef KISAK_SP
static actorInfo_t *CG_GetActorLinkInfo_SP(int localClientNum, int entityNum)
{
    const centity_s *cent = CG_GetEntity(localClientNum, entityNum);
    if (cent->nextState.eType != ET_ACTOR)
        return nullptr;
    bgs_t *context = bgs ? bgs : &CG_GetLocalClientGlobals(0)->bgs;
    return &context->actorinfo[cent->nextState.lerp.u.actor.actorNum];
}

int CG_GetLinkToParent_SP(const centity_s *cent)
{
    if ((cent->clientFlags & 2u) == 0)
        return ENTITYNUM_NONE;
    const int localClientNum = cent->pose.localClientNum;
    if (cent->nextState.eType == ET_ACTOR)
    {
        const actorInfo_t *info = CG_GetActorLinkInfo_SP(localClientNum, cent->nextState.number);
        if (!info)
            return ENTITYNUM_NONE;
        if (!cent->linkInfo)
            return cent->nextState.otherEntityNum == ENTITYNUM_WORLD
                ? ENTITYNUM_NONE : cent->nextState.otherEntityNum;
        return info->attachedEntNum;
    }
    if (cent->nextState.eType == ET_PLAYER)
    {
        const clientInfo_t *info = CG_GetClientInfoForPlayerEnt_SP(localClientNum, cent->nextState.number);
        return info ? info->attachedEntNum : ENTITYNUM_NONE;
    }
    return cent->linkInfo ? cent->linkInfo->linkEnt : ENTITYNUM_NONE;
}

static int CG_GetRootActorAnimStartTime_SP(int localClientNum, int entityNum)
{
    actorInfo_t *info = CG_GetActorLinkInfo_SP(localClientNum, entityNum);
    return info ? info->animScriptedAnimTime : 0;
}

int CG_GetEntityLinkParent_SP(int localClientNum, const centity_s *cent)
{
    if (cent->nextState.eType == ET_SCRIPTMOVER)
        return cent->nextState.lerp.u.scriptMover.attachedEntNum;
    if (cent->nextState.eType == ET_ACTOR)
    {
        actorInfo_t *info = CG_GetActorLinkInfo_SP(localClientNum, cent->nextState.number);
        if (CG_GetLocalClientGlobals(0)->time >= info->attachedEntTime)
            info->attachedEntNum = info->nextAttachedEntNum;
        return info->attachedEntNum;
    }
    if (cent->nextState.eType == ET_PLAYER)
        return CG_GetClientInfoForPlayerEnt_SP(localClientNum, cent->nextState.number)->attachedEntNum;
    return ENTITYNUM_NONE;
}

static const char *CG_GetEntityLinkTagName_SP(int localClientNum, const centity_s *cent)
{
    if (cent->nextState.eType == ET_SCRIPTMOVER && cent->nextState.lerp.u.scriptMover.attachedTagIndex)
        return CL_GetConfigString(static_cast<unsigned int>(cent->nextState.lerp.u.scriptMover.attachedTagIndex) + 3115u);
    if (cent->nextState.eType == ET_ACTOR)
        return CG_GetActorLinkInfo_SP(localClientNum, cent->nextState.number)->attachedTagName;
    if (cent->nextState.eType == ET_PLAYER)
        return CG_GetClientInfoForPlayerEnt_SP(localClientNum, cent->nextState.number)->attachedTagName;
    return "";
}

static bool CG_RootAnimIsAvailable_SP(int localClientNum, const centity_s *cent, int animIndex)
{
    if (!animIndex)
        return false;
    cg_s *cg = CG_GetLocalClientGlobals(0);
    const clientInfo_t *player = CG_GetClientInfoForPlayerEnt_SP(localClientNum, cg->clientNum);
    const int entityNum = cent->nextState.number;
    if (cg->predictedPlayerState.viewlocked_entNum != entityNum &&
        (!player || player->attachedEntNum != entityNum) &&
        cg_defensive_anim_delay->current.enabled)
    {
        if (cent->nextState.eType == ET_SCRIPTMOVER)
        {
            if (cent->nextState.lerp.u.scriptMover.animScriptedAnimTime > cg->time)
                return false;
        }
        else if (CG_GetRootActorAnimStartTime_SP(localClientNum, entityNum) > cg->time)
        {
            return false;
        }
    }
    return true;
}

void CG_TransformEntityRoot_SP(int localClientNum, centity_s *cent,
    const float (*rootAxis)[3], float *resultOrigin, float *resultAngles)
{
    DObj *obj = Com_GetClientDObj(cent->nextState.number, localClientNum);
    if (!obj || !DObjGetTree(obj))
        return;
    float rotation[2];
    float translation[3];
    XAnimCalcAbsDelta(obj, cent->rootInfo_SP->animIndex, rotation, translation);
    MatrixTransformVector43(translation, rootAxis, resultOrigin);
    float yawAxis[3][3];
    float resultAxis[3][3];
    YawToAxis(static_cast<float>(RotationToYaw(rotation)), yawAxis);
    MatrixMultiply(yawAxis, rootAxis, resultAxis);
    AxisToAngles(resultAxis, resultAngles);
}

static void CG_InterpolateRootAngles_SP(const float *from, const float *to,
    float interpolation, float *result)
{
    for (int i = 0; i < 3; ++i)
    {
        const float base = from[i];
        volatile float delta = to[i] - base;
        volatile float revolutions = delta * 0.002777777845069766f;
        volatile float shifted = revolutions + 0.5f;
        const float signedMagic = std::copysign(8388608.0f, shifted);
        volatile float roundedSum = shifted + signedMagic;
        volatile float rounded = roundedSum - signedMagic;
        volatile float difference = rounded - shifted;
        if (!(difference <= std::copysign(0.0f, shifted)))
            rounded -= 1.0f;
        volatile float fraction = revolutions - rounded;
        volatile float scaled = fraction * interpolation;
        volatile float degrees = scaled * 360.0f;
        volatile float interpolated = degrees + base;
        result[i] = interpolated;
    }
}

void CG_CaptureRootExitPoseOffsets_SP(int localClientNum, centity_s *cent)
{
    cg_s *cg = CG_GetLocalClientGlobals(0);
    const float fraction = cg->frameInterpolation;
    cent->currentState.apos.trBase[0] = AngleNormalize180(cent->currentState.apos.trBase[0]);
    cent->nextState.lerp.apos.trBase[0] = AngleNormalize180(cent->nextState.lerp.apos.trBase[0]);
    float currentOrigin[3];
    float nextOrigin[3];
    float currentAngles[3];
    float nextAngles[3];
    BG_EvaluateTrajectory(&cent->currentState.pos, cg->snap->serverTime, currentOrigin);
    BG_EvaluateTrajectory(&cent->nextState.lerp.pos, cg->nextSnap->serverTime, nextOrigin);
    float interpolatedOrigin[3];
    for (int i = 0; i < 3; ++i)
        interpolatedOrigin[i] = (nextOrigin[i] - currentOrigin[i]) * fraction + currentOrigin[i];
    BG_EvaluateTrajectory(&cent->currentState.apos, cg->snap->serverTime, currentAngles);
    BG_EvaluateTrajectory(&cent->nextState.lerp.apos, cg->nextSnap->serverTime, nextAngles);
    float interpolatedAngles[3];
    CG_InterpolateRootAngles_SP(currentAngles, nextAngles, fraction, interpolatedAngles);
    for (int i = 0; i < 3; ++i)
        cent->rootOriginOffset_SP[i] = cent->pose.origin[i] - interpolatedOrigin[i];
    AnglesSubtract(cent->pose.angles, interpolatedAngles, cent->rootAnglesOffset_SP);
    cent->clientFlags |= CENT_FLAG_SP_ROOT_CONTINUITY;
}

void CG_UpdateRootInfo_SP(int localClientNum, centity_s *cent)
{
    if (cent->nextState.eType != ET_SCRIPTMOVER && cent->nextState.eType != ET_ACTOR)
    {
        if (cent->rootInfo_SP)
        {
            MT_Free(reinterpret_cast<unsigned char *>(cent->rootInfo_SP), sizeof(cgRootInfo_SP), SCRIPTINSTANCE_SERVER);
            cent->rootInfo_SP = nullptr;
        }
        return;
    }
    int animIndex;
    if (cent->nextState.eType == ET_SCRIPTMOVER)
    {
        if (cent->nextState.lerp.u.scriptMover.animScriptedAnimTime <= CG_GetLocalClientGlobals(0)->time)
            animIndex = cent->nextState.lerp.u.scriptMover.animScriptedAnim;
        else
            animIndex = cent->currentState.u.scriptMover.animScriptedAnim;
    }
    else
    {
        animIndex = CG_GetActorScriptedAnim_SP(localClientNum, cent->nextState.number);
    }
    const bool available = CG_RootAnimIsAvailable_SP(localClientNum, cent, animIndex);
    cgRootInfo_SP *root = cent->rootInfo_SP;
    if (!available)
    {
        if (!root)
            return;
        if (animIndex)
        {
            root->valid = 0;
            return;
        }
        MT_Free(reinterpret_cast<unsigned char *>(root), sizeof(cgRootInfo_SP), SCRIPTINSTANCE_SERVER);
        cent->rootInfo_SP = nullptr;
        if (cent->linkInfo)
        {
            MT_Free(reinterpret_cast<unsigned char *>(cent->linkInfo), sizeof(cLinkInfo_s), SCRIPTINSTANCE_SERVER);
            cent->linkInfo = nullptr;
            CG_UpdateEntityLink(localClientNum, cent);
        }
        if (!cent->linkInfo)
            CG_CaptureRootExitPoseOffsets_SP(localClientNum, cent);
        return;
    }
    if (!root)
    {
        root = reinterpret_cast<cgRootInfo_SP *>(MT_Alloc(sizeof(cgRootInfo_SP), 18, SCRIPTINSTANCE_SERVER));
        cent->rootInfo_SP = root;
        root->animIndex = 0;
        root->valid = 0;
    }
    if (root->valid && root->animIndex == animIndex)
        return;
    root->valid = 1;
    root->animIndex = static_cast<unsigned short>(animIndex);
    AnglesToAxis(cent->nextState.lerp.apos.trDelta, root->axis);
    for (int i = 0; i < 3; ++i)
        root->axis[3][i] = cent->nextState.lerp.pos.trDelta[i];
    float origin[3];
    float angles[3];
    for (int i = 0; i < 3; ++i)
    {
        origin[i] = cent->pose.origin[i];
        angles[i] = cent->pose.angles[i];
    }
    if (CG_EntityLinked(localClientNum, cent))
    {
        CG_LinkTransformForEntity(localClientNum, cent, origin, angles);
    }
    else
    {
        CG_TransformEntityRoot_SP(localClientNum, cent, root->axis, origin, angles);
        for (int i = 0; i < 3; ++i)
            cent->rootOriginOffset_SP[i] = cent->pose.origin[i] - origin[i];
        AnglesSubtract(cent->pose.angles, angles, cent->rootAnglesOffset_SP);
    }
    cent->clientFlags |= CENT_FLAG_SP_ROOT_CONTINUITY;
    if (cent->linkInfo)
    {
        MatrixIdentity33(cent->linkInfo->axis);
        cent->linkInfo->axis[3][0] = 0.0f;
        cent->linkInfo->axis[3][1] = 0.0f;
        cent->linkInfo->axis[3][2] = 0.0f;
    }
}

void CG_ApplyRootInfo_SP(int localClientNum, centity_s *cent)
{
    const cgRootInfo_SP *root = cent->rootInfo_SP;
    if (!root)
        return;
    if (CG_EntityLinked(localClientNum, cent))
        CG_LinkTransformForEntity(localClientNum, cent, cent->pose.origin, cent->pose.angles);
    else
        CG_TransformEntityRoot_SP(localClientNum, cent, root->axis, cent->pose.origin, cent->pose.angles);
}

static void CG_InterpolateScriptMoverAngles_SP(int localClientNum, centity_s *cent)
{
    cg_s *cg = CG_GetLocalClientGlobals(0);
    for (int index = 0; index < cg->cameraLinkedEntitiesCount; ++index)
        if (cg->cameraLinkedEntities[index] == cent->nextState.number)
            return;
    if (cent->nextState.number >= 1024)
    {
        BG_EvaluateTrajectory(&cent->nextState.lerp.apos, cg->time, cent->pose.angles);
        fake_centity_s *fake = CG_GetFakeEntity(localClientNum, cent->nextState.number);
        const unsigned int endBits = static_cast<unsigned int>(cent->nextState.lerp.apos.trTime)
            + static_cast<unsigned int>(cent->nextState.lerp.apos.trDuration);
        int endTime;
        memcpy(&endTime, &endBits, sizeof(endTime));
        if ((fake->flags & 2u) && cg->time > endTime)
        {
            fake->flags &= ~2u;
            CScr_NotifyNum(localClientNum, cent->nextState.number, 0, cscr_const.rotatedone, 0);
        }
        return;
    }
    float current[3], next[3];
    BG_EvaluateTrajectory(&cent->currentState.apos, cg->snap->serverTime, current);
    BG_EvaluateTrajectory(&cent->nextState.lerp.apos, cg->nextSnap->serverTime, next);
    CG_InterpolateRootAngles_SP(current, next, cg->frameInterpolation, cent->pose.angles);
    if (cent->nextState.eType == ET_ACTOR
        && ((zombiemode->current.enabled && cent->nextState.lerp.u.actor.species == 3)
            || cent->allowPitch))
    {
        const float currentPitch[3] = {
            static_cast<float>(cent->currentState.u.actor.proneInfo.fBodyPitch) * 0.0054931640625f,
            0.0f, 0.0f
        };
        const float nextPitch[3] = {
            static_cast<float>(cent->nextState.lerp.u.actor.proneInfo.fBodyPitch) * 0.0054931640625f,
            0.0f, 0.0f
        };
        float interpolatedPitch[3];
        CG_InterpolateRootAngles_SP(currentPitch, nextPitch, cg->frameInterpolation, interpolatedPitch);
        cent->pose.actor.pitch = interpolatedPitch[0];
        cent->pose.actor.proneType = 1;
    }
}

static void CG_LinkScriptMoverTransform_SP(int localClientNum, centity_s *cent, float *origin, float *angles)
{
    float parentAxis[4][3], parentAngles[3], matrix[4][3];
    CG_CalcTagParentAxis(localClientNum, cent, parentAxis);
    AxisToAngles(parentAxis, parentAngles);
    if (cent->rootInfo_SP)
    {
        CG_TransformEntityRoot_SP(localClientNum, cent, parentAxis, origin, angles);
        return;
    }
    if (cent->linkInfo->linkTag >= 0)
    {
        if (origin)
            Vec3Copy(parentAxis[3], origin);
        if (angles)
        {
            Vec3Copy(parentAngles, angles);
            if (cent->clientFlags & 0x400000u)
                angles[1] = cent->currentState.apos.trBase[1];
        }
        return;
    }
    MatrixMultiply43(cent->linkInfo->axis, parentAxis, matrix);
    if (origin)
        Vec3Copy(matrix[3], origin);
    if (angles)
    {
        if (cent->currentState.apos.trType == TR_INTERPOLATE)
            AxisToAngles(matrix, angles);
        else
            CG_InterpolateScriptMoverAngles_SP(localClientNum, cent);
    }
}

bool CG_ActorLinked_SP(int localClientNum, int entityNum)
{
    const centity_s *cent = CG_GetEntity(localClientNum, entityNum);
    const actorInfo_t *info = CG_GetActorLinkInfo_SP(localClientNum, entityNum);
    if (!info || !cent->linkInfo || info->attachedEntNum == 1023)
        return false;
    return CG_GetEntity(localClientNum, info->attachedEntNum)->nextState.eType != ET_MG42;
}

void CG_ActorLinkTransform_SP(int localClientNum, int entityNum, float *origin, float *angles)
{
    centity_s *cent = CG_GetEntity(localClientNum, entityNum);
    if (!cent->rootInfo_SP)
    {
        const actorInfo_t *info = CG_GetActorLinkInfo_SP(localClientNum, cent->nextState.number);
        const int animTime = info ? info->animScriptedAnimTime : 0;
        if (animTime > CG_GetLocalClientGlobals(0)->time)
        {
            AnglesToAxis(cent->currentState.apos.trDelta, cent->linkInfo->axis);
            Vec3Copy(cent->currentState.pos.trDelta, cent->linkInfo->axis[3]);
        }
        else
        {
            AnglesToAxis(cent->nextState.lerp.apos.trDelta, cent->linkInfo->axis);
            Vec3Copy(cent->nextState.lerp.pos.trDelta, cent->linkInfo->axis[3]);
        }
    }
    CG_LinkTransformForEntity(localClientNum, cent, origin, angles);
}

#endif
void __cdecl CG_LinkTransformForEntity(int localClientNum, centity_s *cent, float *resultOrigin, float *resultAngles)
{
#ifdef KISAK_SP
    if (cent && (cent->nextState.eType == ET_SCRIPTMOVER || cent->nextState.eType == ET_ACTOR))
    {
        CG_LinkScriptMoverTransform_SP(localClientNum, cent, resultOrigin, resultAngles);
        return;
    }
#endif
    float *v4; // [esp+4h] [ebp-80h]
    float matrix[4][3]; // [esp+Ch] [ebp-78h] BYREF
    float origin[3]; // [esp+3Ch] [ebp-48h]
    float angles[3]; // [esp+48h] [ebp-3Ch] BYREF
    float parentAxis[4][3]; // [esp+54h] [ebp-30h] BYREF

    if ( !cent
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp", 89, 0, "%s", "cent") )
    {
        __debugbreak();
    }
    CG_CalcTagParentAxis(localClientNum, cent, parentAxis);
    if ( cent->nextState.clientLinkInfo.parentEnt )
    {
        AnglesToAxis(cent->nextState.lerp.apos.trDelta, cent->linkInfo->axis);
        v4 = cent->linkInfo->axis[3];
        *v4 = cent->nextState.lerp.pos.trDelta[0];
        v4[1] = cent->nextState.lerp.pos.trDelta[1];
        v4[2] = cent->nextState.lerp.pos.trDelta[2];
    }
    MatrixMultiply43(cent->linkInfo->axis, parentAxis, matrix);
    AxisToAngles(matrix, angles);
    *(_QWORD *)origin = *(_QWORD *)&matrix[3][0];
    origin[2] = matrix[3][2];
    if ( resultOrigin )
    {
        *resultOrigin = origin[0];
        resultOrigin[1] = origin[1];
        resultOrigin[2] = origin[2];
    }
    if ( resultAngles )
    {
        *resultAngles = angles[0];
        resultAngles[1] = angles[1];
        resultAngles[2] = angles[2];
    }
}

void __cdecl CG_GenerateLinkInfo(int localClientNum, centity_s *cent, int attachedEntNum, int attachedTagIndex)
{
    float *v4; // [esp+0h] [ebp-A4h]
    float *relative_angles; // [esp+10h] [ebp-94h]
    float invParentAxis[4][3]; // [esp+14h] [ebp-90h] BYREF
    float parentAxis[4][3]; // [esp+44h] [ebp-60h] BYREF
    float axis[4][3]; // [esp+74h] [ebp-30h] BYREF

    if ( !cent->linkInfo )
    {
        cent->linkInfo = (cLinkInfo_s *)MT_Alloc(sizeof(cLinkInfo_s), 18, SCRIPTINSTANCE_SERVER);
        cent->linkInfo->linkEnt = 1023;
        relative_angles = cent->linkInfo->relative_angles;
        *relative_angles = 0.0f;
        relative_angles[1] = 0.0f;
        relative_angles[2] = 0.0f;
        cent->linkInfo->angles_set = 0;
    }
    if ( cent->linkInfo->linkEnt != attachedEntNum )
    {
        cent->linkInfo->linkEnt = attachedEntNum;
        if ( attachedTagIndex )
            cent->linkInfo->linkTag = attachedTagIndex;
        else
            cent->linkInfo->linkTag = -1;
        if ( cent->linkInfo->linkTag <= 0 )
        {
            v4 = cent->linkInfo->axis[3];
            *v4 = cent->nextState.lerp.pos.trBase[0];
            v4[1] = cent->nextState.lerp.pos.trBase[1];
            v4[2] = cent->nextState.lerp.pos.trBase[2];
            AnglesToAxis(cent->nextState.lerp.apos.trBase, cent->linkInfo->axis);
        }
        else
        {
            AnglesToAxis(cent->nextState.lerp.apos.trBase, axis);
            axis[3][0] = cent->nextState.lerp.pos.trBase[0];
            axis[3][1] = cent->nextState.lerp.pos.trBase[1];
            axis[3][2] = cent->nextState.lerp.pos.trBase[2];
            CG_GetTagMatrix(localClientNum, cent->linkInfo->linkEnt, cent->linkInfo->linkTag, parentAxis);
            MatrixInverseOrthogonal43(parentAxis, invParentAxis);
            MatrixMultiply43(axis, invParentAxis, cent->linkInfo->axis);
        }
    }
}

#ifdef KISAK_SP
static void CG_UpdatePlayerAttachmentLink_SP(int localClientNum, centity_s *cent)
{
    const clientInfo_t *info = CG_GetClientInfoForPlayerEnt_SP(localClientNum, cent->nextState.number);
    const int parentEnt = info->attachedEntNum;
    cLinkInfo_s *linkInfo = cent->linkInfo;
    if (parentEnt == 1023)
    {
        if (linkInfo)
        {
            MT_Free(reinterpret_cast<unsigned char *>(linkInfo), sizeof(cLinkInfo_s), SCRIPTINSTANCE_SERVER);
            cent->linkInfo = nullptr;
        }
        return;
    }
    if (!linkInfo)
    {
        linkInfo = reinterpret_cast<cLinkInfo_s *>(MT_Alloc(sizeof(cLinkInfo_s), 18, SCRIPTINSTANCE_SERVER));
        cent->linkInfo = linkInfo;
        linkInfo->linkEnt = 1023;
        linkInfo->relative_angles[0] = 0.0f;
        linkInfo->relative_angles[1] = 0.0f;
        linkInfo->relative_angles[2] = 0.0f;
        linkInfo->angles_set = 0;
    }
    if (linkInfo->linkEnt == parentEnt)
    {
        const char *tagName = CG_GetClientInfoForPlayerEnt_SP(localClientNum, cent->nextState.number)->attachedTagName;
        linkInfo->linkTag = tagName[0]
            ? static_cast<unsigned short>(SL_FindString(tagName, SCRIPTINSTANCE_SERVER)) : -1;
        return;
    }
    linkInfo->linkEnt = parentEnt;
    const char *tagName = CG_GetClientInfoForPlayerEnt_SP(localClientNum, cent->nextState.number)->attachedTagName;
    linkInfo->linkTag = tagName[0]
        ? static_cast<unsigned short>(SL_FindString(tagName, SCRIPTINSTANCE_SERVER)) : -1;
    AnglesToAxis(cent->nextState.lerp.apos.trDelta, linkInfo->axis);
    Vec3Copy(cent->nextState.lerp.pos.trDelta, linkInfo->axis[3]);
}
#endif

void __cdecl CG_UpdateEntityLink(int localClientNum, centity_s *cent)
{
#ifdef KISAK_SP
    const entityType_t type = cent->nextState.eType;
    if (type != ET_SCRIPTMOVER && type != ET_ACTOR && type != ET_PLAYER && type != ET_ACTOR_CORPSE)
        return;
    const int parentEnt = CG_GetEntityLinkParent_SP(localClientNum, cent);
    const bool isPlayer = cent->nextState.eType == ET_PLAYER;
    cLinkInfo_s *linkInfo = cent->linkInfo;
    if (parentEnt == 1023)
    {
        if (!linkInfo)
            return;
        MT_Free(reinterpret_cast<unsigned char *>(linkInfo), sizeof(cLinkInfo_s), SCRIPTINSTANCE_SERVER);
        cgRootInfo_SP *rootInfo = cent->rootInfo_SP;
        cent->linkInfo = nullptr;
        if (rootInfo)
        {
            MT_Free(reinterpret_cast<unsigned char *>(rootInfo), sizeof(cgRootInfo_SP), SCRIPTINSTANCE_SERVER);
            cent->rootInfo_SP = nullptr;
            CG_UpdateRootInfo_SP(localClientNum, cent);
        }
        return;
    }

    if (!linkInfo)
    {
        linkInfo = reinterpret_cast<cLinkInfo_s *>(MT_Alloc(sizeof(cLinkInfo_s), 18, SCRIPTINSTANCE_SERVER));
        cent->linkInfo = linkInfo;
        linkInfo->linkEnt = 1023;
        linkInfo->relative_angles[0] = 0.0f;
        linkInfo->relative_angles[1] = 0.0f;
        linkInfo->relative_angles[2] = 0.0f;
        linkInfo->angles_set = 0;
    }
    if (linkInfo->linkEnt == parentEnt)
    {
        if (isPlayer)
        {
            const char *tagName = CG_GetEntityLinkTagName_SP(localClientNum, cent);
            linkInfo->linkTag = tagName[0] ? static_cast<unsigned short>(SL_FindString(tagName, SCRIPTINSTANCE_SERVER)) : -1;
        }
        return;
    }

    linkInfo->linkEnt = parentEnt;
    if (cent->nextState.number >= 1024)
    {
        const int tag = cent->nextState.lerp.u.scriptMover.attachedTagIndex;
        linkInfo->linkTag = tag ? tag : -1;
    }
    else
    {
        const char *tagName = CG_GetEntityLinkTagName_SP(localClientNum, cent);
        linkInfo->linkTag = tagName[0] ? static_cast<unsigned short>(SL_FindString(tagName, SCRIPTINSTANCE_SERVER)) : -1;
    }
    if (cent->rootInfo_SP)
    {
        MatrixIdentity33(linkInfo->axis);
        linkInfo->axis[3][0] = 0.0f;
        linkInfo->axis[3][1] = 0.0f;
        linkInfo->axis[3][2] = 0.0f;
    }
    else
    {
        AnglesToAxis(cent->nextState.lerp.apos.trDelta, linkInfo->axis);
        Vec3Copy(cent->nextState.lerp.pos.trDelta, linkInfo->axis[3]);
    }
#else
    char *tagName; // [esp+0h] [ebp-Ch]
    __int16 parentEnt; // [esp+4h] [ebp-8h]
    __int16 parentEnta; // [esp+4h] [ebp-8h]
    unsigned int tagIndex; // [esp+8h] [ebp-4h]

    if ( cent->nextState.number < 1024 )
    {
        parentEnt = cent->nextState.clientLinkInfo.parentEnt;
        if ( parentEnt )
        {
            parentEnta = parentEnt - 1;
            if ( cent->nextState.clientLinkInfo.tagIndex >= 0x20u
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp",
                            172,
                            0,
                            "%s",
                            "cent->nextState.clientLinkInfo.tagIndex < MAX_TAGS") )
            {
                __debugbreak();
            }
            tagIndex = 0;
            if ( cent->nextState.clientLinkInfo.tagIndex )
            {
                tagName = CL_GetConfigString(cent->nextState.clientLinkInfo.tagIndex + 3115);
                tagIndex = SL_FindString(tagName, SCRIPTINSTANCE_SERVER);
            }
            if ( !cent->linkInfo || cent->linkInfo->linkEnt != parentEnta || cent->linkInfo->linkTag != tagIndex )
                CG_GenerateLinkInfo(localClientNum, cent, parentEnta, tagIndex);
        }
        else if ( cent->linkInfo )
        {
            cent->linkInfo->linkEnt = 1023;
        }
    }
#endif
}

void __cdecl CG_UpdateFakeEntityLink(int localClientNum, centity_s *cent, int parentNum, int tagIndex)
{
    cLinkInfo_s *linkInfo; // [esp+0h] [ebp-4h]

    if ( !cent
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp", 188, 0, "%s", "cent") )
    {
        __debugbreak();
    }
    if ( cent->nextState.number >= 0x600u
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp",
                    189,
                    0,
                    "cent->nextState.number doesn't index MAX_LOCAL_CENTITIES\n\t%i not in [0, %i)",
                    cent->nextState.number,
                    1536) )
    {
        __debugbreak();
    }
    linkInfo = cent->linkInfo;
    if ( parentNum == 1023 )
    {
        if ( linkInfo )
        {
            MT_Free((unsigned char*)linkInfo, sizeof(cLinkInfo_s), SCRIPTINSTANCE_SERVER);
            cent->linkInfo = 0;
        }
    }
    else
    {
        CG_GenerateLinkInfo(localClientNum, cent, parentNum, tagIndex);
    }
}

#ifdef KISAK_SP
void __cdecl CG_UpdateFakeEntityLink(int localClientNum, centity_s *cent)
{
    cLinkInfo_s *linkInfo;

    if ( !cent
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp", 207, 0, "%s", "cent") )
    {
        __debugbreak();
    }
    if ( cent->nextState.number >= 0x600u
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp",
                    208,
                    0,
                    "cent->nextState.number doesn't index MAX_LOCAL_CENTITIES\n\t%i not in [0, %i)",
                    cent->nextState.number,
                    1536) )
    {
        __debugbreak();
    }
    linkInfo = cent->linkInfo;
    if ( linkInfo
        && linkInfo->linkEnt != 1023
        && ((CG_GetEntity(localClientNum, cent->linkInfo->linkEnt)->clientFlags >> 1) & 1) == 0 )
    {
        MT_Free((unsigned char*)linkInfo, sizeof(cLinkInfo_s), SCRIPTINSTANCE_SERVER);
        cent->linkInfo = 0;
    }
}
#else
void __cdecl CG_UpdateFakeEntityLink(int localClientNum, centity_s *cent)
{
    cLinkInfo_s *linkInfo; // [esp+Ch] [ebp-4h]

    if ( !cent
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp", 207, 0, "%s", "cent") )
    {
        __debugbreak();
    }
    if ( cent->nextState.number >= 0x600u
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp",
                    208,
                    0,
                    "cent->nextState.number doesn't index MAX_LOCAL_CENTITIES\n\t%i not in [0, %i)",
                    cent->nextState.number,
                    1536) )
    {
        __debugbreak();
    }
    linkInfo = cent->linkInfo;
    if ( linkInfo
        && linkInfo->linkEnt != 1023
        && ((*((unsigned int *)CG_GetEntity(localClientNum, cent->linkInfo->linkEnt) + 201) >> 1) & 1) == 0 )
    {
        MT_Free((unsigned char*)linkInfo, sizeof(cLinkInfo_s), SCRIPTINSTANCE_SERVER);
        cent->linkInfo = 0;
    }
}
#endif

bool __cdecl CG_EntityLinked(int localClientNum, centity_s *cent)
{
#ifdef KISAK_SP
    return cent->linkInfo && (CG_GetEntity(localClientNum, cent->linkInfo->linkEnt)->clientFlags & 2u) != 0;
#else
    return CG_EntGetLinkToParent(localClientNum, cent) != 0;
#endif
}

centity_s *__cdecl CG_EntGetLinkToParent(int localClientNum, centity_s *cent)
{
    centity_s *centParent; // [esp+8h] [ebp-4h]

    if ( !cent->linkInfo || cent->linkInfo->linkEnt == 1023 )
        return 0;
    if ( cent->linkInfo->linkEnt >= 0x600u
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp",
                    234,
                    0,
                    "%s",
                    "cent->linkInfo->linkEnt >= 0 && cent->linkInfo->linkEnt < MAX_LOCAL_CENTITIES") )
    {
        __debugbreak();
    }
    centParent = CG_GetEntity(localClientNum, cent->linkInfo->linkEnt);
    if ( !centParent
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_animscripted_mp.cpp",
                    236,
                    0,
                    "%s",
                    "centParent") )
    {
        __debugbreak();
    }
    if ( ((centParent->clientFlags >> 1) & 1) != 0 )
        return centParent;
    Com_PrintWarning(
        14,
        "Entity #%i of type %i: parent ent #%i not in snapshot.\n",
        cent->nextState.number,
        cent->nextState.eType,
        cent->linkInfo->linkEnt);
    return 0;
}


#ifdef KISAK_SP
clientInfo_t *CG_GetClientInfoForPlayerEnt_SP(int localClientNum, int entityNum)
{
    const centity_s *cent = CG_GetEntity(localClientNum, entityNum);
    if (cent->nextState.eType != ET_PLAYER)
        return nullptr;
    return &bgs->clientinfo[static_cast<unsigned char>(cent->nextState.clientNum)];
}
#endif
