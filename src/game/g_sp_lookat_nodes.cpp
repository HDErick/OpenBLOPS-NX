#include "g_sp_lookat_nodes.h"
#ifdef KISAK_SP
#include "actor_events.h"
#include "sentient.h"
#include <game_mp/actor_mp.h>
#include <game_mp/g_main_mp.h>
#include <qcommon/common.h>
#include <universal/com_math.h>
#include <cmath>
#include <cstring>
#include <climits>
#include <bit>

const dvar_t *ai_playerLOSHalfWidth;

namespace
{
const dvar_t *losRange;
const dvar_t *losMinTime;
struct LookAtNodeState
{
    bool moved;
};
LookAtNodeState states[32];

float DistanceSquaredXY(const float *a, const float *b)
{
    const float x = a[0] - b[0], y = a[1] - b[1];
    return x * x + y * y;
}

// Retail 0x008189C0. The unsquared width comparison and unnormalized
// direction smoothing are intentional instruction-level behavior.
bool StableLookDirection(gentity_s *player, const float *forward)
{
    gclient_s *client = player->client;
    float length = sqrtf(forward[1] * forward[1] + forward[0] * forward[0]);
    if (length <= 0.0f) length = 1.0f;
    const float inverseLength = 1.0f / length;
    const float direction[2] = { forward[0] * inverseLength, forward[1] * inverseLength };
    const float x = player->r.currentOrigin[0] - client->playerLOSPosition[0];
    const float y = player->r.currentOrigin[1] - client->playerLOSPosition[1];
    if (y * y + x * x > ai_playerLOSHalfWidth->current.value
        || direction[1] * client->playerLOSDirection[1]
           + direction[0] * client->playerLOSDirection[0] < 0.9848f)
    {
        for (int i = 0; i < 2; ++i)
        {
            client->playerLOSPosition[i] = player->r.currentOrigin[i];
            client->playerLOSDirection[i] = direction[i];
        }
        client->playerLOSTime = level.time;
        return false;
    }
    for (int i = 0; i < 2; ++i)
    {
        client->playerLOSPosition[i] += (player->r.currentOrigin[i] - client->playerLOSPosition[i]) * 0.25f;
        client->playerLOSDirection[i] += (direction[i] - client->playerLOSDirection[i]) * 0.25f;
    }
    const int deadline = std::bit_cast<int>(static_cast<unsigned int>(client->playerLOSTime)
        + static_cast<unsigned int>(losMinTime->current.integer));
    if (deadline > level.time)
        return false;
    client->playerLOSTime = level.time;
    return true;
}

// Retail 0x00818C00; source-compatible path nodes and sentient handles.
void PublishNodes(gentity_s *player, const float *start, const float *forward, float length)
{
    float end[3], middle[3];
    Vec3Mad(start, length, forward, end);
    for (int i = 0; i < 3; ++i) middle[i] = (start[i] + end[i]) * 0.5f;
    const float horizontalLengthSq = DistanceSquaredXY(start, end);
    pathsort_t nodes[4];
    const int count = Path_NodesInCylinder(middle, sqrtf(horizontalLengthSq) * 0.5f,
        80.0f, nodes, 4, 0x83FFC);
    const float widthSq = ai_playerLOSHalfWidth->current.value * ai_playerLOSHalfWidth->current.value;
    for (int i = 0; i < count; ++i)
    {
        pathnode_t *node = nodes[i].node;
        // Retail's degenerate XY line produces an unordered distance and does
        // not reject a node. Avoid the source math helper's zero-length assert.
        if (horizontalLengthSq > 0.0f
            && PointToLineDistSq2D(node->constant.vOrigin, start, end) > widthSq)
            continue;
        if (node->dynamic.pOwner.isDefined())
        {
            sentient_s *owner = node->dynamic.pOwner.sentient();
            if (owner->pClaimedNode == node)
            {
                if (Actor_PointNearNode(owner->ent->r.currentOrigin, node)) continue;
                actor_s *actor = owner->ent->actor;
                if (!actor || owner->eTeam != player->sentient->eTeam
                    || Actor_KeepClaimedNode(actor)) continue;
                // Retail actor+0x1AA4. The reconstructed cover search must
                // maintain this existing member for the >1 branch to execute.
                if (actor->numCoverNodesInGoal > 1)
                {
                    Path_RelinquishNodeSoon(owner);
                    node->dynamic.pOwner.setSentient(nullptr);
                }
            }
        }
        node->dynamic.inPlayerLOSTime = level.time + losMinTime->current.integer;
    }
}
}

void G_SPRegisterLookAtNodes()
{
    losRange = _Dvar_RegisterFloat("ai_playerLOSRange", 150.0f, 0.0f, 500.0f, 0x2080, "");
    ai_playerLOSHalfWidth = _Dvar_RegisterFloat("ai_playerLOSHalfWidth", 15.0f, 0.0f, 100.0f, 0x2080, "");
    losMinTime = _Dvar_RegisterInt("ai_playerLOSMinTime", 1500, 0, INT_MAX, 0x2080, "");
}

void G_SPResetLookAtNodes()
{
    memset(states, 0, sizeof(states));
    for (gclient_s &client : g_clients)
    {
        memset(client.playerLOSPosition, 0, sizeof(client.playerLOSPosition));
        memset(client.playerLOSDirection, 0, sizeof(client.playerLOSDirection));
        client.playerLOSTime = 0;
    }
}
void G_SPResetClientLookAtNodes(int clientNum)
{
    if (unsigned(clientNum) < 32)
    {
        memset(&states[clientNum], 0, sizeof(states[clientNum]));
        gclient_s &client = level.clients[clientNum];
        memset(client.playerLOSPosition, 0, sizeof(client.playerLOSPosition));
        memset(client.playerLOSDirection, 0, sizeof(client.playerLOSDirection));
        client.playerLOSTime = 0;
    }
}

void G_SPUpdateLookAtNodes(gentity_s *player, const float *start,
    const float *forward, float traceFraction)
{
    // Retail LOS publication does not depend on the look-at entity. Call after
    // the first trace and before either the null-hit return or trigger retrace.
    if (!player || !player->client || !player->sentient || unsigned(player->s.number) >= 32)
        return;
    if (StableLookDirection(player, forward))
    {
        const float distance = traceFraction * 15000.0f;
        PublishNodes(player, start, forward,
            distance < losRange->current.value ? distance : losRange->current.value);
    }
    if (player->client->ps.weapFlags & 2)
    {
        float end[3];
        Vec3Mad(start, losRange->current.value, forward, end);
        Actor_BroadcastLineEvent(player, nullptr, AI_EV_BLOCK_FRIENDLIES,
            1 << (unsigned(player->sentient->eTeam) & 31), start, end, 0.0f);
    }
}

void G_SPUpdatePlayerNodeClaim(gentity_s *player, gentity_s *previousTarget)
{
    // Retail 0x004634A0 runs in G_RunFrame's early client loop, so the target
    // belongs to the previous frame, before ClientEndFrame replaces the handle.
    if (!player || !player->r.inuse || !player->client || !player->sentient
        || unsigned(player->s.number) >= 32) return;
    sentient_s *self = player->sentient;
    const bool moved = DistanceSquaredXY(self->oldOrigin, player->r.currentOrigin) >= 0.01f;
    states[player->s.number].moved = moved;
    Vec3Copy(player->r.currentOrigin, self->oldOrigin);
    if (Sentient_NearestNodeDirty(self, moved)) Sentient_InvalidateNearestNode(self);

    // Retail 0x007D4770.
    pathnode_t *nearest = Sentient_NearestNode(self);
    float origin[3];
    Sentient_GetOrigin(self, origin);
    if (nearest && DistanceSquaredXY(origin, nearest->constant.vOrigin) > 1024.0f) nearest = nullptr;
    if (nearest != self->pClaimedNode)
    {
        if (self->pClaimedNode)
        {
            if (!previousTarget || !previousTarget->actor || !previousTarget->sentient
                || previousTarget->sentient->pClaimedNode)
                Path_RelinquishNodeNow(self);
            else
                Path_RelinquishNodeSoon(self);
        }
        if (nearest)
        {
            if (!nearest->dynamic.pOwner.isDefined()) Path_ForceClaimNode(nearest, self);
            else
            {
                sentient_s *owner = nearest->dynamic.pOwner.sentient();
                if (owner != self)
                {
                    Sentient_GetOrigin(owner, origin);
                    if (DistanceSquaredXY(origin, nearest->constant.vOrigin) >= 225.0f)
                        Path_ForceClaimNode(nearest, self);
                }
            }
        }
    }
    Sentient_BanNearNodes(self);
}
#endif
