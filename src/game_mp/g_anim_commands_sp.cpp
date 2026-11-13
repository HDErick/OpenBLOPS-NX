#include "g_anim_commands_sp.h"

#ifdef KISAK_SP
#include "g_main_mp.h"
#include <bgame/bg_animation.h>
#include <qcommon/common.h>
#include <server_mp/sv_main_mp.h>
#include <universal/dvar.h>
#include <cstring>
#include <cstddef>
#include <universal/memfile.h>
#include <cmath>
#include "g_utils_mp.h"
#include <xanim/xanim.h>
#include "checkpoint_fields.h"
#include <memory>


namespace
{
constexpr unsigned int SP_ANIM_COMMAND_CAPACITY = 1024;
constexpr unsigned int SP_ANIM_COMMAND_REFERENCE_CAPACITY = 4096;

struct AnimCommandReference_SP
{
    short primary;
    short dependent;
    short pending;
    short reserved;
    int expiry;
};

struct AnimCommandListNode_SP
{
    short index;
    short previous;
    short next;
};

struct AnimCommandList_SP
{
    int head;
    int tail;
};

AnimCommand_SP g_animCommands_SP[SP_ANIM_COMMAND_CAPACITY];
short g_animCommandRefCounts_SP[SP_ANIM_COMMAND_CAPACITY];
AnimCommandReference_SP g_animCommandReferences_SP[SP_ANIM_COMMAND_REFERENCE_CAPACITY];
unsigned int g_animCommandSequences_SP[SP_ANIM_COMMAND_CAPACITY];
AnimCommandListNode_SP g_animCommandNodes_SP[SP_ANIM_COMMAND_CAPACITY];
AnimCommandList_SP g_freeAnimCommands_SP;
AnimCommandList_SP g_usedAnimCommands_SP;
int g_lastAnimCommandSnapshotTime_SP;
unsigned char g_animCommandReuse_SP[SP_ANIM_COMMAND_CAPACITY];

void G_MoveAnimCommand_SP(AnimCommandList_SP &from, AnimCommandList_SP &to, int index)
{
    AnimCommandListNode_SP &node = g_animCommandNodes_SP[index];
    if (node.previous == -1)
    {
        if (node.next == -1)
            from.head = from.tail = -1;
        else
        {
            from.head = node.next;
            g_animCommandNodes_SP[node.next].previous = -1;
        }
    }
    else
    {
        g_animCommandNodes_SP[node.previous].next = node.next;
        if (node.next == -1)
            from.tail = node.previous;
        else
            g_animCommandNodes_SP[node.next].previous = g_animCommandNodes_SP[node.previous].index;
    }
    if (to.head != -1)
    {
        g_animCommandNodes_SP[to.tail].next = node.index;
        node.previous = g_animCommandNodes_SP[to.tail].index;
        to.tail = index;
        node.next = -1;
    }
    else
    {
        to.head = to.tail = index;
        node.previous = node.next = -1;
    }
}

int G_MinAnimCommandSnapshotTime_SP()
{
    int time = 0x7fffffff;
    for (int clientNum = 0; clientNum < com_maxclients->current.integer; ++clientNum)
        if (svs.clients[clientNum].lastSnapshotTime < time)
            time = svs.clients[clientNum].lastSnapshotTime;
    return time;
}

void G_RemoveAnimCommandReferences_SP(int index)
{
    int count = g_animCommandRefCounts_SP[index];
    for (AnimCommandReference_SP &reference : g_animCommandReferences_SP)
    {
        if (count <= 0)
            return;
        if (reference.primary == index)
        {
            if (reference.pending != -1)
            {
                reference.primary = reference.pending;
                reference.pending = -1;
                reference.expiry = -1;
            }
            else
                reference.primary = -1;
            --count;
        }
        else if (reference.pending == index)
        {
            reference.pending = -1;
            --count;
        }
        else if (reference.dependent == index)
            reference.dependent = -1;
    }
}

void G_ClearAnimCommandDefault_SP(AnimCommand_SP &command)
{
    command.serverTime = 0;
    G_RemoveAnimCommandReferences_SP(command.index);
    g_animCommandRefCounts_SP[command.index] = 0;
    G_MoveAnimCommand_SP(g_usedAnimCommands_SP, g_freeAnimCommands_SP, command.index);
}

AnimCommand_SP *G_AllocAnimCommandDefault_SP()
{
    if (g_freeAnimCommands_SP.head != -1)
    {
        const int index = g_animCommandNodes_SP[g_freeAnimCommands_SP.head].index;
        g_animCommands_SP[index].index = index;
        G_MoveAnimCommand_SP(g_freeAnimCommands_SP, g_usedAnimCommands_SP, index);
        return &g_animCommands_SP[index];
    }
    for (int node = g_usedAnimCommands_SP.head; node != -1; node = g_animCommandNodes_SP[node].next)
    {
        const int index = g_animCommandNodes_SP[node].index;
        AnimCommand_SP &command = g_animCommands_SP[index];
        if (g_animCommandRefCounts_SP[index])
            continue;
        if ((command.type == 1 || command.type == 2 || command.type == 4 || command.type == 5)
            && command.serverTime > g_lastAnimCommandSnapshotTime_SP)
            continue;
        command.index = index;
        G_MoveAnimCommand_SP(g_usedAnimCommands_SP, g_usedAnimCommands_SP, index);
        return &command;
    }
    return nullptr;
}

void G_ClearAnimCommand_SP(AnimCommand_SP &command)
{
    command.serverTime = 0;
    if (!g_oldAnimCmdNetwork->current.enabled)
    {
        G_RemoveAnimCommandReferences_SP(command.index);
        g_animCommandRefCounts_SP[command.index] = 0;
        G_MoveAnimCommand_SP(g_usedAnimCommands_SP, g_freeAnimCommands_SP, command.index);
    }
}

AnimCommand_SP *G_AllocAnimCommand_SP(int entNum, const AnimCommand_SP &incoming)
{
    if (g_oldAnimCmdNetwork->current.enabled)
    {
        const bool clear = incoming.type == 1 || incoming.type == 2;
        AnimCommand_SP *candidate = nullptr;
        for (int index = 0; index < SP_ANIM_COMMAND_CAPACITY; ++index)
        {
            AnimCommand_SP &stored = g_animCommands_SP[index];
            if (stored.entNum != entNum || stored.animIndex != incoming.animIndex)
                continue;
            if ((stored.type == 1 || stored.type == 2) && !clear)
                continue;
            if ((incoming.type == 2 || incoming.type == 6) && stored.type != incoming.type)
                continue;
            stored.index = index;
            candidate = &stored;
            if (stored.serverTime > 0)
                return candidate;
        }
        if (candidate)
            return candidate;
        for (int index = 0; index < SP_ANIM_COMMAND_CAPACITY; ++index)
            if (!g_animCommands_SP[index].serverTime)
            {
                g_animCommands_SP[index].index = index;
                return &g_animCommands_SP[index];
            }
        return nullptr;
    }
    if (g_freeAnimCommands_SP.head != -1)
    {
        const int index = g_animCommandNodes_SP[g_freeAnimCommands_SP.head].index;
        g_animCommands_SP[index].index = index;
        G_MoveAnimCommand_SP(g_freeAnimCommands_SP, g_usedAnimCommands_SP, index);
        return &g_animCommands_SP[index];
    }
    for (int node = g_usedAnimCommands_SP.head; node != -1; node = g_animCommandNodes_SP[node].next)
    {
        const int index = g_animCommandNodes_SP[node].index;
        AnimCommand_SP &command = g_animCommands_SP[index];
        if (g_animCommandRefCounts_SP[index])
            continue;
        if ((command.type == 1 || command.type == 2 || command.type == 4 || command.type == 5)
            && command.serverTime > g_lastAnimCommandSnapshotTime_SP)
            continue;
        command.index = index;
        G_MoveAnimCommand_SP(g_usedAnimCommands_SP, g_usedAnimCommands_SP, index);
        return &command;
    }
    return nullptr;
}

bool G_AnimCommandMatches_SP(const AnimCommand_SP &stored, const AnimCommand_SP &incoming)
{
    if (stored.animIndex != incoming.animIndex || fabsf(stored.weight - incoming.weight) > 0.015f)
        return false;
    if (stored.goalTime != incoming.goalTime)
        return false;
    if ((!stored.serverTime || g_animCommandReuse_SP[stored.index]) && stored.serverTime != incoming.serverTime)
        return false;
    return stored.type == incoming.type && stored.entNum == incoming.entNum
        && stored.rootAnimIndex == incoming.rootAnimIndex && !(fabsf(stored.rate - incoming.rate) > 0.001f);
}

bool G_AnimCommandHasZeroGoal_SP(const XAnimTree_s *tree, unsigned int animIndex)
{
    unsigned int index = tree->children ? XAnimGetInfoIndex_r(animIndex, tree->children) : 0;
    if (!index)
        return true;
    for (;;)
    {
        if (GetAnimInfo(index)->state.goalWeight < 0.001f)
            return true;
        index = GetAnimInfo(index)->parent;
        if (!index)
            return false;
    }
}

bool G_AnimCommandIsDescendant_SP(const XAnimTree_s *tree, unsigned int parentAnim, unsigned int childAnim)
{
    if (!tree->children)
        return false;
    const unsigned int parent = XAnimGetInfoIndex_r(parentAnim, tree->children);
    unsigned int child = XAnimGetInfoIndex_r(childAnim, tree->children);
    if (!parent || !child)
        return false;
    do
    {
        if (child == parent)
            return true;
        child = GetAnimInfo(child)->parent;
    } while (child);
    return false;
}

void G_ClearOtherAnimCommands_SP(int entNum, const XAnimTree_s *tree, unsigned int animIndex,
                               const AnimCommand_SP &incoming)
{
    for (AnimCommand_SP &stored : g_animCommands_SP)
        if (stored.serverTime > 0 && stored.entNum == entNum && stored.index != incoming.index)
        {
            if (stored.animIndex == animIndex)
            {
                if (incoming.type != 2 || stored.type == 2)
                    G_ClearAnimCommand_SP(stored);
            }
            else if ((incoming.type == 1 && !incoming.animIndex)
                || G_AnimCommandIsDescendant_SP(tree, animIndex, stored.animIndex))
                G_ClearAnimCommand_SP(stored);
        }
}
}

void __cdecl G_InitAnimCommands_SP()
{
    G_ResetAnimCommandSequences_SP();
    memset(g_animCommands_SP, 0, sizeof(g_animCommands_SP));
    memset(g_animCommandRefCounts_SP, 0, sizeof(g_animCommandRefCounts_SP));
    for (int index = 0; index < SP_ANIM_COMMAND_CAPACITY; ++index)
    {
        g_animCommandNodes_SP[index].index = static_cast<short>(index);
        g_animCommandNodes_SP[index].previous = static_cast<short>(index ? index - 1 : -1);
        g_animCommandNodes_SP[index].next = static_cast<short>(index == SP_ANIM_COMMAND_CAPACITY - 1 ? -1 : index + 1);
    }
    g_freeAnimCommands_SP.head = 0;
    g_freeAnimCommands_SP.tail = SP_ANIM_COMMAND_CAPACITY - 1;
    g_usedAnimCommands_SP.head = g_usedAnimCommands_SP.tail = -1;
    for (AnimCommandReference_SP &reference : g_animCommandReferences_SP)
    {
        reference.primary = reference.dependent = reference.pending = -1;
        reference.expiry = -1;
    }
}

void __cdecl G_AddAnimCommandReference_SP(short commandIndex, short infoIndex, int expiry)
{
    AnimCommandReference_SP &reference = g_animCommandReferences_SP[infoIndex];
    short primary = reference.primary;
    const short pending = reference.pending;
    if (commandIndex == -1)
    {
        if (primary != -1)
            --g_animCommandRefCounts_SP[primary];
        if (pending != -1)
            --g_animCommandRefCounts_SP[pending];
        reference.expiry = -1;
        reference.primary = reference.pending = -1;
        return;
    }
    const int snapshotTime = G_MinAnimCommandSnapshotTime_SP();
    if (commandIndex == primary || commandIndex == pending)
        return;
    if (pending != -1)
    {
        if (primary != -1)
            --g_animCommandRefCounts_SP[primary];
        reference.primary = pending;
        reference.pending = -1;
        reference.expiry = -1;
        primary = pending;
    }
    if (svs.time <= expiry && (reference.primary == -1
        || expiry == g_animCommands_SP[reference.primary].serverTime
        || g_animCommands_SP[reference.primary].serverTime <= snapshotTime))
    {
        if (primary != -1)
            --g_animCommandRefCounts_SP[primary];
        reference.expiry = -1;
        reference.primary = commandIndex;
        reference.pending = -1;
    }
    else
    {
        reference.pending = commandIndex;
        reference.expiry = expiry;
    }
    ++g_animCommandRefCounts_SP[commandIndex];
}

void __cdecl G_AddAnimCommandDependent_SP(short commandIndex, short infoIndex)
{
    short &dependent = g_animCommandReferences_SP[infoIndex].dependent;
    if (dependent == commandIndex)
        return;
    if (dependent != -1)
        --g_animCommandRefCounts_SP[dependent];
    dependent = commandIndex;
    if (commandIndex != -1)
        ++g_animCommandRefCounts_SP[commandIndex];
}

void __cdecl G_UpdateAnimCommandReferences_SP()
{
    const int snapshotTime = G_MinAnimCommandSnapshotTime_SP();
    for (AnimCommandReference_SP &reference : g_animCommandReferences_SP)
        if (reference.expiry >= level_bgs.time && (reference.primary == -1
            || reference.expiry == g_animCommands_SP[reference.primary].serverTime
            || g_animCommands_SP[reference.primary].serverTime <= snapshotTime))
        {
            if (reference.primary != -1)
                --g_animCommandRefCounts_SP[reference.primary];
            reference.primary = reference.pending;
            reference.pending = -1;
            reference.expiry = -1;
        }
}

AnimCommand_SP *__cdecl G_GetAnimCommandsBase_SP()
{
    return g_animCommands_SP;
}

void __cdecl G_SetLastAnimCommandSnapshotTime_SP(int time)
{
    g_lastAnimCommandSnapshotTime_SP = time;
}

void __cdecl G_ResetAnimCommandSequences_SP()
{
    memset(g_animCommandSequences_SP, 0, sizeof(g_animCommandSequences_SP));
}

int __cdecl G_StoreAnimCommandDefault_SP(const gentity_s *ent, int type, unsigned int animIndex,
                        unsigned int rootAnimIndex, float weight, float goalTime, float rate, int flags)
{
    const int entNum = ent->s.number;
    AnimCommand_SP incoming = {};
    incoming.index = -1;
    incoming.type = type;
    incoming.entNum = entNum;
    incoming.animIndex = animIndex;
    incoming.rootAnimIndex = rootAnimIndex;
    incoming.flags = static_cast<unsigned char>(flags);
    incoming.serverTime = level.time == g_lastAnimCommandSnapshotTime_SP
        ? static_cast<int>(static_cast<unsigned int>(level.time) + 50u) : level.time;
    if (!level.time)
        incoming.serverTime = 50;
    incoming.weight = weight <= 0.0f ? 0.0f : (weight >= 1.0f ? 1.0f : weight);
    incoming.goalTime = goalTime <= 0.0f ? 0.0f : (goalTime >= 1.5f ? 1.5f : goalTime);
    incoming.rate = rate <= 0.0f ? 0.0f : (rate >= 3.0f ? 3.0f : rate);
    AnimCommand_SP *stored = G_AllocAnimCommandDefault_SP();
    if (!stored)
    {
        Com_PrintWarning(19, "G_StoreAnimCommand:  T-Pose potential. No available animation slots\n");
        return -1;
    }
    incoming.index = stored->index;
    *stored = incoming;
    stored->sequence = static_cast<int>(g_animCommandSequences_SP[entNum]++);
    return incoming.index;
}

void __cdecl G_ReleaseAnimCommandsForEntDefault_SP(const gentity_s *ent)
{
    const int entNum = ent->s.number;
    for (int node = g_usedAnimCommands_SP.head; node != -1; )
    {
        const int index = g_animCommandNodes_SP[node].index;
        node = g_animCommandNodes_SP[node].next;
        if (g_animCommands_SP[index].entNum == entNum)
            G_ClearAnimCommandDefault_SP(g_animCommands_SP[index]);
    }
}

void __cdecl G_PruneUnreferencedAnimCommandsDefault_SP()
{
    bool remoteClient = false;
    for (int clientNum = 0; clientNum < com_maxclients->current.integer; ++clientNum)
    {
        const client_t &client = svs.clients[clientNum];
        if (client.header.state >= CS_CONNECTED && client.header.netchan.remoteAddress.type != NA_LOOPBACK)
        {
            remoteClient = true;
            break;
        }
    }
    for (int node = g_usedAnimCommands_SP.head; node != -1; )
    {
        const int index = g_animCommandNodes_SP[node].index;
        node = g_animCommandNodes_SP[node].next;
        if (g_animCommandRefCounts_SP[index])
            continue;
        AnimCommand_SP &command = g_animCommands_SP[index];
        const bool protect = command.type == 1 || command.type == 2 || command.type == 4 || command.type == 5;
        if (!protect || ((remoteClient || command.serverTime <= g_lastAnimCommandSnapshotTime_SP)
            && (command.serverTime <= g_lastAnimCommandSnapshotTime_SP || command.goalTime != 0.0f)))
            G_ClearAnimCommandDefault_SP(command);
    }
}

int __cdecl G_StoreAnimCommand_SP(const gentity_s *ent, const XAnimTree_s *tree, int type, unsigned int animIndex,
                        unsigned int rootAnimIndex, float weight, float goalTime, float rate, int flags)
{
    const int entNum = ent->s.number;
    AnimCommand_SP incoming = {};
    incoming.index = -1;
    incoming.type = type;
    incoming.entNum = entNum;
    incoming.animIndex = animIndex;
    incoming.rootAnimIndex = rootAnimIndex;
    incoming.flags = static_cast<unsigned char>(flags);
    incoming.serverTime = level.time == g_lastAnimCommandSnapshotTime_SP
        ? static_cast<int>(static_cast<unsigned int>(level.time) + 50u) : level.time;
    if (!level.time)
        incoming.serverTime = 50;
    incoming.weight = weight <= 0.0f ? 0.0f : (weight >= 1.0f ? 1.0f : weight);
    incoming.goalTime = goalTime <= 0.0f ? 0.0f : (goalTime >= 1.5f ? 1.5f : goalTime);
    incoming.rate = rate <= 0.0f ? 0.0f : (rate >= 3.0f ? 3.0f : rate);
    AnimCommand_SP *stored = G_AllocAnimCommand_SP(entNum, incoming);
    if (!stored)
    {
        Com_PrintWarning(19, "G_StoreAnimCommand:  T-Pose potential. No available animation slots\n");
        return -1;
    }
    const bool clear = type == 1 || type == 2;
    if (stored->serverTime && g_oldAnimCmdNetwork->current.enabled && !clear
        && G_AnimCommandMatches_SP(*stored, incoming) && !(flags & 2)
        && (!(weight > 0.001f) || !G_AnimCommandHasZeroGoal_SP(tree, animIndex)))
        return -1;
    if (g_oldAnimCmdNetwork->current.enabled && clear && (incoming.flags & 1))
        G_ClearOtherAnimCommands_SP(entNum, tree, animIndex, incoming);
    incoming.index = stored->index;
    if (g_oldAnimCmdNetwork->current.enabled)
    {
        g_animCommandReuse_SP[incoming.index] = 0;
        for (int index = 0; index < SP_ANIM_COMMAND_CAPACITY; ++index)
            g_animCommandReuse_SP[index] = g_animCommands_SP[index].entNum == entNum
                && g_animCommands_SP[index].animIndex == static_cast<int>(animIndex);
    }
    *stored = incoming;
    stored->sequence = static_cast<int>(g_animCommandSequences_SP[entNum]++);
    return incoming.index;
}

void __cdecl G_ReleaseAnimCommandsForEnt_SP(const gentity_s *ent)
{
    const int entNum = ent->s.number;
    if (g_oldAnimCmdNetwork->current.enabled)
    {
        for (AnimCommand_SP &command : g_animCommands_SP)
            if (command.entNum == entNum)
                G_ClearAnimCommand_SP(command);
        return;
    }
    for (int node = g_usedAnimCommands_SP.head; node != -1; )
    {
        const int index = g_animCommandNodes_SP[node].index;
        node = g_animCommandNodes_SP[node].next;
        if (g_animCommands_SP[index].entNum == entNum)
            G_ClearAnimCommand_SP(g_animCommands_SP[index]);
    }
}

void __cdecl G_PruneUnreferencedAnimCommands_SP()
{
    bool remoteClient = false;
    for (int clientNum = 0; clientNum < com_maxclients->current.integer; ++clientNum)
    {
        const client_t &client = svs.clients[clientNum];
        if (client.header.state >= CS_CONNECTED && client.header.netchan.remoteAddress.type != NA_LOOPBACK)
        {
            remoteClient = true;
            break;
        }
    }
    if (g_oldAnimCmdNetwork->current.enabled)
    {
        for (AnimCommand_SP &command : g_animCommands_SP)
        {
            if (command.serverTime <= 0)
                continue;
            gentity_s *ent = &g_entities[command.entNum];
            if (!ent->r.inuse)
            {
                G_ClearAnimCommand_SP(command);
                continue;
            }
            XAnimTree_s *tree = G_GetEntAnimTree(ent);
            if (!tree)
            {
                G_ClearAnimCommand_SP(command);
                continue;
            }
            if (command.type == 1 || command.type == 2)
            {
                const unsigned int delta = static_cast<unsigned int>(level.time)
                    - static_cast<unsigned int>(command.serverTime);
                int age;
                memcpy(&age, &delta, sizeof(age));
                if (age < 5000)
                    continue;
            }
            if (XAnimIsPrimitive(XAnimGetAnims(tree), command.animIndex)
                && !XAnimHasFinished(tree, command.animIndex))
                G_ClearAnimCommand_SP(command);
        }
        return;
    }
    for (int node = g_usedAnimCommands_SP.head; node != -1; )
    {
        const int index = g_animCommandNodes_SP[node].index;
        node = g_animCommandNodes_SP[node].next;
        if (g_animCommandRefCounts_SP[index])
            continue;
        AnimCommand_SP &command = g_animCommands_SP[index];
        const bool protect = command.type == 1 || command.type == 2 || command.type == 4 || command.type == 5;
        if (!protect || ((remoteClient || command.serverTime <= g_lastAnimCommandSnapshotTime_SP)
            && (command.serverTime <= g_lastAnimCommandSnapshotTime_SP || command.goalTime != 0.0f)))
            G_ClearAnimCommand_SP(command);
    }
}
namespace
{
static_assert(sizeof(AnimCommandListNode_SP) == 6);
static_assert(offsetof(AnimCommandListNode_SP, index) == 0);
static_assert(offsetof(AnimCommandListNode_SP, previous) == 2);
static_assert(offsetof(AnimCommandListNode_SP, next) == 4);
static_assert(sizeof(short) == 2);
static_assert(sizeof(int) == 4);
static_assert(offsetof(AnimCommand_SP, index) == 0);
static_assert(offsetof(AnimCommand_SP, flags) == 40);

void G_WriteAnimCommandArchiveData_SP(MemoryFile *memFile, unsigned int size, const void *data)
{
    MemFile_WriteData(memFile, size, const_cast<unsigned char *>(static_cast<const unsigned char *>(data)));
}

void G_WriteAnimCommandListState_SP(MemoryFile *memFile, const AnimCommandList_SP &list)
{
    short index = list.head == -1 ? -1 : g_animCommandNodes_SP[list.head].index;
    G_WriteAnimCommandArchiveData_SP(memFile, sizeof(index), &index);
    if (list.head != -1)
    {
        index = g_animCommandNodes_SP[list.tail].index;
        G_WriteAnimCommandArchiveData_SP(memFile, sizeof(index), &index);
    }
}

void G_ReadAnimCommandListState_SP(MemoryFile *memFile, AnimCommandList_SP &list)
{
    short index;
    MemFile_ReadData(memFile, sizeof(index), reinterpret_cast<unsigned char *>(&index));
    if (index == -1)
        list.head = list.tail = -1;
    else
    {
        list.head = index;
        MemFile_ReadData(memFile, sizeof(index), reinterpret_cast<unsigned char *>(&index));
        list.tail = index;
    }
}
}

void __cdecl G_WriteAnimCommandState_SP(MemoryFile *memFile)
{
    for (const AnimCommandListNode_SP &node : g_animCommandNodes_SP)
        G_WriteAnimCommandArchiveData_SP(memFile, sizeof(node), &node);
    for (int node = g_usedAnimCommands_SP.head; node != -1; node = g_animCommandNodes_SP[node].next)
    {
        const int index = g_animCommandNodes_SP[node].index;
        G_WriteAnimCommandArchiveData_SP(memFile, sizeof(index), &index);
        G_WriteAnimCommandArchiveData_SP(memFile, sizeof(g_animCommands_SP[index]), &g_animCommands_SP[index]);
        G_WriteAnimCommandArchiveData_SP(memFile, sizeof(g_animCommandRefCounts_SP[index]), &g_animCommandRefCounts_SP[index]);
    }
    const int end = -1;
    G_WriteAnimCommandArchiveData_SP(memFile, sizeof(end), &end);
    G_WriteAnimCommandListState_SP(memFile, g_freeAnimCommands_SP);
    G_WriteAnimCommandListState_SP(memFile, g_usedAnimCommands_SP);
}

void __cdecl G_ReadAnimCommandState_SP(MemoryFile *memFile)
{
    for (AnimCommandListNode_SP &node : g_animCommandNodes_SP)
        MemFile_ReadData(memFile, sizeof(node), reinterpret_cast<unsigned char *>(&node));
    int index;
    MemFile_ReadData(memFile, sizeof(index), reinterpret_cast<unsigned char *>(&index));
    while (index >= 0)
    {
        if (index >= static_cast<int>(SP_ANIM_COMMAND_CAPACITY))
            Com_Error(ERR_DROP, "\x15G_LoadGame: animcmd num out of range (%i, MAX = %i)", index, static_cast<int>(SP_ANIM_COMMAND_CAPACITY));
        MemFile_ReadData(memFile, sizeof(g_animCommands_SP[index]), reinterpret_cast<unsigned char *>(&g_animCommands_SP[index]));
        short references;
        MemFile_ReadData(memFile, sizeof(references), reinterpret_cast<unsigned char *>(&references));
        g_animCommandRefCounts_SP[index] = references;
        MemFile_ReadData(memFile, sizeof(index), reinterpret_cast<unsigned char *>(&index));
    }
    G_ReadAnimCommandListState_SP(memFile, g_freeAnimCommands_SP);
    G_ReadAnimCommandListState_SP(memFile, g_usedAnimCommands_SP);
}


namespace {
struct CheckpointCommands_SP {
    AnimCommand_SP commands[1024]{};short counts[1024]{};
    AnimCommandReference_SP references[4096]{};unsigned sequences[1024]{};
    AnimCommandListNode_SP nodes[1024]{};AnimCommandList_SP free{},used{};
    int lastTime=0;unsigned char reuse[1024]{};
};
template<class V>void CheckpointCommandFields_SP(V& v,CheckpointCommands_SP& state){
    for(auto& x:state.commands){v.Field("index",x.index);v.Field("type",x.type);v.Field("time",x.serverTime);v.Field("sequence",x.sequence);v.Field("entity",x.entNum);v.Field("animation",x.animIndex);v.Field("root",x.rootAnimIndex);v.Field("weight",x.weight);v.Field("rate",x.rate);v.Field("goal_time",x.goalTime);v.Field("flags",x.flags);}
    v.Field("counts",state.counts);v.Field("sequences",state.sequences);v.Field("reuse",state.reuse);v.Field("snapshot_time",state.lastTime);
    for(auto& x:state.references){v.Field("primary",x.primary);v.Field("dependent",x.dependent);v.Field("pending",x.pending);v.Field("reserved",x.reserved);v.Field("expiry",x.expiry);}
    for(auto& x:state.nodes){v.Field("index",x.index);v.Field("previous",x.previous);v.Field("next",x.next);}
    v.Field("free_head",state.free.head);v.Field("free_tail",state.free.tail);v.Field("used_head",state.used.head);v.Field("used_tail",state.used.tail);
}
}
void G_CheckpointCaptureAnimCommands_SP(std::vector<std::uint8_t>& bytes){
    auto state=std::make_unique<CheckpointCommands_SP>();
    std::memcpy(state->commands,g_animCommands_SP,sizeof(state->commands));std::memcpy(state->counts,g_animCommandRefCounts_SP,sizeof(state->counts));std::memcpy(state->sequences,g_animCommandSequences_SP,sizeof(state->sequences));
    std::memcpy(state->references,g_animCommandReferences_SP,sizeof(state->references));std::memcpy(state->nodes,g_animCommandNodes_SP,sizeof(state->nodes));std::memcpy(state->reuse,g_animCommandReuse_SP,sizeof(state->reuse));state->free=g_freeAnimCommands_SP;state->used=g_usedAnimCommands_SP;state->lastTime=g_lastAnimCommandSnapshotTime_SP;
    bytes.clear();checkpoint::Resources refs;checkpoint::FieldWriter writer(bytes,refs);CheckpointCommandFields_SP(writer,*state);
}
void G_CheckpointApplyAnimCommands_SP(const std::vector<std::uint8_t>& bytes,const std::vector<unsigned short>& remap,bool apply){
    auto state=std::make_unique<CheckpointCommands_SP>();checkpoint::Resources refs;checkpoint::FieldReader reader(bytes,refs);CheckpointCommandFields_SP(reader,*state);reader.Finish();
    bool seen[1024]{};unsigned total=0;
    for(auto list:{state->free,state->used}){int previous=-1;for(int index=list.head;index!=-1;index=state->nodes[index].next){if(index<0||index>=1024||seen[index]||state->nodes[index].index!=index||state->nodes[index].previous!=previous)throw std::runtime_error("checkpoint animation command list corrupt");seen[index]=true;++total;previous=index;}if(previous!=list.tail)throw std::runtime_error("checkpoint animation command tail corrupt");}
    if(total!=1024)throw std::runtime_error("checkpoint animation command slot missing");
    for(const auto& ref:state->references)for(int index:{ref.primary,ref.dependent,ref.pending})if(index < -1||index>=1024)throw std::runtime_error("checkpoint animation command reference invalid");
    if(!apply)return;
    std::memcpy(g_animCommands_SP,state->commands,sizeof(state->commands));std::memcpy(g_animCommandSequences_SP,state->sequences,sizeof(state->sequences));std::memcpy(g_animCommandNodes_SP,state->nodes,sizeof(state->nodes));std::memcpy(g_animCommandReuse_SP,state->reuse,sizeof(state->reuse));g_freeAnimCommands_SP=state->free;g_usedAnimCommands_SP=state->used;g_lastAnimCommandSnapshotTime_SP=state->lastTime;
    std::memset(g_animCommandRefCounts_SP,0,sizeof(g_animCommandRefCounts_SP));
    for(unsigned old=1;old<4096;++old)if(old<remap.size()&&remap[old]){const auto& saved=state->references[old];g_animCommandReferences_SP[remap[old]]=saved;for(int command:{saved.primary,saved.pending})if(command!=-1)++g_animCommandRefCounts_SP[command];}
}
#endif
