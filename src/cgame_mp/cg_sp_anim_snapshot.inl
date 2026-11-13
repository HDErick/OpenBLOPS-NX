// Included by cg_animscripted_mp.cpp; kept out of MP builds.
#include <bgame/bg_sp_anim_snapshot.h>
#include <clientscript/cscr_animtree.h>
#include <game_mp/g_main_mp.h>
#include <game_mp/actor_mp.h>
#include <game_mp/g_scr_main_mp.h>
#include <server_mp/sv_main_mp.h>
#include <server_mp/sv_snapshot_mp.h>
#include <qcommon/msg_mp.h>
#include <algorithm>

extern ClientTreeStorage gGScrXAnimTreesForClient[2][128];

namespace
{
SpAnimSnapshot::Frame s_remoteAnimFrames[32];
int s_remoteAnimAppliedTime[1024] = {};
DObj *s_remoteAnimAppliedObj[1024] = {};
int s_remoteAnimLastServerTime = -1;
SpAnimSnapshot::TreeCatalog<XAnim_s> s_remoteAnimTrees;

const char *CG_AnimSnapshotTreeName_SP(const XAnim_s *anims)
{
    // debugName is deliberately absent in normal non-developer builds.
    // The script loader retains these filenames for CScr_RetrieveAnimTree.
    for (unsigned user = 0; user < 2; ++user)
        for (unsigned i = 1; i <= gScrAnimPub[SCRIPTINSTANCE_SERVER].xanim_num[user] && i < MAX_XANIMTREE_NUM; ++i)
        {
            const ClientTreeStorage &stored = gGScrXAnimTreesForClient[user][i];
            if (stored.animTree.anims == anims && stored.strName
                && gScrAnimPub[SCRIPTINSTANCE_SERVER].xanim_lookup[user][i].anims == anims)
                return stored.strName;
        }
    return nullptr;
}

bool CG_UsesSelectedAnimCommands_SP(int type)
{
    return type == ET_ACTOR || type == ET_ACTOR_CORPSE || type == ET_SCRIPTMOVER
        || type == ET_VEHICLE || type == ET_VEHICLE_CORPSE;
}

bool CG_UsesAnimSnapshot_SP(int type)
{
    return CG_UsesSelectedAnimCommands_SP(type)
        || type == ET_MG42 || type == ET_PLANE;
}

const SpAnimSnapshot::Frame *CG_RemoteAnimFrame_SP(int localClientNum)
{
    if (com_sv_running->current.enabled) return nullptr;
    const cg_s *cg = CG_GetLocalClientGlobals(localClientNum);
    const snapshot_s *snap = cg->nextSnap ? cg->nextSnap : cg->snap;
    if (!snap) return nullptr;
    for (const SpAnimSnapshot::Frame &frame : s_remoteAnimFrames)
        if (frame.serverTime == snap->serverTime) return &frame;
    return nullptr;
}
}

void CG_CaptureRemoteAnimTrees_SP()
{
    // CG_Init frees the temporary SERVER script VM and zeros xanim_num after
    // compilation. XAnim assets live in the map hunk; retain their identities
    // independently, replacing this catalog on every remote map load.
    s_remoteAnimTrees.Clear();
    for (unsigned user = 0; user < 2; ++user)
        for (unsigned i = 1; i <= gScrAnimPub[SCRIPTINSTANCE_SERVER].xanim_num[user] && i < MAX_XANIMTREE_NUM; ++i)
        {
            XAnim_s *anims = gScrAnimPub[SCRIPTINSTANCE_SERVER].xanim_lookup[user][i].anims;
            const char *name = anims ? CG_AnimSnapshotTreeName_SP(anims) : nullptr;
            if (!name) continue;
            s_remoteAnimTrees.Add(name, anims->size, anims);
            Com_Printf(16, "SP remote animation tree registered: '%s' (%u nodes)\n", name, anims->size);
        }
}

void CG_ClearRemoteAnimTrees_SP()
{
    s_remoteAnimTrees.Clear();
}

void CG_ResetRemoteAnimSnapshots_SP()
{
    for (SpAnimSnapshot::Frame &frame : s_remoteAnimFrames) frame = {};
    memset(s_remoteAnimAppliedTime, 0, sizeof(s_remoteAnimAppliedTime));
    memset(s_remoteAnimAppliedObj, 0, sizeof(s_remoteAnimAppliedObj));
    s_remoteAnimLastServerTime = -1;
}

#ifdef KISAK_SP
bool CG_HasRemoteAnimSnapshot_SP(int localClientNum)
{
    return CG_RemoteAnimFrame_SP(localClientNum) != nullptr;
}
#endif
const SpAnimSnapshot::Entity *CG_GetRemoteAnimEntity_SP(int localClientNum, int entNum)
{
    const SpAnimSnapshot::Frame *frame = CG_RemoteAnimFrame_SP(localClientNum);
    if (!frame) return nullptr;
    for (const SpAnimSnapshot::Entity &entity : frame->entities)
        if (static_cast<int>(entity.number) == entNum) return &entity;
    return nullptr;
}

XAnim_s *CG_GetRemoteAnimations_SP(int localClientNum, int entNum)
{
    const SpAnimSnapshot::Entity *entity = CG_GetRemoteAnimEntity_SP(localClientNum, entNum);
    if (!entity) return nullptr;
    // Server table indices are process-local. Resolve the named tree that the
    // remote client's script load compiled, then verify the animation layout.
    if (XAnim_s *anims = s_remoteAnimTrees.Find(entity->tree, entity->treeSize)) return anims;
    for (const auto &stored : s_remoteAnimTrees.entries)
        Com_Printf(16, "SP animation tree lookup: name '%s' nodes %u\n", stored.name.c_str(), stored.size);
    Com_Error(ERR_DROP, "SP animation tree '%s' (%u nodes) is not loaded on client", entity->tree.c_str(), entity->treeSize);
    return nullptr;
}

bool CG_ApplyRemoteAnimSnapshot_SP(int localClientNum, int entNum, DObj *obj, bool force)
{
    if (com_sv_running->current.enabled) return false;

    const SpAnimSnapshot::Frame *frame = CG_RemoteAnimFrame_SP(localClientNum);
    if (!frame || entNum < 0 || entNum >= 1023 || !obj || !obj->localTree) return true;
    if (CG_UsesSelectedAnimCommands_SP(CG_GetEntity(localClientNum, entNum)->nextState.eType))
        return false;
    if (!CG_UsesAnimSnapshot_SP(CG_GetEntity(localClientNum, entNum)->nextState.eType)) return true;
    if (!force && s_remoteAnimAppliedTime[entNum] == frame->serverTime && s_remoteAnimAppliedObj[entNum] == obj) return true;
    const SpAnimSnapshot::Entity *entity = CG_GetRemoteAnimEntity_SP(localClientNum, entNum);
    XAnimTree_s *tree = obj->localTree;
    if (entity && tree->anims != CG_GetRemoteAnimations_SP(localClientNum, entNum)) return true;
    if (entity)
        for (const SpAnimSnapshot::Node &node : entity->nodes)
        {
            const XAnimEntry &entry = tree->anims->entries[node.index];
            if ((!node.parent && node.index != 0)
                || (node.parent && (node.index == 0
                    || entry.parent != entity->nodes[node.parent-1].index
                    || IsLeafNode(&tree->anims->entries[entity->nodes[node.parent-1].index]))))
            {
                Com_Error(ERR_DROP, "SP animation snapshot has invalid local tree hierarchy");
                return true;
            }
        }
    // Each payload is a complete current baseline. This removes animations
    // stopped since the previous snapshot, including node/entity reuse.
    XAnimClearTree(tree);
    std::vector<unsigned> instances(1, 0);
    XModelNameMap modelMap[512];
    if (entity) XAnimInitModelMap(obj->localModels, obj->numModels, modelMap);
    unsigned previousRoot = 0;
    if (entity)
        for (const SpAnimSnapshot::Node &node : entity->nodes)
        {
            const XAnimEntry &entry = tree->anims->entries[node.index];
            unsigned short animToModel = IsLeafNode(&entry) ? static_cast<unsigned short>(XAnimGetAnimMap(entry.parts, modelMap)) : 0;
            unsigned index = XAnimAllocInfoWithParent(tree, animToModel, node.index, instances[node.parent], 1);
            XAnimInfo *info = GetAnimInfo(index);
            XAnimInitInfo(info);
            // The allocator's parent-zero path assumes a single root. Preserve
            // even restarted root instances without touching its global sentinel.
            if (!node.parent)
            {
                info->prev = static_cast<unsigned short>(previousRoot);
                info->next = 0;
                if (previousRoot) { GetAnimInfo(previousRoot)->next = static_cast<unsigned short>(index); tree->children = static_cast<unsigned short>(instances[1]); }
                previousRoot = index;
            }
            instances.push_back(index);
            info->state.currentAnimTime = node.time;
            info->state.oldTime = node.time;
            info->state.cycleCount = node.cycle;
            info->state.oldCycleCount = node.cycle;
            info->state.goalTime = node.goalTime;
            info->state.goalWeight = node.goalWeight;
            info->state.weight = node.weight;
            info->state.rate = node.rate;
            info->state.instantWeightChange = false;
            info->notifyChild = static_cast<unsigned short>(node.notifyChild);
            info->notifyIndex = -1;
            info->notifyType = node.goalWeight > 0.001f ? 2 : 0;
        }
    // DObjUpdateClientInfo advances normally between snapshots. Never reset
    // currentTime each render frame or replay the integrated command ring.
    s_remoteAnimAppliedTime[entNum] = frame->serverTime;
    s_remoteAnimAppliedObj[entNum] = obj;
    return true;
}

static bool SV_CopyActorStates_SP(SpAnimSnapshot::Frame &frame)
{
    std::vector<SpAnimSnapshot::Actor> actors;
    for (int i = 0; i < MAX_ACTORS; ++i)
    {
        const actor_s *actor = reinterpret_cast<const actor_s *>(
            reinterpret_cast<const unsigned char *>(sv.gameActors) + i * sv.gameActorSize);
        if (!actor->inuse) continue;
        const actorState_s state = *G_GetActorState_SP(i);
        if (state.actorNum != i) continue;
        SpAnimSnapshot::Actor value;
        value.actorNum = state.actorNum;
        value.entityNum = state.entityNum;
        value.team = static_cast<int>(state.team);
        value.modelindex = state.modelindex;
        for (unsigned j = 0; j < 6; ++j)
        {
            value.attachModelIndex[j] = state.attachModelIndex[j];
            value.attachTagIndex[j] = state.attachTagIndex[j];
        }
        value.attachIgnoreCollision = state.attachIgnoreCollision;
        const char *end = static_cast<const char *>(memchr(state.name, 0, sizeof(state.name)));
        if (!end) return false;
        value.name.assign(state.name, end);
        value.attachedEntNum = state.attachedEntNum;
        value.attachedEntTime = state.attachedEntTime;
        value.attachedTagIndex = state.attachedTagIndex;
        value.animScriptedAnim = state.animScriptedAnim;
        value.animScriptedAnimTime = state.animScriptedAnimTime;
        value.hudwarningType = state.hudwarningType;
        value.lookAtEntNum = state.lookAtEntNum;
        if (!SpAnimSnapshot::ValidActor(value)) return false;
        actors.push_back(std::move(value));
    }
    frame.actors.swap(actors);
    return true;
}

void SV_WriteAnimSnapshot_SP(msg_t *msg, int serverTime, int firstEntity, int numEntities, const client_t *client, const clientSnapshot_t *selected)
{
    SpAnimSnapshot::Frame frame; frame.serverTime = serverTime;
    unsigned totalNodes = 0;
    for (int i = 0; i < numEntities; ++i)
    {
        const entityState_s &state = svsHeader.snapshotEntities[(firstEntity+i) % svsHeader.numSnapshotEntities];
        if (state.number < 0 || state.number >= 1023) continue;
        // Player animation already has its own MP-derived state transport.
        if (!CG_UsesAnimSnapshot_SP(state.eType)) continue;
        DObj *obj = Com_GetServerDObj(state.number);
        if (!obj || !obj->tree || !obj->tree->anims) continue;
        const XAnimTree_s *tree = obj->tree;
        SpAnimSnapshot::Entity entity;
        entity.number = state.number;
        entity.treeSize = tree->anims->size;
        const char *treeName = CG_AnimSnapshotTreeName_SP(tree->anims);
        if (!treeName)
        {
            Com_Error(ERR_DROP, "SP animation snapshot entity %d has no registered script tree name", state.number);
            return;
        }
        entity.tree = treeName;
        if (state.eType == ET_ACTOR || state.eType == ET_ACTOR_CORPSE)
        {
            const gentity_s &ent = g_entities[state.number];
            entity.ignoreCollision = ent.attachIgnoreCollision & 63;
            for (unsigned j = 0; j < 6; ++j)
            {
                entity.attachments[j].model = ent.attachModelNames[j];
                if (ent.attachModelNames[j] && ent.attachTagNames[j])
                    entity.attachments[j].tag = SL_ConvertToString(ent.attachTagNames[j], SCRIPTINSTANCE_SERVER);
            }
        }
        if (CG_UsesSelectedAnimCommands_SP(state.eType))
        {
            frame.entities.push_back(std::move(entity));
            continue;
        }
        std::vector<std::pair<unsigned, unsigned>> pendingInfo;
        if (tree->children) pendingInfo.emplace_back(tree->children, 0);
        while (!pendingInfo.empty())
        {
            if (++totalNodes > SpAnimSnapshot::MaxNodes)
            {
                Com_Error(ERR_DROP, "SP animation snapshot exceeds %u active nodes", SpAnimSnapshot::MaxNodes);
                return;
            }
            const auto pending = pendingInfo.back();
            unsigned infoIndex = pending.first;
            pendingInfo.pop_back();
            const XAnimInfo *info = GetAnimInfo(infoIndex);
            if (info->next) pendingInfo.emplace_back(info->next, pending.second);
            if (info->children) pendingInfo.emplace_back(info->children, static_cast<unsigned>(entity.nodes.size()+1));
            SpAnimSnapshot::Node node;
            node.parent = pending.second;
            node.index = info->animIndex; node.notifyChild = info->notifyChild;
            node.time = info->state.currentAnimTime; node.cycle = info->state.cycleCount;
            node.goalTime = info->state.goalTime; node.goalWeight = info->state.goalWeight;
            node.weight = info->state.weight; node.rate = info->state.rate;
            entity.nodes.push_back(node);
        }
        frame.entities.push_back(std::move(entity));
    }
    std::sort(frame.entities.begin(), frame.entities.end(), [](const SpAnimSnapshot::Entity &a, const SpAnimSnapshot::Entity &b) { return a.number < b.number; });
    const int clientNum = static_cast<int>(client - svs.clients);
    for (int n = 0; n < selected->numAnimCommands; ++n)
    {
        const AnimCommand_SP &command = svs.snapshotAnimCommands[clientNum][
            (static_cast<unsigned int>(selected->firstAnimCommand) + static_cast<unsigned int>(n)) % svs.numSnapshotAnimCommands];
        SpAnimSnapshot::Command value;
        value.index = command.index; value.type = command.type;
        value.serverTime = command.serverTime; value.sequence = command.sequence;
        value.entNum = command.entNum; value.animIndex = command.animIndex;
        value.rootAnimIndex = command.rootAnimIndex;
        value.weight = command.weight; value.rate = command.rate;
        value.goalTime = command.goalTime; value.flags = command.flags;
        frame.commands.push_back(SpAnimSnapshot::QuantizeCommand(value));
    }
    if (!SV_CopyActorStates_SP(frame))
    {
        Com_Error(ERR_DROP, "Invalid SP selected actor state");
        return;
    }
    std::vector<unsigned char> bytes;
    if (!SpAnimSnapshot::Valid(frame))
    {
        Com_Error(ERR_DROP, "SP animation snapshot contains invalid animation state (%u animated entities)", static_cast<unsigned>(frame.entities.size()));
        return;
    }
    if (!SpAnimSnapshot::Encode(frame, bytes) || msg->overflowed
        || static_cast<int>(bytes.size()) + 16 > msg->maxsize - msg->cursize)
    {
        Com_Error(ERR_DROP, "SP animation snapshot exceeds validated node/packet budget (%u animated entities)", static_cast<unsigned>(frame.entities.size()));
        return;
    }
    MSG_WriteLong(msg, static_cast<int>(bytes.size()));
    MSG_WriteData(msg, bytes.data(), static_cast<unsigned>(bytes.size()));
}

static bool CL_CopyActorStates_SP(clSnapshot_t *snapshot, const SpAnimSnapshot::Frame &frame)
{
    if (frame.actors.size() > MAX_ACTORS) return false;
    actorState_s states[MAX_ACTORS] = {};
    for (unsigned i = 0; i < frame.actors.size(); ++i)
    {
        const SpAnimSnapshot::Actor &actor = frame.actors[i];
        if (!SpAnimSnapshot::ValidActor(actor)
            || (i && frame.actors[i-1].actorNum >= actor.actorNum)) return false;
        actorState_s &state = states[i];
        state.actorNum = actor.actorNum;
        state.entityNum = actor.entityNum;
        state.team = static_cast<team_t>(actor.team);
        state.modelindex = actor.modelindex;
        for (unsigned j = 0; j < 6; ++j)
        {
            state.attachModelIndex[j] = actor.attachModelIndex[j];
            state.attachTagIndex[j] = actor.attachTagIndex[j];
        }
        state.attachIgnoreCollision = actor.attachIgnoreCollision;
        memcpy(state.name, actor.name.data(), actor.name.size());
        state.attachedEntNum = actor.attachedEntNum;
        state.attachedEntTime = actor.attachedEntTime;
        state.attachedTagIndex = actor.attachedTagIndex;
        state.animScriptedAnim = actor.animScriptedAnim;
        state.animScriptedAnimTime = actor.animScriptedAnimTime;
        state.hudwarningType = actor.hudwarningType;
        state.lookAtEntNum = actor.lookAtEntNum;
    }
    memcpy(snapshot->actors, states, sizeof(states));
    snapshot->numActors = static_cast<int>(frame.actors.size());
    return true;
}

void CL_ReadAnimSnapshot_SP(msg_t *msg, int serverTime, int messageNum, clSnapshot_t *snapshot)
{
    const int length = MSG_ReadLong(msg);
    if (msg->overflowed || length < 0 || static_cast<unsigned>(length) > SpAnimSnapshot::MaxBytes)
    {
        Com_Error(ERR_DROP, "Invalid SP animation snapshot length");
        return;
    }
    std::vector<unsigned char> bytes(static_cast<unsigned>(length));
    MSG_ReadData(msg, bytes.data(), length);
    SpAnimSnapshot::Frame frame;
    if (msg->overflowed || !SpAnimSnapshot::Decode(bytes.data(), static_cast<unsigned>(length), frame) || frame.serverTime != serverTime)
    {
        Com_Error(ERR_DROP, "Invalid SP animation snapshot payload");
        return;
    }
    if (!CL_CopyActorStates_SP(snapshot, frame))
    {
        Com_Error(ERR_DROP, "Invalid SP selected actor state");
        return;
    }
    AnimCommand_SP commands[SpAnimSnapshot::MaxCommands] = {};
    for (unsigned n = 0; n < frame.commands.size(); ++n)
    {
        const SpAnimSnapshot::Command &value = frame.commands[n];
        AnimCommand_SP &command = commands[n];
        command.index = value.index; command.type = value.type;
        command.serverTime = value.serverTime; command.sequence = value.sequence;
        command.entNum = value.entNum; command.animIndex = value.animIndex;
        command.rootAnimIndex = value.rootAnimIndex;
        command.weight = value.weight; command.rate = value.rate;
        command.goalTime = value.goalTime; command.flags = value.flags;
    }
    memcpy(snapshot->animCommands, commands, sizeof(commands));
    snapshot->numAnimCommands = static_cast<int>(frame.commands.size());
    frame.actors.clear();
    frame.commands.clear();
    if (serverTime < s_remoteAnimLastServerTime) CG_ResetRemoteAnimSnapshots_SP();
    s_remoteAnimLastServerTime = serverTime;
    s_remoteAnimFrames[messageNum & 31] = std::move(frame);
}
