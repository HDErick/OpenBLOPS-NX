#include "cg_actors_mp.h"
#include "cg_local_mp.h"
#include <client/splitscreen.h>
#include <clientscript/cscr_vm.h>
#include <clientscript/scr_const.h>
#include <EffectsCore/fx_marks.h>
#include <qcommon/dobj_management.h>
#include "cg_ents_mp.h"
#include "cg_animtree_mp.h"
#include <xanim/dobj_utils.h>
#include "cg_main_mp.h"
#ifdef KISAK_SP
#include <qcommon/common.h>
#include "cg_view_mp.h"
#endif
#include <cgame/cg_scr_main.h>
#include <bgame/bg_dog.h>
#include <ragdoll/ragdoll.h>
#include <cgame/cg_world.h>
#include "cg_players_mp.h"
#include <gfx_d3d/r_scene.h>
#ifdef KISAK_SP
#include "cg_animscripted_mp.h"
#include <game_mp/g_main_mp.h>
#include <game_mp/g_utils_mp.h>
#include <ragdoll/ragdoll_update.h>
#include <gfx_d3d/r_model.h>
#include <gfx_d3d/r_foliage.h>
#include <gfx_d3d/r_dobj_skin.h>
#include <clientscript/cscr_animtree.h>
#include <bgame/bg_animation.h>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <bgame/bg_sp_anim_snapshot.h>
#include <clientscript/cscr_stringlist.h>
#include "cg_scr_main_mp.h"
#include <game/actor.h>
#include <ik/ik_import.h>
#include <EffectsCore/fx_system.h>
#ifdef KISAK_NX
#include <game_mp/g_main_mp.h>
#include <game/actor.h>
#endif
#endif

#ifdef KISAK_SP
const dvar_s *ai_useServerAnims;
const dvar_s *ai_useServerAnimsEntity;

void CG_RegisterActorAnimationSyncDvars_SP()
{
    ai_useServerAnims = _Dvar_RegisterBool("ai_useServerAnims", false, 0, "");
    ai_useServerAnimsEntity = _Dvar_RegisterInt("ai_useServerAnimsEntity", -1, -1, 1023, 0x80u, "");
}

static XAnim_s *CG_GetActorRequestedAnims_SP(int localClientNum, centity_s *cent)
{
    return com_sv_running->current.enabled
        ? Scr_GetAnims(cent->nextState.animTreeIndex, SCRIPTINSTANCE_SERVER)
        : Scr_GetClientAnims(cent->nextState.animTreeIndex, SCRIPTINSTANCE_SERVER);
}

static actorInfo_t *CG_GetActorTreeOwner_SP(cg_s *cgameGlob, cgs_t *cgs, centity_s *cent)
{
    if (cent->nextState.eType == ET_ACTOR)
    {
        const int actorNum = cent->nextState.lerp.u.actor.actorNum;
        iassert(actorNum >= 0 && actorNum < MAX_ACTORS);
        return &cgameGlob->bgs.actorinfo[actorNum];
    }
    const int corpseNum = cent->nextState.lerp.u.actor.actorNum;
    iassert(corpseNum >= 0 && corpseNum < MAX_ACTOR_CORPSES);
    return &cgs->actorCorpseInfo[corpseNum];
}

static XAnimTree_s *CG_RebuildActorTree_SP(int localClientNum, centity_s *cent)
{
    cg_s *cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    cgs_t *cgs = CG_GetLocalClientStaticGlobals(localClientNum);
    XAnim_s *anims = cent->nextState.animTreeIndex
        ? CG_GetActorRequestedAnims_SP(localClientNum, cent)
        : cgameGlob->bgs.animData->generic_dog.tree.anims;
    XAnimTree_s *tree = anims ? XAnimCreateTree(anims, Hunk_AllocXAnimClient) : NULL;
    if (cent->nextState.eType == ET_ACTOR)
    {
        const int actorNum = cent->nextState.lerp.u.actor.actorNum;
        iassert(actorNum >= 0 && actorNum < MAX_ACTORS);
        XAnimFreeTree(cgameGlob->bgs.actorinfo[actorNum].pXAnimTree, NULL, SCRIPTINSTANCE_SERVER);
        const int currentActorNum = cent->nextState.lerp.u.actor.actorNum;
        iassert(currentActorNum >= 0 && currentActorNum < MAX_ACTORS);
        cgameGlob->bgs.actorinfo[currentActorNum].pXAnimTree = tree;
    }
    else
    {
        const int corpseNum = cent->nextState.lerp.u.actor.actorNum;
        iassert(corpseNum >= 0 && corpseNum < MAX_ACTOR_CORPSES);
        XAnimFreeTree(cgs->actorCorpseInfo[corpseNum].pXAnimTree, NULL, SCRIPTINSTANCE_SERVER);
        const int currentCorpseNum = cent->nextState.lerp.u.actor.actorNum;
        iassert(currentCorpseNum >= 0 && currentCorpseNum < MAX_ACTOR_CORPSES);
        cgs->actorCorpseInfo[currentCorpseNum].pXAnimTree = tree;
    }
    return tree;
}

static XAnimTree_s *CG_GetActorTree_SP(int localClientNum, centity_s *cent)
{
    cg_s *cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    cgs_t *cgs = cent->nextState.eType == ET_ACTOR ? NULL
        : CG_GetLocalClientStaticGlobals(localClientNum);
    XAnimTree_s *tree = CG_GetActorTreeOwner_SP(cgameGlob, cgs, cent)->pXAnimTree;
    XAnim_s *anims = cgameGlob->bgs.animData->generic_dog.tree.anims;
    if (cent->nextState.animTreeIndex)
        anims = CG_GetActorRequestedAnims_SP(localClientNum, cent);
    if (XAnimGetAnims(tree) != anims)
        return CG_RebuildActorTree_SP(localClientNum, cent);
    return tree;
}

void __cdecl CG_UpdateActorDObj_SP(int localClientNum, centity_s *cent)
{
    if (!(cent->clientFlags & 2))
        return;
    const int actorNum = cent->nextState.lerp.u.actor.actorNum;
    iassert(actorNum >= 0 && actorNum < MAX_ACTORS);
    actorInfo_t *info = &CG_GetLocalClientGlobals(localClientNum)->bgs.actorinfo[actorNum];
    if (!info->nextValid)
        return;
    DObj *obj = Com_GetClientDObj(cent->nextState.number, localClientNum);
    const unsigned int weapon = static_cast<unsigned char>(cent->nextState.weapon);
    XAnimTree_s *tree = CG_GetActorTree_SP(localClientNum, cent);
    if (tree && (!info->infoValid || !info->model[0]))
    {
        CG_SafeDObjFree(localClientNum, cent->nextState.number);
        return;
    }
    if (obj)
    {
        if (info->dobjWeapon == weapon && !info->dobjDirty)
        {
            if (tree == DObjGetTree(obj))
                return;
            tree = info->pXAnimTree;
        }
        CG_SafeDObjFree(localClientNum, cent->nextState.number);
    }
    DObjModel_s models[7];
    models[0].model = R_RegisterModel(info->model);
    models[0].boneName = 0;
    models[0].ignoreCollision = 0;
    iassert(models[0].model);
    unsigned int count = 1;
    for (unsigned int i = 0; i < 6 && info->attachModelNames[i][0]; ++i)
    {
        models[count].model = R_RegisterModel(info->attachModelNames[i]);
        models[count].boneName = SL_FindString(info->attachTagNames[i], SCRIPTINSTANCE_SERVER);
        models[count].ignoreCollision = (info->attachIgnoreCollision & (1u << i)) != 0;
        ++count;
    }
    info->dobjWeapon = weapon;
    Com_ClientDObjCreate(models, count, tree, cent->nextState.number, localClientNum);
    if (cent->pose.isRagdoll && cent->pose.ragdollHandle > 0)
        Ragdoll_RebindBody(cent->pose.ragdollHandle);
    info->dobjDirty = 0;
    info->dobjCreated = 1;
}

static void CG_ProcessActorSnapshot_SP(int localClientNum, centity_s *cent)
{
    XAnimTree_s *tree = CG_GetActorTree_SP(localClientNum, cent);
    if (!tree)
        return;
    const int actorNum = cent->nextState.lerp.u.actor.actorNum;
    iassert(actorNum >= 0 && actorNum < MAX_ACTORS);
    cg_s *cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    actorInfo_t *info = &cgameGlob->bgs.actorinfo[actorNum];
    cgs_t *cgs = CG_GetLocalClientStaticGlobals(localClientNum);
    if (cgs->localServer && (ai_useServerAnims->current.enabled
        || ai_useServerAnimsEntity->current.integer == cent->nextState.number))
    {
        iassert(cent->nextState.number >= 0 && cent->nextState.number < MAX_GENTITIES);
        XAnimTree_s *serverTree = G_GetAnimTreeForEntity_SP(cent->nextState.number, cent->nextState.eType);
        DObj *obj = Com_GetClientDObj(cent->nextState.number, localClientNum);
        if (serverTree)
        {
            XAnim_s *clientAnims = XAnimGetAnims(tree);
            XAnim_s *serverAnims = XAnimGetAnims(serverTree);
            if (clientAnims != serverAnims)
            {
                XAnimFreeTree(tree, NULL, SCRIPTINSTANCE_SERVER);
                tree = serverAnims ? XAnimCreateTree(serverAnims, Hunk_AllocXAnimClient) : NULL;
                const int currentActorNum = cent->nextState.lerp.u.actor.actorNum;
                iassert(currentActorNum >= 0 && currentActorNum < MAX_ACTORS);
                cgameGlob->bgs.actorinfo[currentActorNum].pXAnimTree = tree;
            }
            if (tree)
            {
                DObjSetTree(obj, tree);
                XAnimCloneAnimTree(serverTree, obj, 0.0f);
            }
            return;
        }
    }
    if (info->dobjCreated)
        Com_GetClientDObj(cent->nextState.number, localClientNum);
}
#endif

void __cdecl CG_ActorProcessSnapshot(int localClientNum, centity_s *cent)
{
#ifdef KISAK_SP
    CG_ProcessActorSnapshot_SP(localClientNum, cent);
#else
    bool v2; // [esp+3h] [ebp-5h]
    cg_s *cgameGlob; // [esp+4h] [ebp-4h]

    cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    CG_UpdateActorDObj(localClientNum, cent, &cgameGlob->bgs.actorinfo[cent->nextState.lerp.u.actor.actorNum]);
    if ( CL_LocalClient_IsFirstActive(localClientNum) )
    {
        if ( cent )
            v2 = ((cent->clientFlags >> 8) & 1) != 0;
        else
            v2 = 0;
    }
    else
    {
        v2 = 0;
    }
    if ( v2 && cent->currentState.u.turret.ownerNum != cent->nextState.lerp.u.turret.ownerNum )
        CScr_NotifyNum(localClientNum, cent->nextState.number, 0, cscr_const.enemy, 0);
#endif
}

#ifdef KISAK_NX
// cgameGlob->time each entity last created its actor DObj, for the report below.
static int s_nxActorDObjCreatedAt[1024];

// One entity, client side and (when the server is local) server side, for the
// "both live on one actor slot" report.
static void CG_NxDescribeActorEnt(int localClientNum, int number, char *out, size_t size)
{
    const centity_s *cent = CG_GetEntity(localClientNum, number);
    int len = snprintf(out, size, "ent %d: client eType %d slot %u eFlags 0x%x inSnap %d dobjAt %d",
                       number, cent->nextState.eType, cent->nextState.lerp.u.actor.actorNum,
                       cent->nextState.lerp.eFlags, (cent->clientFlags & 2) != 0,
                       number >= 0 && number < 1024 ? s_nxActorDObjCreatedAt[number] : -1);
    if ( len < 0 || (size_t)len >= size )
        return;
    if ( !com_sv_running || !com_sv_running->current.enabled || number < 0 || number >= MAX_GENTITIES )
    {
        snprintf(out + len, size - len, "; server not local");
        return;
    }
    const gentity_s *ent = &g_entities[number];
    const actor_s *actor = ent->actor;
    len += snprintf(out + len, size - len, "; server inuse %d eType %d slot %u health %d actor %s",
                    ent->r.inuse, ent->s.eType, ent->s.lerp.u.actor.actorNum, ent->health,
                    !actor ? "none" : !actor->inuse ? "freed" : "live");
    if ( actor && actor->inuse && len > 0 && (size_t)len < size )
        snprintf(out + len, size - len, " (its ent %d)", actor->ent ? actor->ent->s.number : -1);
}
#endif

void __cdecl CG_UpdateActorDObj(int localClientNum, centity_s *cent, actorInfo_t *ai)
{
#ifdef KISAK_SP
    if (cent->nextState.eType == ET_ACTOR)
    {
        CG_UpdateActorDObj_SP(localClientNum, cent);
        return;
    }
#endif
    XAnimTree_s *Tree; // eax
    DObj *v4; // eax
    float *v5; // [esp+0h] [ebp-230h]
    XModel *xmodel; // [esp+4h] [ebp-22Ch]
    cg_s *cgameGlob; // [esp+Ch] [ebp-224h]
    DObj *pDObj; // [esp+10h] [ebp-220h]
    int model; // [esp+14h] [ebp-21Ch]
    entityState_s *p_nextState; // [esp+18h] [ebp-218h]
    const cgs_t *cgs; // [esp+1Ch] [ebp-214h]
    FxMarkDObjUpdateContext markUpdateContext; // [esp+20h] [ebp-210h] BYREF
    int objExists; // [esp+128h] [ebp-108h]
    XAnimTree_s *pAnimTree; // [esp+12Ch] [ebp-104h]
    DObjModel_s dobjModels[32]; // [esp+130h] [ebp-100h] BYREF


    if ( ((cent->clientFlags >> 1) & 1) != 0 )
    {
        p_nextState = &cent->nextState;
        cgameGlob = CG_GetLocalClientGlobals(localClientNum);
        if ( cent->nextState.lerp.u.actor.actorNum >= MAX_ACTORS
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_actors_mp.cpp",
                        123,
                        0,
                        "es->lerp.u.actor.actorNum doesn't index MAX_ACTORS\n\t%i not in [0, %i)",
                        cent->nextState.lerp.u.actor.actorNum,
                        MAX_ACTORS) )
        {
            __debugbreak();
        }
        cgs = CG_GetLocalClientStaticGlobals(localClientNum);
#ifdef KISAK_SP
        // Retail's actor DObj path consumes the same entityState+0xD0
        // animtree index as script movers, but owns its tree in actorInfo.
        // Recreate that independent client tree before comparing/creating the
        // DObj when useAnimTree selected a different server animation set.
        const unsigned int treeIndex = p_nextState->animTreeIndex;
        XAnim_s *desiredAnims = !com_sv_running->current.enabled
                              ? CG_GetRemoteAnimations_SP(localClientNum, p_nextState->number)
                              : treeIndex
                              ? CG_ResolvePublishedAnims_SP(treeIndex, false)
                              : BG_GetActorAnims();
        if ( desiredAnims && (!ai->pXAnimTree || XAnimGetAnims(ai->pXAnimTree) != desiredAnims) )
        {
            if ( ai->pXAnimTree )
                XAnimFreeTree(ai->pXAnimTree, 0, SCRIPTINSTANCE_SERVER);
            ai->pXAnimTree = XAnimCreateTree(desiredAnims, Hunk_AllocXAnimClient);
            ai->dobjDirty = 1;
            Com_Printf(
                15,
                "SP actor animtree resolve: client %d ent %d actor %u index %u tree %p anims %p %s\n",
                localClientNum,
                p_nextState->number,
                p_nextState->lerp.u.actor.actorNum,
                treeIndex,
                ai->pXAnimTree,
                desiredAnims,
                desiredAnims->debugName ? desiredAnims->debugName : "<none>");
        }
#endif
        pDObj = Com_GetClientDObj(p_nextState->number, localClientNum);
        FX_MarkEntUpdateBegin(&markUpdateContext, pDObj, 0, 0);
        objExists = pDObj != 0;
        pAnimTree = ai->pXAnimTree;
        model = CG_WhatModelShouldLocalPlayerSee(
                            localClientNum,
                            cgameGlob,
                            cent,
#ifdef KISAK_SP
                            static_cast<team_t>(ai->team),
#else
                            cent->nextState.lerp.u.actor.team,
#endif
                            cent->nextState.index.brushmodel,
                            cent->nextState.enemyModel);
        if ( pAnimTree && !model )
        {
            XAnimClearTree(pAnimTree);
            if ( objExists )
                CG_SafeDObjFree(localClientNum, p_nextState->number);
            return;
        }
        if ( objExists )
        {
            xmodel = DObjGetModel(pDObj, 0);
            if ( !ai->dobjDirty )
            {
                Tree = DObjGetTree(pDObj);
                if ( pAnimTree == Tree && cgs->gameModels[model] == xmodel
                    )
                    return;
            }
            CG_SafeDObjFree(localClientNum, p_nextState->number);
        }
        dobjModels[0].model = cgs->gameModels[model];
        if ( !dobjModels[0].model
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_actors_mp.cpp",
                        167,
                        0,
                        "%s",
                        "dobjModels[iNumModels].model") )
        {
            __debugbreak();
        }
        dobjModels[0].boneName = 0;
        dobjModels[0].ignoreCollision = 0;
        if ( pAnimTree )
        {
            unsigned int numModels = 1;
#ifdef KISAK_SP
            for ( int i = 0; i < 6 && ai->attachModelNames[i][0]; ++i )
            {
                dobjModels[numModels].model = R_RegisterModel(ai->attachModelNames[i]);
                dobjModels[numModels].boneName = SL_FindString(ai->attachTagNames[i], SCRIPTINSTANCE_SERVER);
                dobjModels[numModels].ignoreCollision = (ai->attachIgnoreCollision & (1u << i)) != 0;
                ++numModels;
            }
#endif
#ifdef KISAK_NX
            // One live DObj per actor tree. CG_UpdateEntInfo advances the tree
            // of every DObj in the snapshot, so a second DObj on the same tree
            // -- an old occupant of this actor slot that was never freed --
            // played the new zombie's animations at double speed.
            {
                static const XAnimTree_s *s_treeOwnerTree[64];
                static int s_treeOwnerEnt[64];
                int slot = -1, freeSlot = -1;
                for ( int i = 0; i < 64; ++i )
                {
                    if ( s_treeOwnerTree[i] == pAnimTree )
                        slot = i;
                    else if ( !s_treeOwnerTree[i] && freeSlot < 0 )
                        freeSlot = i;
                }
                if ( slot >= 0 && s_treeOwnerEnt[slot] != p_nextState->number )
                {
                    const int previous = s_treeOwnerEnt[slot];
                    const DObj *previousObj = Com_GetClientDObj(previous, localClientNum);
                    // Both entities in this snapshot as actors on the same slot:
                    // freeing the other only makes it recreate its DObj next
                    // frame and free ours, every frame, and anything that reads
                    // its DObj in between (aim assist) finds none. Leave it.
                    const centity_s *previousCent = CG_GetEntity(localClientNum, previous);
                    const bool previousLive = (previousCent->clientFlags & 2) != 0   // in this snapshot
                        && previousCent->nextState.eType == p_nextState->eType
                        && previousCent->nextState.lerp.u.actor.actorNum == p_nextState->lerp.u.actor.actorNum;
                    if ( previousObj && DObjGetTree(previousObj) == pAnimTree && previousLive )
                    {
                        static int s_reportedLive;
                        if ( s_reportedLive++ < 5 )
                        {
                            char previousDesc[320], currentDesc[320];
                            CG_NxDescribeActorEnt(localClientNum, previous, previousDesc, sizeof(previousDesc));
                            CG_NxDescribeActorEnt(localClientNum, p_nextState->number, currentDesc, sizeof(currentDesc));
                            Com_Printf(15, "[nx-anim] ents %d and %d are both live on actor slot %u; tree shared (time %d)\n",
                                       previous, p_nextState->number, p_nextState->lerp.u.actor.actorNum,
                                       cgameGlob->time);
                            Com_Printf(15, "[nx-anim]   %s\n", previousDesc);
                            Com_Printf(15, "[nx-anim]   %s\n", currentDesc);
                        }
                    }
                    else if ( previousObj && DObjGetTree(previousObj) == pAnimTree )
                    {
                        static int s_reported;
                        if ( s_reported++ < 20 )
                            Com_Printf(15, "[nx-anim] ent %d still had a DObj on the anim tree ent %d now uses; freed\n",
                                       previous, p_nextState->number);
                        CG_SafeDObjFree(localClientNum, previous);
                    }
                }
                if ( slot < 0 )
                    slot = freeSlot >= 0 ? freeSlot : 0;
                s_treeOwnerTree[slot] = pAnimTree;
                s_treeOwnerEnt[slot] = p_nextState->number;
                if ( p_nextState->number >= 0 && p_nextState->number < 1024 )
                    s_nxActorDObjCreatedAt[p_nextState->number] = cgameGlob->time;
            }
#endif
            v4 = Com_ClientDObjCreate(dobjModels, numModels, pAnimTree, p_nextState->number, localClientNum);
#ifdef KISAK_SP
            if (!com_sv_running->current.enabled)
                CG_ApplyRemoteAnimSnapshot_SP(localClientNum, p_nextState->number, v4, true);
#endif
            ai->dobjDirty = 0;
            v5 = cg_entityOriginArray[localClientNum][p_nextState->number];
            *v5 = 131072.0f;
            v5[1] = 131072.0f;
            v5[2] = 131072.0f;
            FX_MarkEntUpdateEnd(&markUpdateContext, localClientNum, p_nextState->number, v4, 0, 0);
        }
    }
}

//cgs_t *__cdecl CG_GetLocalClientStaticGlobals(int localClientNum)
//{
//    if ( localClientNum
//        && !Assert_MyHandler(
//                    "c:\\projects_pc\\cod\\codsrc\\src\\cgame\\../cgame_mp/cg_local_mp.h",
//                    1843,
//                    0,
//                    "%s\n\t(localClientNum) = %i",
//                    "(localClientNum == 0)",
//                    localClientNum) )
//    {
//        __debugbreak();
//    }
//    return cgsArray;
//}

#ifdef KISAK_SP
void __cdecl CG_ResetActorEntity(int localClientNum, cg_s *cgameGlob, centity_s *cent)
{
    if ( cent->nextState.lerp.u.actor.actorNum >= MAX_ACTORS
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_actors_mp.cpp",
                    201,
                    0,
                    "es->lerp.u.actor.actorNum doesn't index MAX_ACTORS\n\t%i not in [0, %i)",
                    cent->nextState.lerp.u.actor.actorNum,
                    MAX_ACTORS) )
    {
        __debugbreak();
    }
    actorInfo_t *ai = &cgameGlob->bgs.actorinfo[cent->nextState.lerp.u.actor.actorNum];
    XAnimTree_s *pAnimTree = ai->pXAnimTree;
    if ( !pAnimTree
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_actors_mp.cpp", 209, 0, "%s", "pAnimTree") )
    {
        __debugbreak();
    }
    XAnimClearTreeGoalWeights(pAnimTree, 0, 0.0f, -1);
    CG_UpdateActorDObj_SP(localClientNum, cent);
    cent->animCommandTime = 0;
    ai->cachedLookAtEntNum = ENTITYNUM_NONE;
    CG_InitializeEntityAnimCommandTime_SP(localClientNum, cent->nextState.number);
}
#else
void __cdecl CG_ResetActorEntity(int localClientNum, cg_s *cgameGlob, centity_s *cent)
{
    actorInfo_t *ai; // [esp+8h] [ebp-10h]
    XAnimTree_s *pAnimTree; // [esp+14h] [ebp-4h]

    if ( cent->nextState.lerp.u.actor.actorNum >= MAX_ACTORS
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_actors_mp.cpp",
                    201,
                    0,
                    "es->lerp.u.actor.actorNum doesn't index MAX_ACTORS\n\t%i not in [0, %i)",
                    cent->nextState.lerp.u.actor.actorNum,
                    MAX_ACTORS) )
    {
        __debugbreak();
    }
    ai = &cgameGlob->bgs.actorinfo[cent->nextState.lerp.u.actor.actorNum];
    CG_GetLocalClientStaticGlobals(localClientNum);
    pAnimTree = ai->pXAnimTree;
    ai->dobjDirty = 1;
    if ( !pAnimTree
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_actors_mp.cpp", 209, 0, "%s", "pAnimTree") )
    {
        __debugbreak();
    }
    XAnimClearTreeGoalWeights(pAnimTree, 0, 0.0, -1);
    CG_UpdateActorDObj(localClientNum, cent, ai);
#ifdef KISAK_SP
    CG_ApplyPendingAnimCommandsForDObj_SP(
        localClientNum,
        cent->nextState.number,
        Com_GetClientDObj(cent->nextState.number, localClientNum));
#endif
}
#endif

#ifdef KISAK_SP
static actorInfo_t *__cdecl CG_GetActorInfoForControllers_SP(int localClientNum, int entityNum)
{
    centity_s *cent = CG_GetEntity(localClientNum, entityNum);
    if (cent->nextState.eType != ET_ACTOR)
        return nullptr;
    bgs_t *context = bgs ? bgs : &CG_GetLocalClientGlobals(localClientNum)->bgs;
    return &context->actorinfo[cent->nextState.lerp.u.actor.actorNum];
}

int __cdecl CG_GetActorScriptedAnim_SP(int localClientNum, int entityNum)
{
    actorInfo_t *info = CG_GetActorInfoForControllers_SP(localClientNum, entityNum);
    if (!info)
        return 0;
    if (info->animScriptedAnimTime <= CG_GetLocalClientGlobals(0)->time)
        info->animScriptedAnim = info->nextAnimScriptedAnim;
    return info->animScriptedAnim;
}

static bool __cdecl CG_Actor_UsesBodyPitch_SP(const centity_s *cent)
{
    return zombiemode->current.enabled && cent->nextState.eType == ET_ACTOR
        && cent->nextState.lerp.u.actor.species == 3;
}

static float CG_ActorLookAtFraction_SP(float difference)
{
    volatile float revolutions = difference * 0.002777777845069766f;
    volatile float shifted = revolutions + 0.5f;
    const float signedMagic = std::copysign(8388608.0f, shifted);
    volatile float roundedSum = shifted + signedMagic;
    volatile float rounded = roundedSum - signedMagic;
    volatile float error = rounded - shifted;
    if (!(error <= std::copysign(0.0f, shifted)))
        rounded -= 1.0f;
    return revolutions - rounded;
}

static float CG_ActorLookAtBlend_SP(float weight)
{
    for (int i = 0; i != 2; ++i)
    {
        volatile float angle = weight * 3.1415927410125732f;
        angle -= 1.5707963705062866f;
        volatile float value = static_cast<float>(std::sin(static_cast<double>(angle)));
        volatile float shifted = value + 1.0f;
        weight = shifted * 0.5f;
    }
    return weight;
}

static int CG_ActorLookAtTimeout_SP(int time)
{
    const unsigned int timeBits = static_cast<unsigned int>(time) + 2000u;
    int baseTime;
    std::memcpy(&baseTime, &timeBits, sizeof(baseTime));
    const double timeout = random() * 1000.0 + static_cast<double>(baseTime);
    if (!(timeout >= -2147483648.0 && timeout < 2147483648.0))
        return (-2147483647 - 1);
    return static_cast<int>(timeout);
}

static void __cdecl CG_Actor_UpdateLookAt_SP(int localClientNum, centity_s *cent)
{
    cg_s *cg = CG_GetLocalClientGlobals(localClientNum);
    const int entityNum = cent->nextState.number;
    actorInfo_t *info = CG_GetActorInfoForControllers_SP(localClientNum, entityNum);
    if (!info)
        return;
    const int time = cg->time;
    DObj *obj = Com_GetClientDObj(entityNum, localClientNum);
    if (!obj)
        return;
    unsigned char headBone = 0;
    DObjGetBoneIndex(obj, scr_const.j_head, &headBone, -1);
    cent->pose.actor.headBone = headBone;
    if (!cg_allowActorLookAtEntity->current.enabled || !info->infoValid)
        return;

    if (info->lookAtEntNum != ENTITYNUM_NONE)
    {
        info->cachedLookAtEntNum = info->lookAtEntNum;
        if (!info->lookAtActive)
        {
            info->lookAtActive = 1;
            info->lookAtTimeout = CG_ActorLookAtTimeout_SP(time);
        }
    }
    else
    {
        info->lookAtActive = 0;
        if (info->lookAtWeight == 0.0f)
        {
            info->cachedLookAtEntNum = ENTITYNUM_NONE;
            info->lookAtAngles[0] = 0.0f;
            info->lookAtAngles[1] = 0.0f;
            info->lookAtAngles[2] = 0.0f;
        }
    }

    if (info->cachedLookAtEntNum == ENTITYNUM_NONE)
    {
        if (!CG_Actor_UsesBodyPitch_SP(cent) && !cent->allowPitch)
            cent->pose.actor.pitch = 0.0f;
        cent->pose.actor.height = 0.0f;
        cent->pose.actor.roll = 0.0f;
        return;
    }

    cent->pose.actor.proneType = 0;
    centity_s *target = CG_GetEntity(localClientNum, info->cachedLookAtEntNum);
    volatile float elapsed = static_cast<float>(cg->frametime) * 0.0010000000474974513f;
    volatile float step = elapsed * 0.5f;
    if (info->lookAtActive)
    {
        info->lookAtWeight += step;
        if (info->lookAtWeight > 1.0f)
            info->lookAtWeight = 1.0f;
    }
    else
    {
        info->lookAtWeight -= step;
        if (0.0f > info->lookAtWeight)
            info->lookAtWeight = 0.0f;
    }

    float headAxis[3][3], headOrigin[3];
    CG_DObjGetWorldBoneMatrix(&cent->pose, obj, headBone, headAxis, headOrigin);
    const unsigned char parentBone = DObjGetParentBoneIndex(obj, headBone);
    float parentAxis[3][3], parentOrigin[3];
    CG_DObjGetWorldBoneMatrix(&cent->pose, obj, parentBone, parentAxis, parentOrigin);
    DObjClearSkel(obj);

    float direction[3];
    const float *origin = cent->nextState.lerp.pos.trBase;
    const float *targetOrigin = target->nextState.lerp.pos.trBase;
    if (target->nextState.number == cg->clientNum)
    {
        origin = headOrigin;
        targetOrigin = cg->refdef.vieworg;
    }
    for (int i = 0; i != 3; ++i)
        direction[i] = targetOrigin[i] - origin[i];
    float angles[3];
    vectoangles(direction, angles);
    for (int i = 0; i != 3; ++i)
        angles[i] = static_cast<float>(AngleDelta(angles[i], cent->pose.angles[i]));

    const float limits[3] = {60.0f, 120.0f, 0.0f};
    for (int i = 0; i != 3; ++i)
    {
        float high = limits[i];
        if (!(angles[i] - high >= 0.0f))
            high = angles[i];
        float low = -limits[i];
        if (!(low - angles[i] >= 0.0f))
            low = high;
        angles[i] = low;
    }

    volatile float inverseDuration = 1.0f / 0.4000000059604645f;
    for (int i = 0; i != 3; ++i)
    {
        const float previous = info->lookAtAngles[i];
        float difference = angles[i];
        if (std::fabs(difference) != std::fabs(limits[i])
            || ((previous >= 0.0f) == (difference >= 0.0f)))
            difference -= previous;
        volatile float fraction = CG_ActorLookAtFraction_SP(difference);
        volatile float elapsedSeconds = static_cast<float>(cg->frametime) * 0.0010000000474974513f;
        volatile float scale = elapsedSeconds * inverseDuration;
        volatile float change = fraction * scale;
        change *= 360.0f;
        info->lookAtAngles[i] = change + previous;
    }
    cent->pose.actor.yaw = CG_ActorLookAtBlend_SP(info->lookAtWeight);
    cent->pose.actor.pitch = info->lookAtAngles[0];
    cent->pose.actor.height = info->lookAtAngles[1];
    cent->pose.actor.roll = info->lookAtAngles[2];
}

#endif
void __cdecl CG_Actor_PreControllers(int localClientNum, centity_s *cent)
{
#ifdef KISAK_SP
    if (!CG_Actor_UsesBodyPitch_SP(cent) && !cent->allowPitch)
        cent->pose.actor.pitch = 0.0f;
    cent->pose.actor.height = 0.0f;
    cent->pose.actor.roll = 0.0f;
    CG_Actor_UpdateLookAt_SP(localClientNum, cent);
#else
    cent->pose.player.nextWaterHeightCheck = 0;
    cent->pose.turret.barrelPitch = 0.0f;
    cent->pose.fx.triggerTime = 1;
    cent->pose.player.nextWaterHeightCheck = 0;
#endif
}

#ifdef KISAK_SP
static float CG_ActorBurnFraction_SP(int localClientNum, centity_s *cent)
{
    if ((cent->currentState.eFlags2 & 0x200000u) == 0
        && (cent->clientFlags & 0x20u) == 0)
    {
        cent->pose.startBurnTime = 0;
        return 0.0f;
    }
    if (zombietron->current.enabled)
        return 0.0f;
    if (!cent->pose.startBurnTime)
        cent->pose.startBurnTime = CG_GetLocalClientGlobals(localClientNum)->time;
    const unsigned int elapsedBits = static_cast<unsigned int>(CG_GetLocalClientGlobals(localClientNum)->time)
        - static_cast<unsigned int>(cent->pose.startBurnTime);
    int elapsed;
    std::memcpy(&elapsed, &elapsedBits, sizeof(elapsed));
    volatile float duration = cg_actorBurnTransitionTime->current.value * 1000.0f;
    volatile float amount = static_cast<float>(elapsed) / duration;
    volatile float upperDifference = amount - 1.0f;
    float upper = 1.0f;
    if (!(upperDifference >= 0.0f))
        upper = amount;
    volatile float lowerDifference = 0.0f - amount;
    return lowerDifference >= 0.0f ? 0.0f : upper;
}

static bool CG_Actor_RenderPrefix_SP(centity_s *cent)
{
    if ((cent->nextState.lerp.eFlags & 0x20) != 0 || cent->actorRenderError_SP)
        return false;
    if ((g_skinErrorFlags.allbits & 3u) != 0 && cent->nextState.eType == ET_ACTOR_CORPSE)
    {
        cent->actorRenderError_SP = 1;
        return false;
    }
    return true;
}

static bool CG_Actor_RunCallbacks_SP(int localClientNum, centity_s *cent)
{
    if (CG_EntityNeedsScriptThread(localClientNum, cent))
    {
        cent->clientFlags |= 0x100u;
        Scr_AddInt(localClientNum, SCRIPTINSTANCE_CLIENT);
        const unsigned short thread = CScr_ExecEntThread(cent, cg_scr_data.entityspawned, 1u);
        Scr_FreeThread(thread, SCRIPTINSTANCE_CLIENT);
    }
    const int species = cent->nextState.lerp.u.actor.species;
    if (zombiemode->current.enabled && species != AI_SPECIES_DOG && species != AI_SPECIES_ZOMBIE_DOG)
    {
        const bool oldEyeFlag = (cent->clientFlags & 0x1000000u) != 0;
        const bool eyeFlag = (cent->nextState.lerp.eFlags & 0x20000u) != 0;
        if (oldEyeFlag != eyeFlag && cg_scr_sp_data.zombieEyeCallback)
        {
            cent->clientFlags = (cent->clientFlags & ~0x1000000u) | (eyeFlag ? 0x1000000u : 0u);
            Scr_AddInt(eyeFlag, SCRIPTINSTANCE_CLIENT);
            Scr_AddInt(localClientNum, SCRIPTINSTANCE_CLIENT);
            const unsigned short thread = CScr_ExecEntThread(cent, cg_scr_sp_data.zombieEyeCallback, 2u);
            Scr_FreeThread(thread, SCRIPTINSTANCE_CLIENT);
        }
    }
    const cg_s *cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    return !cent->animCommandTime || cent->animCommandTime <= cgameGlob->time;
}

static unsigned int CG_ActorRenderFlags_SP(const entityState_s *state);

#endif
void __cdecl CG_Actor(int localClientNum, centity_s *cent)
{
    unsigned int renderFxFlags; // [esp+2Ch] [ebp-5Ch]
    unsigned __int16 t; // [esp+30h] [ebp-58h]
    const DObj *obj; // [esp+34h] [ebp-54h]
    entityState_s *s1; // [esp+38h] [ebp-50h]
    float mins[3]; // [esp+40h] [ebp-48h] BYREF
    const cgs_t *cgs; // [esp+4Ch] [ebp-3Ch]
    float bounds[2][3]; // [esp+50h] [ebp-38h] BYREF
    float maxs[3]; // [esp+68h] [ebp-20h] BYREF
    float lightingOrigin[3]; // [esp+74h] [ebp-14h] BYREF
#ifndef KISAK_SP
    actorInfo_t *actorInfo; // [esp+84h] [ebp-4h]
#endif

    s1 = &cent->nextState;
#ifdef KISAK_SP
    if (CG_Actor_RenderPrefix_SP(cent))
#else
    if ( (cent->nextState.lerp.eFlags & 0x20) == 0 )
#endif
    {
        cgs = CG_GetLocalClientStaticGlobals(localClientNum);
        obj = Com_GetClientDObj(s1->number, localClientNum);
        if ( obj )
        {
#ifdef KISAK_SP
            if (!CG_Actor_RunCallbacks_SP(localClientNum, cent))
                return;
#else
            if ( CG_EntityNeedsScriptThread(localClientNum, cent) )
            {
                cent->clientFlags |= 0x100u;
                Scr_AddInt(localClientNum, SCRIPTINSTANCE_CLIENT);
                t = CScr_ExecEntThread(cent, cg_scr_data.entityspawned, 1u);
                Scr_FreeThread(t, SCRIPTINSTANCE_CLIENT);
            }
#endif
#ifndef KISAK_SP
            actorInfo = &CG_GetLocalClientGlobals(localClientNum)->bgs.actorinfo[cent->nextState.lerp.u.actor.actorNum];
            BG_Dog_UpdateAnimationState(localClientNum, &cent->nextState, actorInfo);
#endif
            CG_Actor_PreControllers(localClientNum, cent);
            lightingOrigin[0] = cent->pose.origin[0];
            lightingOrigin[1] = cent->pose.origin[1];
            lightingOrigin[2] = cent->pose.origin[2] + 32.0;
            if (cent->pose.isRagdoll && cent->pose.ragdollHandle > 0)
            {
                Ragdoll_GetRootOrigin(cent->pose.ragdollHandle, cent->pose.origin);
            }
#ifdef KISAK_SP
            R_FoliageNotifyActorPosition(localClientNum, cent->pose.origin);
#endif
#ifdef KISAK_SP
            if (ai_water_trails->current.enabled)
                CG_DrawWaterTrail(localClientNum, cent, (s1->lerp.eFlags & 0x400000u) == 0);
            if (s1->eType == ET_ACTOR)
                CG_DoFootsteps(localClientNum, cent);
#else
            CG_DoFootsteps(localClientNum, cent);
#endif
#ifdef KISAK_SP
            const float burnFraction = CG_ActorBurnFraction_SP(localClientNum, cent);
#endif
#ifndef KISAK_SP
            CG_GetEntityDobjBounds(cent, obj, mins, maxs, bounds[0], bounds[1]);
            const bool actorCulled = R_CullBoxCurDpvs(bounds[0], localClientNum);
            if ( !actorCulled )
#endif
            {
                CG_HighlightPlayer(localClientNum, cent, &cent->pose.constantSet, 0);
#ifdef KISAK_SP
                renderFxFlags = CG_ActorRenderFlags_SP(s1);
#else
                renderFxFlags = 4194308;
#endif
                if ( CG_IsInfrared(localClientNum) )
#ifdef KISAK_SP
                    renderFxFlags |= 0x80u;
#else
                    renderFxFlags = 4194436;
#endif
#ifdef KISAK_SP
                const ShaderConstantSet *actorConstants = R_ShaderConstantSetIsUsed(&cent->pose.constantSet)
                    ? &cent->pose.constantSet : nullptr;
#endif
                R_AddDObjToScene(
                    obj,
                    &cent->pose,
                    s1->number,
                    renderFxFlags,
                    lightingOrigin,
                    0.0,
                    0.0,
#ifdef KISAK_SP
                    burnFraction,
#else
                    0.0,
#endif
#ifdef KISAK_SP
                    cent->pose.wetness,
#else
                    0.0,
#endif
                    -1,
                    -1,
#ifdef KISAK_SP
                    actorConstants,
#else
                    &cent->pose.constantSet,
#endif
                    0,
                    0.0,
                    1.0);
            }
#ifdef KISAK_SP
            if (cent->nextState.eType == ET_ACTOR
                && !CG_IsRagdollTrajectory(&cent->nextState.lerp.pos))
                CG_CompassUpdateActorInfo_SP(localClientNum, cent->nextState.number);
#endif
            if ( cent->nextState.eType == 17 )
            {
                if ( CL_LocalClient_IsFirstActive(localClientNum) )
                    CG_DoTouchTriggers(cent, localClientNum);
            }
        }
    }
}

bool __cdecl CG_EntityNeedsScriptThread(int localClientNum, centity_s *cent)
{
#ifdef KISAK_SP
    if (!cg_loadScripts || !cg_loadScripts->current.enabled)
        return false;
    if (!CL_LocalClient_IsFirstActive(localClientNum))
        return false;
#else
    if ( !cg_loadScripts || !cg_loadScripts->current.enabled )
        return 0;
    if ( !CL_LocalClient_IsFirstActive(localClientNum) )
        return 0;
#endif
    if ( cent )
        return ((cent->clientFlags >> 8) & 1) == 0;
    return 0;
}

#if defined(KISAK_NX) && !defined(KISAK_SP)
extern const dvar_t *nx_physics;   // phys_main.cpp
#endif

#ifdef KISAK_SP
static unsigned int CG_ActorRenderFlags_SP(const entityState_s *state)
{
    unsigned int flags = CG_GetShadowHintForRefEntity(static_cast<short>(state->lerp.eFlags));
    if ((state->lerp.eFlags & 0x400u) != 0)
        flags |= 0x10u;
    if ((state->lerp.eFlags2 & 0x4000000u) != 0)
        flags |= 0x200000u;
    return flags | 0x400004u;
}

static int CG_ActorTimeDifference_SP(int time, int previous)
{
    const unsigned int bits = static_cast<unsigned int>(time) - static_cast<unsigned int>(previous);
    int difference;
    std::memcpy(&difference, &bits, sizeof(difference));
    return difference;
}

static bool CG_HighlightActor_SP(int localClientNum, centity_s *cent)
{
    cg_s *cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    if ((cent->clientFlags & 2u) == 0)
        return false;
    if (!CG_IsInfrared(localClientNum))
        return true;

    ShaderConstantSet *constantSet = &cent->pose.constantSet;
    R_UnmapShaderConstantSet(constantSet, 0);
    R_UnmapShaderConstantSet(constantSet, 1);
    R_UnmapShaderConstantSet(constantSet, 2);
    float fraction = 0.0f;
    if (cent->nextState.eType == ET_ACTOR_CORPSE)
    {
        if (!cent->miscTime || cent->miscTime > cgameGlob->time)
            cent->miscTime = cgameGlob->time;
        volatile float elapsed = static_cast<float>(CG_ActorTimeDifference_SP(cgameGlob->time, cent->miscTime));
        volatile float duration = cg_corpseHighlightFadeTime->current.value * 1000.0f;
        fraction = elapsed / duration;
        if (fraction >= 1.0f)
            return false;
    }

    const float offsetR = cg_infraredHighlightOffset->current.value;
    const float scaleR = cg_infraredHighlightScale->current.value;
    float heroLightingR[4] = {scaleR, 0.0f, 0.0f, offsetR};
    const float scaleG = cg_infraredHighlightScale->current.value;
    const float offsetG = cg_infraredHighlightOffset->current.value;
    float heroLightingG[4] = {0.0f, scaleG, 0.0f, offsetG};
    const float scaleB = cg_infraredHighlightScale->current.value;
    const float offsetB = cg_infraredHighlightOffset->current.value;
    float heroLightingB[4] = {0.0f, 0.0f, scaleB, offsetB};
    if (fraction >= 0.0f)
    {
        float *rows[3] = {heroLightingR, heroLightingG, heroLightingB};
        for (int row = 0; row != 3; ++row)
        {
            const float scale = rows[row][row];
            const float offset = rows[row][3];
            volatile float scaleDifference = 1.0f - scale;
            volatile float scaledDifference = scaleDifference * fraction;
            rows[row][row] = scaledDifference + scale;
            volatile float offDiagonal = fraction * 0.0f;
            for (int column = 0; column != 3; ++column)
                if (column != row)
                    rows[row][column] = offDiagonal;
            volatile float scaledOffset = -offset * fraction;
            rows[row][3] = scaledOffset + offset;
        }
    }
    if (R_MapShaderConstantSet(constantSet, 0, "heroLightingR"))
        R_SetShaderConstantSetValue(constantSet, 0, heroLightingR);
    if (R_MapShaderConstantSet(constantSet, 1, "heroLightingG"))
        R_SetShaderConstantSetValue(constantSet, 1, heroLightingG);
    if (R_MapShaderConstantSet(constantSet, 2, "heroLightingB"))
        R_SetShaderConstantSetValue(constantSet, 2, heroLightingB);
    return true;
}


#endif
void __cdecl CG_ActorCorpse(int localClientNum, centity_s *cent)
{
#ifdef KISAK_SP
    const entityState_s *state = &cent->nextState;
    if (!CG_Actor_RenderPrefix_SP(cent))
        return;
    const DObj *obj = Com_GetClientDObj(state->number, localClientNum);
    if (!obj)
        return;

    IKImport_IsIKEntity(reinterpret_cast<unsigned char *>(const_cast<DObj *>(obj)));
    if (CG_EntityNeedsScriptThread(localClientNum, cent))
    {
        cent->clientFlags |= 0x100u;
        Scr_AddInt(localClientNum, SCRIPTINSTANCE_CLIENT);
        const unsigned short thread = CScr_ExecEntThread(cent, cg_scr_data.entityspawned, 1u);
        Scr_FreeThread(thread, SCRIPTINSTANCE_CLIENT);
    }
    const int species = state->lerp.u.actor.species;
    if (zombiemode->current.enabled && species != AI_SPECIES_DOG && species != AI_SPECIES_ZOMBIE_DOG)
    {
        const bool oldEyeFlag = (cent->clientFlags & 0x1000000u) != 0;
        const bool eyeFlag = (state->lerp.eFlags & 0x20000u) != 0;
        if (oldEyeFlag != eyeFlag && cg_scr_sp_data.zombieEyeCallback)
        {
            cent->clientFlags = (cent->clientFlags & ~0x1000000u) | (eyeFlag ? 0x1000000u : 0u);
            Scr_AddInt(eyeFlag, SCRIPTINSTANCE_CLIENT);
            Scr_AddInt(localClientNum, SCRIPTINSTANCE_CLIENT);
            const unsigned short thread = CScr_ExecEntThread(cent, cg_scr_sp_data.zombieEyeCallback, 2u);
            Scr_FreeThread(thread, SCRIPTINSTANCE_CLIENT);
        }
    }
    cg_s *cgameGlob = CG_GetLocalClientGlobals(localClientNum);
    if (cent->animCommandTime && cent->animCommandTime > cgameGlob->time)
        return;

    CG_Actor_PreControllers(localClientNum, cent);
    float lightingOrigin[3] = {cent->pose.origin[0], cent->pose.origin[1], cent->pose.origin[2] + 32.0f};
    if (cent->pose.isRagdoll && cent->pose.ragdollHandle > 0)
        Ragdoll_GetRootOrigin(cent->pose.ragdollHandle, cent->pose.origin);
    R_FoliageNotifyActorPosition(localClientNum, cent->pose.origin);
    if (ai_water_trails->current.enabled)
        CG_DrawWaterTrail(localClientNum, cent, (state->lerp.eFlags & 0x400000u) == 0);
    const float burnFraction = CG_ActorBurnFraction_SP(localClientNum, cent);
    const unsigned int renderFxFlags = CG_ActorRenderFlags_SP(state);
    ShaderConstantSet savedConstants;
    if (CG_IsInfrared(localClientNum))
    {
        R_CopyShaderConstantSet_SP(&cent->pose.constantSet, &savedConstants);
        CG_HighlightActor_SP(localClientNum, cent);
    }
    if (cent->fxHeartbeat)
    {
        FX_ThroughWithEffect(localClientNum, cent->fxHeartbeat, true);
        cent->fxHeartbeat = 0;
    }
    const ShaderConstantSet *constants = R_ShaderConstantSetIsUsed(&cent->pose.constantSet)
        ? &cent->pose.constantSet : nullptr;
    R_AddDObjToScene(obj, &cent->pose, state->number, renderFxFlags, lightingOrigin,
        0.0f, 0.0f, burnFraction, cent->pose.wetness, -1, -1, constants, 0, 0.0f, 1.0f);
    if (CG_IsInfrared(localClientNum))
        R_CopyShaderConstantSet_SP(&savedConstants, &cent->pose.constantSet);
#else
#ifdef KISAK_SP
    if (!CG_Actor_RenderPrefix_SP(cent))
        return;
#endif
    actorInfo_t *ai; // [esp+2Ch] [ebp-20h]
    const DObj *obj; // [esp+30h] [ebp-1Ch]
    entityState_s *p_nextState; // [esp+38h] [ebp-14h]
    unsigned int corpseIndex; // [esp+3Ch] [ebp-10h]
    float lightingOrigin[3]; // [esp+40h] [ebp-Ch] BYREF

    p_nextState = &cent->nextState;
    if ( (cent->nextState.lerp.eFlags & 0x40000) == 0
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_actors_mp.cpp",
                    338,
                    0,
                    "%s",
                    "es->lerp.eFlags & EF_DEAD") )
    {
        __debugbreak();
    }
#if defined(KISAK_NX) && !defined(KISAK_SP)
    // The DObj goes even for a corpse flagged EF_NODRAW: those skipped the
    // free below, kept the actor slot's tree, and the next zombie in the slot
    // animated at double speed again -- seen after killing several at once.
    if ( (!nx_physics || !nx_physics->current.enabled) && Com_GetClientDObj(p_nextState->number, localClientNum) )
        CG_SafeDObjFree(localClientNum, p_nextState->number);
#endif
    if ( (cent->nextState.lerp.eFlags & 0x20) == 0 )
    {
#if defined(KISAK_NX) && !defined(KISAK_SP)
        // HACK while the physics solver is unported (nx_physics off): there is
        // no ragdoll to take the body over when the death animation ends, and
        // the corpse's client anim tree starts empty, so it stood in its bind
        // pose (a T-pose). The corpse is not drawn instead: it vanishes as the
        // death animation finishes. The server entity stays, so scripts keep
        // their reference and the corpse limit still clears it. Copying the
        // server's corpse tree over instead crashed: it carries server script
        // strings into client notetrack notifies. See README-SWITCH, physics.
        // The actor's DObj must go too: it still points at the anim tree of
        // the actor slot, and the next zombie in that slot shares it, so
        // CG_UpdateEntInfo advanced that tree twice a frame (double speed).
        if ( !nx_physics || !nx_physics->current.enabled )
        {
            if ( Com_GetClientDObj(p_nextState->number, localClientNum) )
                CG_SafeDObjFree(localClientNum, p_nextState->number);
            return;
        }
#endif
#ifdef KISAK_SP
        // Retail SP uses the corpse slot carried in actorNum because the actor
        // entity itself becomes the corpse; it is not one of MP's fixed 36..43
        // clone entities.
        corpseIndex = p_nextState->lerp.u.actor.actorNum;
#else
        corpseIndex = p_nextState->number - 36;
#endif
#ifdef KISAK_SP
        if ( corpseIndex >= MAX_ACTOR_CORPSES
#else
        if ( corpseIndex >= 8
#endif
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\cgame_mp\\cg_actors_mp.cpp",
                        345,
                        0,
                        "%s",
                        "(unsigned)corpseIndex < MAX_ACTOR_CORPSES") )
        {
            __debugbreak();
        }
#ifdef KISAK_SP
        ai = &CG_GetLocalClientStaticGlobals(localClientNum)->actorCorpseInfo[corpseIndex];
#else
        ai = (actorInfo_t *)&CG_GetLocalClientStaticGlobals(localClientNum)->actorCorpseInfo[corpseIndex].animInfo.legs.pitchAngle;
#endif
        CG_UpdateActorDObj(localClientNum, cent, ai);
        obj = Com_GetClientDObj(p_nextState->number, localClientNum);
        if ( obj )
        {
            BG_Dog_UpdateAnimationState(localClientNum, &cent->nextState, ai);
            lightingOrigin[0] = cent->pose.origin[0];
            lightingOrigin[1] = cent->pose.origin[1];
            lightingOrigin[2] = cent->pose.origin[2];
            if ( (cent->nextState.lerp.eFlags & 8) != 0 )
            {
                lightingOrigin[2] = lightingOrigin[2] + 12.0;
            }
            else if ( (cent->nextState.lerp.eFlags & 4) != 0 )
            {
                lightingOrigin[2] = lightingOrigin[2] + 20.0;
            }
            else
            {
                lightingOrigin[2] = lightingOrigin[2] + 32.0;
            }
            CG_HighlightPlayer(localClientNum, cent, &cent->pose.constantSet, 0);
            R_AddDObjToScene(
                obj,
                &cent->pose,
                p_nextState->number,
                0x400000u,
                lightingOrigin,
                0.0,
                0.0,
                0.0,
                0.0,
                -1,
                -1,
                &cent->pose.constantSet,
                0,
                0.0,
                1.0);
        }
    }
#endif
}
