#include "actor_exposed.h"
#include <game_mp/actor_mp.h>
#include <game_mp/g_main_mp.h>
#include "actor_senses.h"
#include "actor_state.h"
#include <game_mp/g_utils_mp.h>
#include "actor_navigation.h"
#include "actor_orientation.h"
#include "actor_events.h"
#include <cgame_mp/cg_local_mp.h>
#ifdef KISAK_SP
#include "actor_animapi.h"
#include "actor_corpse.h"
#include "actor_team_move.h"
#include "actor_badplace.h"
#include "sentient.h"
#include "actor_threat.h"
#include "turret.h"
#include <qcommon/cm_trace.h>
#include "pathnode.h"
#include <game_mp/g_spawn_mp.h>
#include <server/sv_world.h>
#include <server/sv_game.h>
#include <clientscript/scr_const.h>
#include <universal/com_math.h>
#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <universal/dvar.h>
#include <cstring>
#endif

#ifdef KISAK_SP
// Local port helper for the zombie specialization of retail 0x00426710;
// this is not a recovered source symbol. The species-zero node selectors
// are unreachable for this controller's AI_SPECIES_ZOMBIE table entry.
static void Actor_Zombie_ExposedCombat_SP(actor_s *self)
{
    iassert(self->species == AI_SPECIES_ZOMBIE);

    Actor_CheckCollisions(self);
    Actor_ClearPileUp(self);
    self->bUseGoalWeight = false;

    scr_animscript_t *combat = &g_animScriptTable[self->species]->combat;
    if ( self->fixedNode && level.time > self->exposedStartTime + 2000
        && self->pAnimScriptFunc != combat )
    {
        self->exposedStartTime = level.time;
    }

    // Reselecting the same script preserves its thread and script-set mode,
    // including melee while safeToChangeScript is false. Actor_AnimStop with
    // ->stop would replace that thread and bypass zombie_combat::TryMelee.
    Actor_SetAnimScript(
        self, combat, AI_MOVE_STOP,
        self->pCloseEnt.isDefined() ? AI_ANIM_MOVE_CODE : AI_ANIM_USE_BOTH_DELTAS,
        AI_ANIM_FUNCTION_COMBAT);
    // Retail 0x007C0EF0 always faces the enemy without a claimed 0x8000
    // node. Its exceptional node/visibility predicates remain unported.
    const pathnode_t *node = self->sentient->pClaimedNode;
    if ( !node || (node->constant.spawnflags & 0x8000) == 0 )
        Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY);
}

extern badplace_t g_badplaces[256]; // actor_badplace.cpp

// Retail 0x005A29F0: is the goal position inside any active bad place?
static bool Actor_IsGoalInAnyBadPlace_SP(const float *pos)
{
    for ( unsigned int i = 0; i < 0x100; ++i )
    {
        const badplace_t *bp = &g_badplaces[i];
        switch ( bp->type )
        {
        case 1u:
        case 3u:
        case 4u:
            if ( IsPosInsideArc(
                     pos,
                     15.0f,
                     bp->parms.arc.origin,
                     bp->parms.arc.radius,
                     bp->parms.arc.angle0,
                     bp->parms.arc.angle1,
                     bp->parms.arc.halfheight) )
                return true;
            break;
        case 2u:
            if ( SV_EntityContact(pos, pos, bp->parms.brush.volume) )
                return true;
            break;
        default:
            break;
        }
    }
    return false;
}

// Retail 0x007CF300: claimed node has a visibility link to the enemy's node.
static bool Actor_ClaimedNodeSeesEnemyNode_SP(actor_s *self)
{
    pathnode_t *claimed = self->sentient->pClaimedNode;
    if ( !claimed )
        return false;
    pathnode_t *enemyNode = Sentient_NearestNode(Actor_GetTargetSentient(self));
    return enemyNode && Path_NodesVisible(claimed, enemyNode);
}

// Retail 0x0050DE30: enemy position known within the last 10 seconds.
bool Actor_EnemyPosRecentlyKnown_SP(actor_s *self, int hadPath)
{
    sentient_s *enemy = Actor_GetTargetSentient(self);
    if ( !enemy )
        return false;
    if ( !hadPath && Actor_ClaimedNodeSeesEnemyNode_SP(self) )
        return true;
    const int t = self->sentientInfo[enemy - level.sentients].lastKnownPosTime;
    return t && level.time - t < 10000;
}

// Retail 0x0061C020: enemy seen within the last 10 seconds.
static bool Actor_EnemyRecentlyVisible_SP(actor_s *self, int checkNode)
{
    sentient_s *enemy = Actor_GetTargetSentient(self);
    if ( !enemy )
        return Actor_CanSeeEntityEx(self, Actor_GetTargetEntity(self), self->fovDot, self->fMaxSightDistSqrd) != 0;
    if ( checkNode && Actor_ClaimedNodeSeesEnemyNode_SP(self) )
        return true;
    const int t = self->sentientInfo[enemy - level.sentients].VisCache.iLastVisTime;
    return t && level.time - t < 10000;
}

// Retail 0x007BB6A0: is the enemy within pathEnemyLookahead (plus 256 when we
// had no path) of the actor or of the next pathEnemyFightDist of path.
static bool Actor_IsEnemyAlongPath_SP(actor_s *self, const float *enemyPos, const float *lookaheadPos, int hadPath)
{
    const path_t *path = &self->Path;
    float range = self->pathEnemyLookahead;
    if ( !hadPath )
        range += 256.0f;
    const float rangeSq = range * range;

    float dx = enemyPos[0] - self->ent->r.currentOrigin[0];
    float dy = enemyPos[1] - self->ent->r.currentOrigin[1];
    if ( dx * dx + dy * dy < rangeSq )
        return true;

    dx = lookaheadPos[0] - enemyPos[0];
    dy = lookaheadPos[1] - enemyPos[1];
    float along = path->lookaheadDir[0] * dx + path->lookaheadDir[1] * dy;
    if ( along < path->fLookaheadDist )
    {
        float distSq;
        if ( along > 0.0f )
        {
            distSq = path->lookaheadDir[1] * dx - path->lookaheadDir[0] * dy;
            distSq *= distSq;
        }
        else
        {
            distSq = dx * dx + dy * dy;
        }
        if ( distSq < rangeSq )
            return true;
    }

    int i = path->lookaheadNextNode;
    const pathpoint_t *pt = &path->pts[i];
    dx = pt->vOrigPoint[0] - enemyPos[0];
    dy = pt->vOrigPoint[1] - enemyPos[1];
    along = pt->fDir2D[0] * dx + pt->fDir2D[1] * dy;
    if ( along < path->fLookaheadDistToNextNode )
    {
        float distSq;
        if ( along > 0.0f )
        {
            distSq = pt->fDir2D[1] * dx - pt->fDir2D[0] * dy;
            distSq *= distSq;
        }
        else
        {
            distSq = dx * dx + dy * dy;
        }
        if ( distSq < rangeSq )
            return true;
    }

    float total = path->fLookaheadDistToNextNode + path->fLookaheadDist;
    while ( total < self->pathEnemyFightDist && --i >= 0 )
    {
        pt = &path->pts[i];
        dx = pt->vOrigPoint[0] - enemyPos[0];
        dy = pt->vOrigPoint[1] - enemyPos[1];
        along = pt->fDir2D[0] * dx + pt->fDir2D[1] * dy;
        if ( along < pt->fOrigLength )
        {
            float distSq;
            if ( along > 0.0f )
            {
                distSq = pt->fDir2D[1] * dx - pt->fDir2D[0] * dy;
                distSq *= distSq;
            }
            else
            {
                distSq = dx * dx + dy * dy;
            }
            if ( distSq < rangeSq )
                return true;
        }
        total += pt->fOrigLength;
    }
    return false;
}

// Retail 0x007BD830: decide whether the enemy is close enough to the path
// that the actor should stop following it (true => caller clears the path).
// May switch the code goal to the enemy (useEnemyGoal).
static bool Actor_CheckStopForEnemy_SP(actor_s *self, bool enemyKnown, pathnode_t *node, int hadPath)
{
    gentity_s *enemyEnt = Actor_GetTargetEntity(self);
    sentient_s *enemy = enemyEnt ? enemyEnt->sentient : NULL;
    if ( !enemy )
        return false;
    if ( !Actor_HasPath(self) )
        return false;

    // Retail 0x007BB620: enemy inside pathEnemyFightDist / goal height.
    const float *enemyPos = enemy->ent->r.currentOrigin;
    const float *myPos = self->ent->r.currentOrigin;
    {
        const float ex = enemyPos[0] - myPos[0];
        const float ey = enemyPos[1] - myPos[1];
        if ( ex * ex + ey * ey >= self->pathEnemyFightDist * self->pathEnemyFightDist )
            return false;
        if ( fabsf(enemyPos[2] - myPos[2]) > self->codeGoal.height )
            return false;
    }
    if ( node && Actor_PointNearNode(myPos, node) )
        return false;
    // Retail 0x00555B90: within 30 units (80 vertical) of the arrival pos.
    if ( self->arrivalInfo.animscriptOverrideRunTo
        && Actor_PointNearPoint(myPos, self->arrivalInfo.animscriptOverrideRunToPos, 30.0f) )
        return false;
    if ( !Actor_CanSeeEnemy(self) )
        return false;

    const path_t *path = &self->Path;
    float lookaheadPos[2];
    lookaheadPos[0] = path->lookaheadDir[0] * path->fLookaheadDist + myPos[0];
    lookaheadPos[1] = path->lookaheadDir[1] * path->fLookaheadDist + myPos[1];
    const float dx = lookaheadPos[0] - enemyPos[0];
    const float dy = lookaheadPos[1] - enemyPos[1];
    if ( path->lookaheadDir[1] * dy + path->lookaheadDir[0] * dx > 0.0f
        && path->fLookaheadDist * path->fLookaheadDist > dy * dy + dx * dx )
    {
        if ( enemyKnown && !self->useEnemyGoal )
        {
            self->useEnemyGoal = true;
            Actor_UpdateGoalPos(self);
        }
        return true;
    }

    if ( Actor_PointAtGoal(myPos, &self->codeGoal) && Actor_CanSeeEnemy(self) )
    {
        if ( !node )
            return true;
        const float nx = node->constant.vOrigin[0] - enemyPos[0];
        const float ny = node->constant.vOrigin[1] - enemyPos[1];
        if ( Path_DistanceGreaterThan(&self->Path, sqrtf(nx * nx + ny * ny)) )
            return true;
    }

    if ( !enemyKnown
        || self->useEnemyGoal
        || Actor_PointAtGoal(myPos, &self->codeGoal)
        || !Actor_IsEnemyAlongPath_SP(self, enemyPos, lookaheadPos, hadPath) )
    {
        return false;
    }

    self->useEnemyGoal = true;
    Actor_UpdateGoalPos(self);
    pathnode_t *claimed = self->sentient->pClaimedNode;
    if ( claimed && Actor_PointAtGoal(claimed->constant.vOrigin, &self->codeGoal) )
        return node != claimed;
    if ( node && Actor_PointAtGoal(node->constant.vOrigin, &self->codeGoal) )
        return false;
    return true;
}

// Retail 0x007BDA90: path to the animscript-requested arrival position,
// restoring the previous path when that fails.
static const dvar_t *ai_coverScore_coverType;
static const dvar_t *ai_coverScore_distance;
static const dvar_t *ai_coverScore_engagement;
static const dvar_t *ai_coverScore_nodeAngle;
static const dvar_t *ai_coverScore_playerLos;
static const dvar_t *ai_coverScore_priority;
static const dvar_t *ai_coverScore_targetDir;
static const dvar_t *ai_coverScore_visibility;
static const dvar_t *ai_coverScore_flanking;
const dvar_t *ai_showSuppression;
static const dvar_t *ai_coverFlankMaxAngle;
static const dvar_t *ai_coverFlankDistToCover;
static const dvar_t *ai_coverFlankCheckRad;
static float s_coverFlankTraceHeight = 15.0f;

void G_RegisterHumanCoverDvars_SP()
{
    ai_showSuppression = _Dvar_RegisterInt("ai_showSuppression", -1, -1, 1023, 0x80u, "");
    ai_coverScore_coverType = _Dvar_RegisterFloat("ai_coverScore_coverType", 2.0f, 0.0f, 50.0f, 0x2080u, "");
    ai_coverScore_distance = _Dvar_RegisterFloat("ai_coverScore_distance", 16.0f, 0.0f, 50.0f, 0x2080u, "");
    ai_coverScore_engagement = _Dvar_RegisterFloat("ai_coverScore_engagement", 4.0f, 0.0f, 50.0f, 0x2080u, "");
    ai_coverScore_nodeAngle = _Dvar_RegisterFloat("ai_coverScore_nodeAngle", 4.0f, 0.0f, 50.0f, 0x2080u, "");
    ai_coverScore_playerLos = _Dvar_RegisterFloat("ai_coverScore_playerLos", 8.0f, 0.0f, 50.0f, 0x2080u, "");
    ai_coverScore_priority = _Dvar_RegisterFloat("ai_coverScore_priority", 8.0f, 0.0f, 50.0f, 0x2080u, "");
    ai_coverScore_targetDir = _Dvar_RegisterFloat("ai_coverScore_targetDir", 4.0f, 0.0f, 50.0f, 0x2080u, "");
    ai_coverScore_visibility = _Dvar_RegisterFloat("ai_coverScore_visibility", 8.0f, 0.0f, 50.0f, 0x2080u, "");
    ai_coverScore_flanking = _Dvar_RegisterFloat("ai_coverScore_flanking", 0.0f, 0.0f, 50.0f, 0x2080u, "");
    ai_coverFlankMaxAngle = _Dvar_RegisterFloat("ai_coverFlankMaxAngle", 70.0f, 0.0f, 360.0f, 0x80u, "");
    ai_coverFlankDistToCover = _Dvar_RegisterFloat("ai_coverFlankDistToCover", 180.0f, 0.0f, 360.0f, 0x80u, "");
    ai_coverFlankCheckRad = _Dvar_RegisterFloat("ai_coverFlankCheckRad", 6.0f, 0.0f, 60.0f, 0x80u, "");
}

static float Actor_Human_CoverDistance_SP(actor_s *self, const pathnode_t *node)
{
    const float dx = self->ent->r.currentOrigin[0] - node->constant.vOrigin[0];
    const float dy = self->ent->r.currentOrigin[1] - node->constant.vOrigin[1];
    const float dz = self->ent->r.currentOrigin[2] - node->constant.vOrigin[2];
    const float distanceSq = (dz * dz + dx * dx) + dy * dy;
    const float maximum = distanceSq - 1440000.0f >= 0.0f ? 1440000.0f : distanceSq;
    const float clamped = -distanceSq >= 0.0f ? 0.0f : maximum;
    return (1.0f - clamped * 6.944444521650439e-7f) * ai_coverScore_distance->current.value;
}

static float Actor_Human_CoverEngagement_SP(actor_s *self, const pathnode_t *node)
{
    const gentity_s *target = Actor_GetTargetEntity(self);
    if ( !target )
        return 0.0f;
    const float dx = target->r.currentOrigin[0] - node->constant.vOrigin[0];
    const float dy = target->r.currentOrigin[1] - node->constant.vOrigin[1];
    const float dz = target->r.currentOrigin[2] - node->constant.vOrigin[2];
    const float distance = std::sqrt((dx * dx + dz * dz) + dy * dy);
    if ( self->engageMinFalloffDist > distance || distance > self->engageMaxFalloffDist )
        return 0.0f;
    if ( self->engageMinDist > distance )
        return 1.0f - (self->engageMinDist - distance) / (self->engageMinDist - self->engageMinFalloffDist);
    if ( distance > self->engageMaxDist )
        return (self->engageMaxFalloffDist - distance) / (self->engageMaxFalloffDist - self->engageMaxDist);
    return 1.0f;
}

static float Actor_Human_CoverNodeAngle_SP(actor_s *self, const pathnode_t *node)
{
    const bool enabled = self->potentialThreat.isEnabled;
    float potential[2];
    if ( enabled )
    {
        potential[0] = self->potentialThreat.direction[0];
        potential[1] = self->potentialThreat.direction[1];
    }
    float angles[7];
    Actor_Human_NodeAngles_SP(node, angles);
    const gentity_s *target = Actor_GetTargetEntity(self);
    if ( target && (Actor_CanSeeEnemy(self) || !enabled) )
        return Actor_Human_NodeFacingPoint_SP(target->r.currentOrigin, node, angles) ? 1.0f : 0.0f;
    if ( !enabled )
        return 0.0f;
    const float point[3] = {
        node->constant.vOrigin[0] + potential[0],
        node->constant.vOrigin[1] + potential[1],
        node->constant.vOrigin[2]
    };
    return Actor_Human_NodeFacingPoint_SP(point, node, angles) ? 1.0f : 0.0f;
}

static float Actor_Human_CoverTargetDirection_SP(actor_s *self, const pathnode_t *node)
{
    const gentity_s *target = Actor_GetTargetEntity(self);
    if ( !target )
        return 0.0f;
    if ( Actor_PointNearNode(self->ent->r.currentOrigin, node) )
        return 1.0f;
    float dx = target->r.currentOrigin[0] - node->constant.vOrigin[0];
    float dy = target->r.currentOrigin[1] - node->constant.vOrigin[1];
    float length = std::sqrt(dx * dx + dy * dy);
    if ( -length >= 0.0f )
        length = 1.0f;
    const float inverse = 1.0f / length;
    dx *= inverse;
    dy *= inverse;
    float nx = node->constant.vOrigin[0] - self->ent->r.currentOrigin[0];
    float ny = node->constant.vOrigin[1] - self->ent->r.currentOrigin[1];
    length = std::sqrt(ny * ny + nx * nx);
    if ( -length >= 0.0f )
        length = 1.0f;
    const float inverseNode = 1.0f / length;
    return (((ny * inverseNode) * dy + (nx * inverseNode) * dx) + 1.0f) * 0.5f;
}

static bool Actor_Human_FlankTrace_SP(const float *start, const float *end, float angle, float distance, float radius)
{
    col_context_t context;
    const float dx = end[0] - start[0];
    const float dy = end[1] - start[1];
    const float dz = end[2] - start[2];
    const float length = std::sqrt((dy * dy + dx * dx) + dz * dz);
    if ( !(length > 0.1f) )
        return false;
    const float inverse = 1.0f / length;
    const float direction[3] = { dx * inverse, dy * inverse, dz * inverse };
    if ( distance - length >= 0.0f )
        distance = length;
    const float from[3] = { start[0], start[1], start[2] + s_coverFlankTraceHeight };
    const float to[3] = {
        direction[0] * distance + start[0], direction[1] * distance + start[1],
        direction[2] * distance + start[2] + s_coverFlankTraceHeight
    };
    const float mins[3] = { -radius, -radius, -radius };
    const float maxs[3] = { radius, radius, radius };
    trace_t trace;
    G_TraceCapsule(&trace, from, mins, maxs, to, 1023, 1, &context);
    if ( !(1.0f > trace.fraction) )
        return false;
    const float nx = trace.normal.vec.v[0];
    const float ny = trace.normal.vec.v[1];
    const float nz = trace.normal.vec.v[2];
    const float normalSq = (ny * ny + nz * nz) + nx * nx;
    if ( !(normalSq > 0.0f) )
        return true;
    const float dot = -((ny * direction[1] + nz * direction[2]) + nx * direction[0]);
    const float radians = angle * 0.01745329238474369f;
    return dot > static_cast<float>(std::cos(static_cast<double>(radians)));
}

static float Actor_Human_CoverFlanking_SP(actor_s *self, const pathnode_t *node)
{
    const gentity_s *target = Actor_GetTargetEntity(self);
    if ( !target )
        return 0.0f;
    return Actor_Human_FlankTrace_SP(target->r.currentOrigin, node->constant.vOrigin,
        ai_coverFlankMaxAngle->current.value, ai_coverFlankDistToCover->current.value,
        ai_coverFlankCheckRad->current.value) ? 0.0f : 1.0f;
}

static float Actor_Human_CoverScore_SP(actor_s *self, const pathnode_t *node)
{
    const float nodeAngle = Actor_Human_CoverNodeAngle_SP(self, node);
    const float distance = Actor_Human_CoverDistance_SP(self, node);
    const float engagement = Actor_Human_CoverEngagement_SP(self, node);
    float score = ai_coverScore_engagement->current.value * engagement + distance;
    sentient_s *enemy = Actor_GetTargetSentient(self);
    pathnode_t *enemyNode = enemy ? Sentient_NearestNode(enemy) : NULL;
    const float visible = enemyNode && !Path_NodesVisible(node, enemyNode) ? 0.0f : 1.0f;
    score = ai_coverScore_visibility->current.value * visible + score;
    const float coverType = ((1u << (static_cast<unsigned int>(node->constant.type) & 31u)) & 0x3800u) ? 0.0f : 1.0f;
    score = ai_coverScore_coverType->current.value * coverType + score;
    score += ai_coverScore_nodeAngle->current.value * nodeAngle;
    score = Actor_Human_CoverTargetDirection_SP(self, node) * ai_coverScore_targetDir->current.value + score;
    const float playerLos = node->dynamic.inPlayerLOSTime >= level.time && self->sentient->eTeam == 2 ? 0.0f : 1.0f;
    score = ai_coverScore_playerLos->current.value * playerLos + score;
    const float priority = (node->constant.spawnflags & 0x40) ? 1.0f : 0.0f;
    score = ai_coverScore_priority->current.value * priority + score;
    if ( nodeAngle > 0.0f && ai_coverScore_flanking->current.value > 0.0f )
        score = Actor_Human_CoverFlanking_SP(self, node) * ai_coverScore_flanking->current.value + score;
    return score;
}

float Actor_Human_GrenadeCoverScore_SP(actor_s *self, const pathnode_t *node)
{
    const float dx = self->ent->r.currentOrigin[0] - node->constant.vOrigin[0];
    const float dy = self->ent->r.currentOrigin[1] - node->constant.vOrigin[1];
    const float dz = self->ent->r.currentOrigin[2] - node->constant.vOrigin[2];
    const float lengthSq = (dx * dx + dy * dy) + dz * dz;
    const float upper = lengthSq - 1440000.0f >= 0.0f ? 1440000.0f : lengthSq;
    const float clamped = -lengthSq >= 0.0f ? 0.0f : upper;
    const float distance = (1.0f - clamped * 6.944444521650439e-7f)
        * ai_coverScore_distance->current.value;
    float visible = 1.0f;
    sentient_s *enemy = Actor_GetTargetSentient(self);
    if ( enemy )
    {
        const pathnode_t *enemyNode = Sentient_NearestNode(enemy);
        if ( enemyNode && !Path_NodesVisible(node, enemyNode) )
            visible = 0.0f;
    }
    const float cover = ((1u << node->constant.type) & 0x3800u) ? 0.0f : 1.0f;
    float score = ai_coverScore_coverType->current.value * cover
        + (ai_coverScore_visibility->current.value * visible + distance);
    score = Actor_Human_CoverNodeAngle_SP(self, node) * ai_coverScore_nodeAngle->current.value + score;
    const float priority = node->constant.spawnflags & 0x40u ? 1.0f : 0.0f;
    return ai_coverScore_priority->current.value * priority + score;
}

bool Actor_Human_CoverUsable_SP(actor_s *self, pathnode_t *node)
{
    if ( !Path_CanClaimNode(node, self->sentient) )
        return false;
    scr_animscript_t *script;
    sentient_s *enemy = Actor_GetTargetSentient(self);
    if ( enemy )
    {
        Sentient_NearestNode(enemy);
        const gentity_s *target = Actor_GetTargetEntity(self);
        if ( target )
        {
            const float dx = target->r.currentOrigin[0] - node->constant.vOrigin[0];
            const float dy = target->r.currentOrigin[1] - node->constant.vOrigin[1];
            if ( 0.0f > node->constant.forward[1] * dy + node->constant.forward[0] * dx )
                return false;
        }
        if ( Actor_Human_NodeAnimUsable_SP(self, node, true, &script) && !script )
            return false;
    }
    else if ( Actor_Human_NodeAnim_SP(self, node, &script) && !script )
        return false;
    return true;
}

static int Actor_Human_FilterCover_SP(actor_s *self, pathsort_t *nodes, int count, gentity_s *volume)
{
    for ( int i = 0; i < count; )
    {
        pathnode_t *node = nodes[i].node;
        if ( !Actor_Human_CoverUsable_SP(self, node)
            || (volume && !SV_EntityContact(node->constant.vOrigin, node->constant.vOrigin, volume)) )
        {
            nodes[i] = nodes[--count];
            continue;
        }
        nodes[i].distMetric = Actor_Human_CoverDistance_SP(self, node);
        nodes[i].metric = Actor_Human_CoverScore_SP(self, node);
        ++i;
    }
    return count;
}

static int __cdecl Actor_Human_CoverCompare_SP(const void *left, const void *right)
{
    const float difference = static_cast<const pathsort_t *>(left)->metric - static_cast<const pathsort_t *>(right)->metric;
    if ( 0.0f > difference )
        return -1;
    if ( difference > 0.0f )
        return 1;
    return 0;
}

static int Actor_Human_FindCover_SP(actor_s *self, pathnode_t **output, int requested)
{
    pathsort_t nodes[256];
    int count = Path_NodesInCylinder(self->codeGoal.pos, self->codeGoal.radius, self->codeGoal.height, nodes, 256, 0x83ffcu);
    count = Actor_Human_FilterCover_SP(self, nodes, count, self->codeGoal.volume);
    self->numCoverNodesInGoal = count;
    for ( int i = 0; i < count; ++i )
        nodes[i].metric -= nodes[i].distMetric;
    qsort(nodes, count, sizeof(pathsort_t), Actor_Human_CoverCompare_SP);
    for ( int i = 0; i < count; ++i )
        if ( nodes[i].node != self->sentient->pClaimedNode || i == count - 1 )
            nodes[i].metric = nodes[i].distMetric + nodes[i].metric;
    qsort(nodes, count, sizeof(pathsort_t), Actor_Human_CoverCompare_SP);
    const int first = count > requested ? count - requested : 0;
    int result = 0;
    for ( int i = first; i < count; ++i )
        output[result++] = nodes[i].node;
    return result;
}

static bool Actor_Human_NodeInsideGoal_SP(const pathnode_t *node, const actor_goal_s *goal)
{
    const float dz = node->constant.vOrigin[2] - goal->pos[2];
    if ( dz * dz > goal->height * goal->height )
        return false;
    const float dy = goal->pos[1] - node->constant.vOrigin[1];
    const float dx = goal->pos[0] - node->constant.vOrigin[0];
    if ( dy * dy + dx * dx > goal->radius * goal->radius )
        return false;
    return !goal->volume || SV_EntityContact(node->constant.vOrigin, node->constant.vOrigin, goal->volume) != 0;
}

static pathnode_t *Actor_Human_SelectCover_SP(actor_s *self)
{
    pathnode_t *claimed = self->sentient->pClaimedNode;
    bool claimedInGoal = false;
    if ( claimed && Actor_Human_NodeInsideGoal_SP(claimed, &self->codeGoal) )
    {
        claimedInGoal = true;
        if ( Actor_Human_CoverUsable_SP(self, claimed) )
            return claimed;
    }
    if ( !self->sentient->targetEnt.isDefined() && self->codeGoal.node )
    {
        pathnode_t *node = self->codeGoal.node;
        return Actor_Human_CoverUsable_SP(self, node)
            && Actor_PointAtGoal(node->constant.vOrigin, &self->codeGoal) ? node : NULL;
    }
    pathnode_t *best;
    if ( Actor_Human_FindCover_SP(self, &best, 1) )
        return best;
    return claimedInGoal ? claimed : NULL;
}


static void Actor_TryPathToArrivalPos_SP(actor_s *self)
{
    static path_t pathBackup; // retail 0x01A505F0

    memcpy(&pathBackup, &self->Path, sizeof(path_t));
    if ( !Actor_FindPath(self, self->arrivalInfo.animscriptOverrideRunToPos, 1, false) )
    {
        Actor_ClearPath(self);
        memcpy(&self->Path, &pathBackup, sizeof(path_t));
        if ( self->Path.wPathLen > 0 && self->Path.wNegotiationStartNode > 0 )
            ++Path_ConvertIndexToNode(self->Path.pts[self->Path.wNegotiationStartNode].iNodeNum)->dynamic.userCount;
        self->arrivalInfo.animscriptOverrideRunTo = 0;
        self->arrivalInfo.arrivalNotifyRequested = 0;
    }
}

// Retail 0x00488D70: per-think goal and path maintenance for the exposed
// state (repath throttle on iTeamMoveWaitTime, bad-place goals, goal_changed
// notify, enemy-goal switching and the stop-for-enemy decision).
static void Actor_FindPathToGoal_SP(actor_s *self)
{
    pathnode_t *node = NULL; // species-0 candidate selection not reconstructed

    if ( self->ent->tagInfo )
    {
        Actor_ClearPath(self);
        return;
    }
    const int hadPath = Actor_HasPath(self);
    if ( !hadPath && level.time < self->iTeamMoveWaitTime )
        return;

    if ( self->badPlaceAwareness > 0.0f && Actor_IsGoalInAnyBadPlace_SP(self->codeGoal.pos) )
        goto clear_path;

    if ( self->fixedNode )
    {
        // Retail tail-calls the fixed-node controller 0x00667C80 (not
        // reconstructed; zombies never use fixed nodes).  Keep the previous
        // bounded behaviour for that case.
        self->useEnemyGoal = false;
        Actor_UpdateGoalPos(self);
        Actor_FindPathToGoalDirect(self);
        return;
    }

    {
        const bool enemyKnown = Actor_EnemyPosRecentlyKnown_SP(self, hadPath);
        if ( !enemyKnown )
            self->useEnemyGoal = false;

        const float prevGoal[3] = { self->codeGoal.pos[0], self->codeGoal.pos[1], self->codeGoal.pos[2] };
        Actor_UpdateGoalPos(self);
        self->goalPosChanged = self->codeGoal.pos[0] != prevGoal[0]
                            || self->codeGoal.pos[1] != prevGoal[1]
                            || self->codeGoal.pos[2] != prevGoal[2];
        if ( self->goalPosChanged )
            Scr_Notify(self->ent, scr_const.goal_changed, 0);

        if ( Actor_KeepClaimedNode(self) )
            goto clear_path;

        sentient_s *sentient = self->sentient;
        pathnode_t *prevClaimed = sentient->pClaimedNode;

        if ( self->arrivalInfo.animscriptOverrideRunTo )
        {
            Actor_TryPathToArrivalPos_SP(self);
        }
        else if ( !self->useEnemyGoal )
        {
            Actor_FindPathToGoalDirect(self);
        }
        else
        {
            // Retail 0x00489022: with no candidate node the enemy goal is
            // dropped again and any path to it discarded.
            self->useEnemyGoal = false;
            Actor_UpdateGoalPos(self);
            if ( Actor_HasPath(self) )
            {
                Actor_ClearPath(self);
                Actor_TeamMoveBlocked(self);
            }
        }

        if ( Actor_CheckStopForEnemy_SP(self, enemyKnown, node, hadPath) )
        {
            // Retail 0x00488E20 (unconditional clear + block here).
            Actor_ClearPath(self);
            Actor_TeamMoveBlocked(self);
            return;
        }

        if ( node || Actor_HasPath(self) )
            Sentient_ClaimNode(sentient, node);
        if ( self->goalPosChanged )
            return;
        if ( sentient->pClaimedNode && prevClaimed != sentient->pClaimedNode )
        {
            self->goalPosChanged = true;
            Scr_Notify(self->ent, scr_const.goal_changed, 0);
        }
        return;
    }

clear_path:
    if ( Actor_HasPath(self) )
    {
        Actor_ClearPath(self);
        Actor_TeamMoveBlocked(self);
    }
}

static bool Actor_Human_NearFixedGoal_SP(const float *point, const float *goal)
{
    const float dz = goal[2] - point[2];
    if ( dz * dz > 6400.0f )
        return false;
    const float dy = point[1] - goal[1];
    const float dx = point[0] - goal[0];
    return dy * dy + dx * dx <= 900.0f;
}

static bool Actor_Human_HoldingFixedGoal_SP(actor_s *self)
{
    gentity_s *target = self->sentient->targetEnt.isDefined()
        ? self->sentient->targetEnt.ent() : NULL;
    if ( !target )
        return false;
    if ( target->sentient
        && level.time - self->sentientInfo[target->sentient - level.sentients].lastKnownPosTime > 10000 )
        return false;

    const float remaining = 64.0f - self->codeGoal.radius;
    const float radius = remaining >= 0.0f ? self->codeGoal.radius : 64.0f;
    if ( self->pAnimScriptFunc != &g_scr_data.humanAnim.combat )
        return false;
    const float dz = self->ent->r.currentOrigin[2] - self->codeGoal.pos[2];
    if ( dz * dz > 6400.0f )
        return false;
    const float dy = self->codeGoal.pos[1] - self->ent->r.currentOrigin[1];
    const float dx = self->codeGoal.pos[0] - self->ent->r.currentOrigin[0];
    return dy * dy + dx * dx <= radius * radius;
}

static bool Actor_Human_FixedGoalSafe_SP(actor_s *self)
{
    if ( Actor_HasPath(self) )
    {
        if ( Actor_Human_NearFixedGoal_SP(self->Path.vFinalGoal, self->codeGoal.pos)
            || (self->arrivalInfo.animscriptOverrideRunTo
                && Actor_Human_NearFixedGoal_SP(self->Path.vFinalGoal,
                    self->arrivalInfo.animscriptOverrideRunToPos)) )
            return true;
    }
    else if ( Actor_Human_NearFixedGoal_SP(self->ent->r.currentOrigin, self->codeGoal.pos) )
    {
        self->commitToFixedNode = false;
        return true;
    }

    pathnode_t *node = self->codeGoal.node;
    if ( node && !Path_CanClaimNode(node, self->sentient) )
    {
        if ( node->dynamic.pOwner.isDefined() )
        {
            Scr_AddEntity(node->dynamic.pOwner.sentient()->ent, SCRIPTINSTANCE_SERVER);
            Scr_Notify(self->ent, scr_const.node_taken, 1);
        }
        return false;
    }
    gentity_s *enemy = Actor_IsKnownEnemyInRegion(self,
        self->fixedNodeSafeVolume.isDefined() ? self->fixedNodeSafeVolume.ent() : NULL,
        self->codeGoal.pos, self->fixedNodeSafeRadius);
    if ( !enemy )
        return true;
    Scr_AddEntity(enemy, SCRIPTINSTANCE_SERVER);
    Scr_Notify(self->ent, scr_const.node_not_safe, 1);
    return false;
}

static bool Actor_Human_FindPathToNode_SP(actor_s *self, pathnode_t *node)
{
    const bool hadPath = Actor_HasPath(self);
    if ( !hadPath && level.time < self->pathWaitTime )
    {
        Actor_ClearPileUp(self);
        return false;
    }
    const bool validPath = Actor_FindPathToNode(self, node, 1, 0);
    if ( !Actor_HasPath(self) )
    {
        Actor_ClearPileUp(self);
        if ( !validPath )
            Actor_HandleInvalidPath(self);
        return validPath;
    }
    if ( !hadPath && ((1u << (static_cast<unsigned int>(node->constant.type) & 31)) & 0x3C0u) == 0 )
    {
        const float dz = self->ent->r.currentOrigin[2] - node->constant.vOrigin[2];
        const float dy = node->constant.vOrigin[1] - self->ent->r.currentOrigin[1];
        const float dx = node->constant.vOrigin[0] - self->ent->r.currentOrigin[0];
        if ( !(dz * dz > 6400.0f) && dy * dy + dx * dx <= 225.0f
            && Actor_PointAtGoal(self->ent->r.currentOrigin, &self->codeGoal) )
        {
            Actor_ClearPath(self);
            Actor_ClearPileUp(self);
        }
    }
    return true;
}

static void Actor_Human_FindPathToFixedGoal_SP(actor_s *self)
{
    self->useEnemyGoal = false;
    const float previous[3] = { self->codeGoal.pos[0], self->codeGoal.pos[1], self->codeGoal.pos[2] };
    Actor_UpdateGoalPos(self);
    self->goalPosChanged = self->codeGoal.pos[0] != previous[0]
        || self->codeGoal.pos[1] != previous[1] || self->codeGoal.pos[2] != previous[2];
    if ( self->goalPosChanged )
    {
        Scr_Notify(self->ent, scr_const.goal_changed, 0);
        self->commitToFixedNode = false;
    }
    if ( Actor_Human_HoldingFixedGoal_SP(self) )
        return;
    if ( self->keepClaimedNode )
    {
        if ( Actor_HasPath(self) )
        {
            Actor_ClearPath(self);
            Actor_TeamMoveBlocked(self);
        }
        return;
    }
    if ( self->keepClaimedNodeInGoal )
    {
        pathnode_t *claimed = self->sentient->pClaimedNode;
        if ( claimed && Actor_PointAtGoal(claimed->constant.vOrigin, &self->codeGoal) )
        {
            if ( Actor_HasPath(self) )
            {
                Actor_ClearPath(self);
                Actor_TeamMoveBlocked(self);
            }
            return;
        }
        self->keepClaimedNodeInGoal = false;
    }
    if ( Actor_UsingCoverAnim_SP(self) )
        self->Path.flags |= 0x100;

    pathnode_t *node = self->codeGoal.node;
    pathnode_t *previousClaimed = self->sentient->pClaimedNode;
    if ( !Actor_Human_FixedGoalSafe_SP(self) && !self->commitToFixedNode )
    {
        Actor_ClearPath(self);
        Actor_TeamMoveBlocked(self);
        return;
    }
    if ( self->arrivalInfo.animscriptOverrideRunTo )
    {
        Actor_TryPathToArrivalPos_SP(self);
    }
    else if ( node )
    {
        if ( !Actor_Human_FindPathToNode_SP(self, node) )
        {
            Actor_TeamMoveBlocked(self);
            node = NULL;
        }
        else if ( !self->commitToFixedNode && Actor_HasPath(self) )
        {
            self->commitToFixedNode = true;
            Scr_Notify(self->ent, scr_const.start_move, 0);
        }
    }
    else
    {
        Actor_FindPathToGoalDirect(self);
        if ( Actor_HasPath(self) && !self->commitToFixedNode )
        {
            self->commitToFixedNode = true;
            Scr_Notify(self->ent, scr_const.start_move, 0);
        }
    }
    if ( node ? Path_CanClaimNode(node, self->sentient) : Actor_HasPath(self) )
        Sentient_ClaimNode(self->sentient, node);
    if ( !self->goalPosChanged && self->sentient->pClaimedNode
        && previousClaimed != self->sentient->pClaimedNode )
    {
        self->goalPosChanged = true;
        Scr_Notify(self->ent, scr_const.goal_changed, 0);
        self->commitToFixedNode = false;
    }
}

static bool Actor_Human_ArrivalGoalNear_SP(const float *point, const float *goal)
{
    const double dx = static_cast<double>(goal[0]) - point[0];
    const double dy = static_cast<double>(goal[1]) - point[1];
    const double dz = static_cast<double>(goal[2]) - point[2];
    return (dx * dx + dz * dz) + dy * dy < 1.0;
}

pathnode_t *Actor_Human_FindBestCoverNode_SP(actor_s *self)
{
    pathnode_t *node;
    return Actor_Human_FindCover_SP(self, &node, 1) ? node : nullptr;
}

bool Actor_Human_UseCoverNode_SP(actor_s *self, pathnode_t *node)
{
    const ai_state_t state = self->eState[self->stateLevel];
    if ((state != AIS_EXPOSED && state != AIS_TURRET)
        || !Actor_Human_CoverUsable_SP(self, node))
        return false;
    if (!self->ent->tagInfo && !Actor_Human_FindPathToNode_SP(self, node))
    {
        Actor_TeamMoveBlocked(self);
        return false;
    }
    Sentient_ClaimNode(self->sentient, node);
    self->iPotentialCoverNodeCount = 0;
    if (self->eState[self->stateLevel] != AIS_EXPOSED)
        Actor_SetState(self, AIS_EXPOSED);
    Actor_SetSubState(self, STATE_EXPOSED_COMBAT);
    return true;
}

int Actor_CheckCoverExitPosWithPath_SP(actor_s *self, const float *exitPos)
{
    if (!Actor_HasPath(self))
        return 0;
    const float *origin = self->ent->r.currentOrigin;
    const float dx = exitPos[0] - origin[0];
    const float dy = exitPos[1] - origin[1];
    const float exitDistanceSq = dy * dy + dx * dx;
    float distanceLimitSq = 16384.0f;
    if (!(16384.0f - exitDistanceSq >= 0.0f))
        distanceLimitSq = exitDistanceSq;
    const int first = self->Path.wPathLen - 2;
    int last = self->Path.wPathLen - 3;
    if (last < self->Path.wNegotiationStartNode)
        last = self->Path.wNegotiationStartNode;
    int behindCount = 0;
    for (int i = first; i >= last; --i)
    {
        const float px = self->Path.pts[i].vOrigPoint[0] - origin[0];
        const float py = self->Path.pts[i].vOrigPoint[1] - origin[1];
        if (px * dx + py * dy < 0.0f && ++behindCount > 1)
            return 0;
        if (py * py + px * px > distanceLimitSq)
            return i != first || behindCount == 0;
    }
    return 1;
}

void Actor_Human_FindPathToGoal_SP(actor_s *self)
{
    const int previousArrivalOverride = self->arrivalInfo.animscriptOverrideRunTo;
    pathnode_t *node = NULL;
    if ( self->ent->tagInfo )
    {
        Actor_ClearPath(self);
        return;
    }
    const bool hadPath = Actor_HasPath(self);
    if ( !hadPath && level.time < self->iTeamMoveWaitTime )
        return;
    if ( self->badPlaceAwareness > 0.0f && Actor_IsGoalInAnyBadPlace_SP(self->codeGoal.pos) )
    {
        if ( Actor_HasPath(self) )
        {
            Actor_ClearPath(self);
            Actor_TeamMoveBlocked(self);
        }
        return;
    }
    if ( self->fixedNode )
    {
        Actor_Human_FindPathToFixedGoal_SP(self);
        return;
    }
    const bool enemyKnown = Actor_EnemyPosRecentlyKnown_SP(self, hadPath);
    if ( !enemyKnown )
        self->useEnemyGoal = false;
    const float previous[3] = { self->codeGoal.pos[0], self->codeGoal.pos[1], self->codeGoal.pos[2] };
    Actor_UpdateGoalPos(self);
    self->goalPosChanged = self->codeGoal.pos[0] != previous[0]
        || self->codeGoal.pos[1] != previous[1] || self->codeGoal.pos[2] != previous[2];
    if ( self->goalPosChanged )
        Scr_Notify(self->ent, scr_const.goal_changed, 0);
    if ( Actor_KeepClaimedNode(self) )
    {
        if ( Actor_HasPath(self) )
        {
            Actor_ClearPath(self);
            Actor_TeamMoveBlocked(self);
        }
        return;
    }
    if ( self->species == AI_SPECIES_HUMAN )
    {
        if ( self->eState[self->stateLevel] == AIS_EXPOSED
            && self->eSubState[self->stateLevel] == STATE_EXPOSED_COMBAT
            && level.time < self->exposedStartTime + self->exposedDuration
            && !self->arrivalInfo.animscriptOverrideRunTo )
            return;
        if ( Actor_UsingCoverAnim_SP(self) )
            self->Path.flags |= 0x100;
        node = Actor_Human_SelectCover_SP(self);
    }
    pathnode_t *previousClaimed = self->sentient->pClaimedNode;
    if ( node && previousArrivalOverride && !self->arrivalInfo.animscriptOverrideRunTo
        && node == previousClaimed && Actor_HasPath(self)
        && Actor_Human_ArrivalGoalNear_SP(self->Path.vFinalGoal, self->arrivalInfo.animscriptOverrideRunToPos) )
        self->arrivalInfo.animscriptOverrideRunTo = 1;
    if ( self->arrivalInfo.animscriptOverrideRunTo )
    {
        Actor_TryPathToArrivalPos_SP(self);
    }
    else if ( node )
    {
        if ( !Actor_Human_FindPathToNode_SP(self, node) )
        {
            if ( !Actor_HasActiveSuppression_SP(self) )
                Path_MarkNodeInvalid(node, self->sentient->eTeam);
            Actor_TeamMoveBlocked(self);
            node = NULL;
        }
        else if ( !Actor_HasPath(self) && self->useEnemyGoal )
        {
            if ( !Actor_PointAtGoal(self->ent->r.currentOrigin, &self->codeGoal)
                || !Actor_PointNearNode(self->ent->r.currentOrigin, node) )
            {
                self->useEnemyGoal = false;
                Actor_UpdateGoalPos(self);
            }
            Actor_TeamMoveBlocked(self);
        }
    }
    else if ( !self->useEnemyGoal )
    {
        Actor_FindPathToGoalDirect(self);
    }
    else
    {
        self->useEnemyGoal = false;
        Actor_UpdateGoalPos(self);
        if ( Actor_HasPath(self) )
        {
            Actor_ClearPath(self);
            Actor_TeamMoveBlocked(self);
        }
    }
    if ( Actor_CheckStopForEnemy_SP(self, enemyKnown, node, hadPath) )
    {
        Actor_ClearPath(self);
        Actor_TeamMoveBlocked(self);
        return;
    }
    if ( node || Actor_HasPath(self) )
        Sentient_ClaimNode(self->sentient, node);
    if ( !self->goalPosChanged && self->sentient->pClaimedNode
        && previousClaimed != self->sentient->pClaimedNode )
    {
        self->goalPosChanged = true;
        Scr_Notify(self->ent, scr_const.goal_changed, 0);
    }
}

// Retail 0x007D30D0: in combat with the enemy visible, should the remaining
// path be abandoned?
static bool Actor_Exposed_ShouldClearCombatPath_SP(actor_s *self)
{
    if ( self->pPileUpActor || self->pCloseEnt.isDefined() )
        return true;
    if ( !Actor_PointAtGoal(self->Path.vFinalGoal, &self->codeGoal) )
        return false;
    if ( self->sentient->pClaimedNode && !Actor_KeepClaimedNode(self) )
        return Actor_PointAt(self->ent->r.currentOrigin, self->sentient->pClaimedNode->constant.vOrigin);

    float shrink = self->codeGoal.radius * 0.5f;
    if ( shrink >= 64.0f )
        shrink = 64.0f;
    // Retail 0x004AC4E0: inside the goal cylinder shrunk by 'shrink'.
    const float *pos = self->ent->r.currentOrigin;
    const float dz = pos[2] - self->codeGoal.pos[2];
    if ( dz * dz > self->codeGoal.height * self->codeGoal.height )
        return false;
    const float dx = self->codeGoal.pos[0] - pos[0];
    const float dy = self->codeGoal.pos[1] - pos[1];
    const float r = self->codeGoal.radius - shrink;
    return dx * dx + dy * dy <= r * r;
}

// Retail 0x007C0E30: standing at the goal but well off its centre; switch to
// a code-driven stop so the actor can be nudged without a script change.
static void Actor_Exposed_CheckCodeStop_SP(actor_s *self)
{
    if ( self->useEnemyGoal
        || self->eAnimMode == AI_ANIM_MOVE_CODE
        || self->eAnimMode == AI_ANIM_USE_ANGLE_DELTAS
        || self->mayMoveTime + 1000 > level.time
        || Actor_KeepClaimedNode(self) )
    {
        return;
    }
    const float r = self->codeGoal.radius - 30.0f;
    if ( r > 0.0f )
    {
        const float dx = self->codeGoal.pos[0] - self->ent->r.currentOrigin[0];
        const float dy = self->codeGoal.pos[1] - self->ent->r.currentOrigin[1];
        if ( dx * dx + dy * dy < r * r )
            return;
    }
    self->moveMode = AI_MOVE_STOP;
    self->eAnimMode = AI_ANIM_MOVE_CODE;
}

// Retail 0x007D2D80: exposed substate transitions, run before dispatch.
static void Actor_Exposed_DecideSubState_SP(actor_s *self)
{
    if ( !Actor_GetTargetEntity(self) )
    {
        Actor_SetSubState(self, STATE_EXPOSED_NONCOMBAT);
        return;
    }
    const ai_substate_t subState = self->eSubState[self->stateLevel];
    if ( subState == STATE_EXPOSED_NONCOMBAT )
    {
        if ( Actor_EnemyRecentlyVisible_SP(self, 1) )
            Actor_SetSubState(self, STATE_EXPOSED_COMBAT);
    }
    else if ( subState == STATE_EXPOSED_REACQUIRE_MOVE )
    {
        if ( !Actor_HasPath(self) || self->pPileUpActor || self->pCloseEnt.isDefined() )
            Actor_SetSubState(self, STATE_EXPOSED_COMBAT);
        // Retail additionally requires a clear muzzle-to-enemy capsule trace
        // (0x0059BAA0, not reconstructed) before leaving the reacquire move
        // early; without it the move simply runs to the end of its path.
    }
}

// Retail 0x007D2E70: noncombat substate controller.
static void Actor_Exposed_NonCombat_SP(actor_s *self)
{
    const pathnode_t *prevClaimed = self->sentient->pClaimedNode;
    const bool prevClaimedValid = prevClaimed && (prevClaimed->constant.spawnflags & 0x8000) != 0;

    Actor_FindPathToGoal_SP(self);
    if ( Actor_HasPath(self) )
    {
        self->pszDebugInfo = "noncombat_move";
        const float *pos = self->ent->r.currentOrigin;
        const float dx = pos[0] - self->Path.vFinalGoal[0];
        const float dy = pos[1] - self->Path.vFinalGoal[1];
        const float dz = pos[2] - self->Path.vFinalGoal[2];
        // Run unless the remaining straight-line distance is under fWalkDist.
        Actor_MoveAlongPathWithTeam(
            self, self->fWalkDist * self->fWalkDist <= dx * dx + dy * dy + dz * dz, true, true);

        ai_orient_mode_t mode = AI_ORIENT_TO_MOTION;
        // Retail 0x007D2E30: a fresh (<2s) path shorter than 60 units into a
        // valid claimed node orients to the goal instead of to motion.
        if ( prevClaimedValid
            && level.time - self->Path.iPathTime < 2000
            && !Path_DistanceGreaterThan(&self->Path, 60.0f)
            && !self->arrivalInfo.animscriptOverrideRunTo )
        {
            mode = AI_ORIENT_TO_GOAL;
        }
        if ( Actor_IsAtGoal(self) && !prevClaimedValid )
            mode = AI_ORIENT_TO_ENEMY_OR_MOTION;
        Actor_SetOrientMode(self, mode);
        return;
    }

    self->pszDebugInfo = "noncombat_stop";
    if ( !Actor_IsAtGoal(self) )
    {
        if ( Actor_GetTargetEntity(self) )
            Actor_Zombie_ExposedCombat_SP(self);
        else
            Actor_AnimStop(self, Actor_GetStopAnim(self));
        Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY);
        return;
    }
    if ( prevClaimedValid )
    {
        Actor_AnimStop(self, Actor_GetStopAnim(self));
        Actor_SetOrientMode(self, AI_ORIENT_TO_GOAL);
        return;
    }
    Actor_Exposed_CheckCodeStop_SP(self);
    if ( Actor_GetTargetEntity(self) )
        Actor_Zombie_ExposedCombat_SP(self);
    else
        Actor_AnimStop(self, Actor_GetStopAnim(self));

    const float goalYaw = self->codeGoal.ang[1];
    if ( goalYaw == 0.0f )
    {
        Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY);
        return;
    }
    // Consume the script goal yaw (SetGoalPos( pos, angles )).
    self->bNotifyTurnDone = 1;
    self->ScriptOrient.eMode = AI_ORIENT_DONT_CHANGE;
    Actor_SetDesiredAngles(&self->ScriptOrient, 0.0f, goalYaw);
    self->codeGoal.ang[1] = 0.0f;
    self->scriptGoal.ang[1] = 0.0f;
}

bool __fastcall Actor_Exposed_Start_SP(actor_s *self, ai_state_t)
{
    if ( self->allowPitch )
    {
        self->ProneInfo.prone = true;
        self->ProneInfo.orientPitch = true;
        self->ProneInfo.iProneTime = level.time;
        self->ProneInfo.iProneTrans = 500;
    }
    Actor_SetSubState(self, STATE_EXPOSED_COMBAT);
    return true;
}

void __fastcall Actor_Exposed_Finish_SP(actor_s *self, ai_state_t)
{
    if ( self->allowPitch )
    {
        self->ProneInfo.prone = false;
        self->ProneInfo.orientPitch = false;
        self->ProneInfo.fTorsoPitch = 0.0f;
    }
}

bool __fastcall Actor_Exposed_Resume_SP(actor_s *self, ai_state_t)
{
    if ( self->allowPitch )
    {
        self->ProneInfo.prone = true;
        self->ProneInfo.orientPitch = true;
        self->ProneInfo.iProneTime = level.time;
        self->ProneInfo.iProneTrans = 500;
    }
    return true;
}

actor_think_result_t __fastcall Actor_Exposed_Think_SP(actor_s *self)
{
    // Retail zombie exposed think is 0x005591f0. This bounded controller
    // preserves the existing goal-maintenance fallback; full SP substate,
    // visibility/path-clear and fixed-node orientation parity remains open.
    // The combat fallback below now follows the reviewed zombie selector
    // at 0x00426710, reached through 0x007c0ef0.
    self->pszDebugInfo = "exposed";
    if ( self->allowPitch )
        Actor_OrientPitchToGround(self->ent, 1);

    Actor_PreThink(self);
    Actor_Exposed_DecideSubState_SP(self);
    switch ( self->eSubState[self->stateLevel] )
    {
    case STATE_EXPOSED_COMBAT:
        self->pszDebugInfo = "exposed_combat";
        // Retail 0x00559245: full goal/path maintenance (0x00488D70).
        Actor_FindPathToGoal_SP(self);
        // Retail 0x0055924C..0x00559270: once the enemy is visible and the
        // path merely ends inside the goal, drop the path and fight.
        if ( Actor_HasPath(self) && Actor_CanSeeEnemy(self) && Actor_Exposed_ShouldClearCombatPath_SP(self) )
            Actor_ClearPath(self);
        if ( Actor_HasPath(self) && self->safeToChangeScript )
        {
            Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY_OR_MOTION);
            Actor_MoveAlongPathWithTeam(self, true, true, true);
        }
        else
        {
            Actor_Zombie_ExposedCombat_SP(self);
            // Retail 0x007C0EF0 -> 0x007C0E30.
            if ( Actor_IsAtGoal(self) && Actor_GetTargetSentient(self) )
                Actor_Exposed_CheckCodeStop_SP(self);
        }
        break;

    case STATE_EXPOSED_NONCOMBAT:
        Actor_Exposed_NonCombat_SP(self);
        break;

    case STATE_EXPOSED_REACQUIRE_MOVE:
        // Retail 0x005592F9 has no outer HasPath gate here; the substate
        // decision above already left this substate when the path vanished,
        // so the guard below is only defensive.
        self->pszDebugInfo = "exposed_reacquire_move";
        Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY_OR_MOTION_SIDESTEP);
        if ( Actor_HasPath(self) )
            Actor_MoveAlongPathWithTeam(self, true, false, true);
        break;

    default:
        // Retail only post-thinks for other substates (incl. flashbanged).
        break;
    }

    Actor_PostThink(self);
    return ACTOR_THINK_DONE;
}

bool Actor_Human_NodeAnim_SP(actor_s *self, const pathnode_t *node, scr_animscript_t **anim)
{
    AnimScriptList *scripts = g_animScriptTable[AI_SPECIES_HUMAN];
    switch ( node->constant.type )
    {
    case NODE_COVER_STAND: *anim = &scripts->cover_stand; break;
    case NODE_COVER_CROUCH:
    case NODE_COVER_CROUCH_WINDOW:
    case NODE_TURRET: *anim = &scripts->cover_crouch; break;
    case NODE_COVER_PRONE: *anim = &scripts->cover_prone; break;
    case NODE_COVER_RIGHT: *anim = &scripts->cover_right; break;
    case NODE_COVER_LEFT: *anim = &scripts->cover_left; break;
    case NODE_COVER_WIDE_RIGHT: *anim = &scripts->cover_wide_right; break;
    case NODE_COVER_WIDE_LEFT: *anim = &scripts->cover_wide_left; break;
    case NODE_COVER_PILLAR: *anim = &scripts->cover_pillar; break;
    case NODE_CONCEALMENT_STAND: *anim = &scripts->concealment_stand; break;
    case NODE_CONCEALMENT_CROUCH: *anim = &scripts->concealment_crouch; break;
    case NODE_CONCEALMENT_PRONE: *anim = &scripts->concealment_prone; break;
    case NODE_GUARD:
        *anim = (node->constant.spawnflags & 0x80) != 0
            && (!Actor_GetTargetEntity(self) || self->grenadeAwareness == 0.0f)
            ? &scripts->stop : &scripts->combat;
        break;
    default:
        *anim = NULL;
        return (node->constant.spawnflags & 0x8000) != 0;
    }
    return true;
}

void Actor_Human_NodeAngles_SP(const pathnode_t *node, float *angles)
{
    const float ordinary[7] = { 81225.0f, -45.0f, 45.0f, -25.0f, 25.0f, -35.0f, 35.0f };
    const float window[7] = { 81225.0f, -30.0f, 10.0f, -15.0f, 1.0f, -20.0f, 5.0f };
    const float prone[7] = { 640000.0f, -30.0f, 30.0f, -15.0f, 15.0f, -20.0f, 20.0f };
    const float right[7] = { 81225.0f, -60.0f, 14.0f, -10.0f, -1.0f, -35.0f, 5.0f };
    const float left[7] = { 81225.0f, -12.0f, 60.0f, 1.0f, 10.0f, -5.0f, 35.0f };
    const float concealed[7] = { 81225.0f, -30.0f, 30.0f, -15.0f, 15.0f, -20.0f, 20.0f };
    const float fallback[7] = { 0.0f, -55.0f, 55.0f, -25.0f, 25.0f, -35.0f, 35.0f };
    const float *values = fallback;
    switch ( node->constant.type )
    {
    case NODE_COVER_STAND:
    case NODE_COVER_CROUCH:
    case NODE_COVER_PILLAR:
    case NODE_CONCEALMENT_STAND:
    case NODE_CONCEALMENT_CROUCH: values = ordinary; break;
    case NODE_COVER_CROUCH_WINDOW: values = window; break;
    case NODE_COVER_PRONE: values = prone; break;
    case NODE_COVER_RIGHT:
    case NODE_COVER_WIDE_RIGHT: values = right; break;
    case NODE_COVER_LEFT:
    case NODE_COVER_WIDE_LEFT: values = left; break;
    case NODE_CONCEALMENT_PRONE: values = concealed; break;
    case NODE_TURRET:
        angles[0] = 81225.0f;
        if ( node->dynamic.turretEntNumber < 0 )
        {
            angles[1] = -45.0f;
            angles[2] = 45.0f;
        }
        else
        {
            const TurretInfo *turret = g_entities[node->dynamic.turretEntNumber].pTurretInfo;
            angles[1] = AngleNormalize180(turret->arcmin[1]);
            angles[2] = AngleNormalize180(turret->arcmax[1]);
        }
        angles[3] = angles[5] = angles[1];
        angles[4] = angles[6] = angles[2];
        return;
    default: break;
    }
    memcpy(angles, values, sizeof(ordinary));
}

bool Actor_Human_NodeFacingPoint_SP(const float *point, const pathnode_t *node, const float *angles)
{
    const float dy = point[1] - node->constant.vOrigin[1];
    const float dx = point[0] - node->constant.vOrigin[0];
    const float yaw = static_cast<float>(std::atan2(static_cast<double>(dy), static_cast<double>(dx)))
        * 57.2957763671875f;
    const float relative = AngleNormalize180(yaw - node->constant.fAngle);
    return relative >= angles[1] && angles[2] >= relative;
}

static bool Actor_Human_NodeEngagesPoint_SP(const float *point, const pathnode_t *node, const float *angles)
{
    const float dy = node->constant.vOrigin[1] - point[1];
    const float dx = node->constant.vOrigin[0] - point[0];
    if ( angles[0] > dy * dy + dx * dx )
        return false;
    return Actor_Human_NodeFacingPoint_SP(point, node, angles);
}

static bool Actor_Human_NodeAnimThreat_SP(actor_s *self, const pathnode_t *node, bool regardlessVisible)
{
    float angles[7];
    Actor_Human_NodeAngles_SP(node, angles);
    gentity_s *target = Actor_GetTargetEntity(self);
    sentient_s *enemy = Actor_GetTargetSentient(self);
    if ( enemy )
    {
        if ( (enemy->ent->flags & 4) != 0 || Actor_CheckIgnore(self->sentient, enemy) )
            return true;
        const sentient_info_t *info = &self->sentientInfo[enemy - level.sentients];
        if ( info->lastKnownPosTime <= 0 || (!regardlessVisible && !Actor_CanSeeEnemy(self)) )
            return true;
        return Actor_Human_NodeEngagesPoint_SP(enemy->ent->r.currentOrigin, node, angles)
            || Actor_Human_NodeEngagesPoint_SP(info->vLastKnownPos, node, angles);
    }
    if ( !target )
    {
        if ( !regardlessVisible || !self->pGrenade.isDefined() )
            return true;
        target = self->pGrenade.ent();
    }
    else if ( !regardlessVisible && !Actor_CanSeeEntity(self, target) )
    {
        return true;
    }
    return Actor_Human_NodeEngagesPoint_SP(target->r.currentOrigin, node, angles);
}

bool Actor_Human_NodeAnimUsable_SP(
    actor_s *self, const pathnode_t *node, bool regardlessVisible, scr_animscript_t **anim)
{
    if ( !Actor_Human_NodeAnim_SP(self, node, anim) )
        return false;
    if ( Actor_Human_NodeAnimThreat_SP(self, node, regardlessVisible) )
        return *anim != NULL;
    *anim = NULL;
    return true;
}

bool Actor_Human_AtClaimedNode_SP(actor_s *self)
{
    if ( Actor_KeepClaimedNode(self) )
        return true;
    const pathnode_t *node = self->sentient->pClaimedNode;
    const float dz = self->ent->r.currentOrigin[2] - node->constant.vOrigin[2];
    if ( dz * dz > 6400.0f )
        return false;
    const float dy = node->constant.vOrigin[1] - self->ent->r.currentOrigin[1];
    const float dx = node->constant.vOrigin[0] - self->ent->r.currentOrigin[0];
    return 225.0f >= dy * dy + dx * dx;
}

void Actor_Human_ExposedCombatAnim_SP(actor_s *self)
{
    Actor_CheckCollisions(self);
    Actor_ClearPileUp(self);
    self->bUseGoalWeight = false;
    const pathnode_t *node = self->sentient->pClaimedNode;
    if ( self->fixedNode )
    {
        if ( node != self->codeGoal.node )
            node = NULL;
        if ( level.time <= self->exposedStartTime + 2000 )
            goto combat;
    }
    if ( node && Actor_Human_AtClaimedNode_SP(self) )
    {
        scr_animscript_t *anim;
        if ( Actor_Human_NodeAnimUsable_SP(self, node, false, &anim) )
        {
            if ( anim )
            {
                Actor_SetAnimScript(self, anim, AI_MOVE_STOP, AI_ANIM_MOVE_CODE, AI_ANIM_FUNCTION_STOP);
                return;
            }
            if ( !self->fixedNode )
                Actor_NodeClaimRevoked(self, 5000);
        }
    }
    if ( self->fixedNode && self->pAnimScriptFunc != &g_animScriptTable[self->species]->combat )
        self->exposedStartTime = level.time;
combat:
    Actor_SetAnimScript(self, &g_animScriptTable[self->species]->combat, AI_MOVE_STOP,
        self->pCloseEnt.isDefined() ? AI_ANIM_MOVE_CODE : AI_ANIM_USE_BOTH_DELTAS,
        AI_ANIM_FUNCTION_COMBAT);
}

static void Actor_Human_ExposedCombat_SP(actor_s *self)
{
    Actor_Human_ExposedCombatAnim_SP(self);
    sentient_s *enemy = Actor_GetTargetSentient(self);
    if ( Actor_IsAtGoal(self) && enemy )
    {
        Actor_Exposed_CheckCodeStop_SP(self);
        const int seen = self->sentientInfo[enemy - level.sentients].VisCache.iLastVisTime;
        const pathnode_t *node = self->sentient->pClaimedNode;
        if ( (!seen || level.time - seen >= 10000) && node && (node->constant.spawnflags & 0x8000) != 0 )
        {
            scr_animscript_t *anim;
            if ( !Actor_Human_NodeAnimUsable_SP(self, node, true, &anim) || anim )
            {
                Actor_SetOrientMode(self, AI_ORIENT_TO_GOAL);
                return;
            }
        }
    }
    Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY);
}

static bool Actor_Human_ClearShot_SP(actor_s *self)
{
    float target[3], muzzle[3];
    Actor_GetTargetLookPosition(self, target);
    if ( !Actor_GetMuzzleInfo(self, muzzle, NULL) )
        return false;
    const float dx = target[0] - muzzle[0];
    const float dz = target[2] - muzzle[2];
    const float dy = target[1] - muzzle[1];
    if ( dx * dx + dz * dz + dy * dy > level.fFogOpaqueDistSqrd )
        return false;
    trace_t trace = {};
    col_context_t context;
    G_TraceCapsule(&trace, muzzle, vec3_origin, vec3_origin, target,
        self->ent->s.number, 0x200C861, &context);
    if ( trace.fraction == 1.0f )
        return true;
    const unsigned int hit = Trace_GetEntityHitId(&trace);
    if ( self->sentient->targetEnt.isDefined() && self->sentient->targetEnt.entnum() == hit )
        return true;
    const sentient_s *hitSentient = g_entities[hit].sentient;
    return hitSentient && hitSentient->eTeam != self->sentient->eTeam;
}

static void Actor_Human_ExposedDecideSubState_SP(actor_s *self)
{
    if ( self->flashBanged )
    {
        Actor_SetSubState(self, STATE_EXPOSED_FLASHBANGED);
        return;
    }
    if ( !Actor_GetTargetEntity(self) )
    {
        Actor_SetSubState(self, STATE_EXPOSED_NONCOMBAT);
        return;
    }
    const ai_substate_t substate = self->eSubState[self->stateLevel];
    if ( substate == STATE_EXPOSED_NONCOMBAT || substate == STATE_EXPOSED_FLASHBANGED )
    {
        if ( substate == STATE_EXPOSED_FLASHBANGED )
            Actor_SetSubState(self, STATE_EXPOSED_NONCOMBAT);
        if ( Actor_EnemyRecentlyVisible_SP(self, 1) )
            Actor_SetSubState(self, STATE_EXPOSED_COMBAT);
    }
    else if ( substate == STATE_EXPOSED_REACQUIRE_MOVE )
    {
        if ( !Actor_HasPath(self) || self->pPileUpActor || self->pCloseEnt.isDefined() )
            Actor_SetSubState(self, STATE_EXPOSED_COMBAT);
        if ( Actor_CanSeeEnemy(self) && Actor_Human_ClearShot_SP(self) )
        {
            Actor_SetSubState(self, STATE_EXPOSED_COMBAT);
            self->exposedStartTime = level.time;
        }
    }
}

static bool Actor_Human_NearPathEnd_SP(actor_s *self)
{
    if ( !Actor_HasPath(self) )
        return false;
    const int age = static_cast<int>(static_cast<unsigned int>(level.time)
        - static_cast<unsigned int>(self->Path.iPathTime));
    return age < 2000 && !Path_DistanceGreaterThan(&self->Path, 60.0f);
}

static void Actor_Human_ExposedNonCombat_SP(actor_s *self)
{
    const pathnode_t *prevClaimed = self->sentient->pClaimedNode;
    Actor_Human_FindPathToGoal_SP(self);
    if ( Actor_HasPath(self) )
    {
        self->pszDebugInfo = "noncombat_move";
        const float *pos = self->ent->r.currentOrigin;
        const float dz = pos[2] - self->Path.vFinalGoal[2];
        const float dx = pos[0] - self->Path.vFinalGoal[0];
        const float dy = pos[1] - self->Path.vFinalGoal[1];
        Actor_MoveAlongPathWithTeam(
            self, dz * dz + dx * dx + dy * dy >= self->fWalkDist * self->fWalkDist, true, true);

        ai_orient_mode_t mode = static_cast<ai_orient_mode_t>(ai_moveOrientMode->current.integer);
        if ( mode == AI_ORIENT_INVALID )
        {
            mode = prevClaimed && (prevClaimed->constant.spawnflags & 0x8000) != 0
                && Actor_Human_NearPathEnd_SP(self) && !self->arrivalInfo.animscriptOverrideRunTo
                ? AI_ORIENT_TO_GOAL : AI_ORIENT_TO_MOTION;
        }
        if ( Actor_IsAtGoal(self) && (!prevClaimed || (prevClaimed->constant.spawnflags & 0x8000) == 0) )
            mode = AI_ORIENT_TO_ENEMY_OR_MOTION;
        Actor_SetOrientMode(self, mode);
        return;
    }

    self->pszDebugInfo = "noncombat_stop";
    if ( !Actor_IsAtGoal(self) )
    {
        if ( Actor_GetTargetEntity(self) )
            Actor_Human_ExposedCombatAnim_SP(self);
        else
            Actor_AnimStop(self, Actor_GetStopAnim(self));
        Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY);
        return;
    }
    if ( prevClaimed && (prevClaimed->constant.spawnflags & 0x8000) != 0 )
    {
        Actor_AnimStop(self, Actor_GetStopAnim(self));
        Actor_SetOrientMode(self, AI_ORIENT_TO_GOAL);
        return;
    }
    Actor_Exposed_CheckCodeStop_SP(self);
    if ( Actor_GetTargetEntity(self) )
        Actor_Human_ExposedCombatAnim_SP(self);
    else
        Actor_AnimStop(self, Actor_GetStopAnim(self));

    const float goalYaw = self->codeGoal.ang[1];
    if ( goalYaw == 0.0f )
    {
        Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY);
        return;
    }
    self->bNotifyTurnDone = 1;
    self->ScriptOrient.eMode = AI_ORIENT_DONT_CHANGE;
    Actor_SetDesiredAngles(&self->ScriptOrient, 0.0f, goalYaw);
    self->codeGoal.ang[1] = 0.0f;
    self->scriptGoal.ang[1] = 0.0f;
}

actor_think_result_t __fastcall Actor_Human_Exposed_Think_SP(actor_s *self)
{
    self->pszDebugInfo = "exposed";
    if ( self->allowPitch )
        Actor_OrientPitchToGround(self->ent, 1);
    if ( self->pGrenade.isDefined() && !self->flashBanged )
    {
        Actor_SetState(self, AIS_GRENADE_RESPONSE);
        return ACTOR_THINK_REPEAT;
    }
    if ( self->aiBadPlace && !self->flashBanged )
    {
        Actor_SetState(self, AIS_BADPLACE_FLEE);
        return ACTOR_THINK_REPEAT;
    }
    if ( self->turretEnt )
    {
        if ( !self->turretEnt->active )
        {
            Actor_SetState(self, AIS_TURRET);
            return ACTOR_THINK_REPEAT;
        }
        self->turretEnt = NULL;
    }
    Actor_PreThink(self);
    Actor_Human_ExposedDecideSubState_SP(self);
    switch ( self->eSubState[self->stateLevel] )
    {
    case STATE_EXPOSED_COMBAT:
        self->pszDebugInfo = "exposed_combat";
        Actor_Human_FindPathToGoal_SP(self);
        if ( Actor_HasPath(self) && Actor_CanSeeEnemy(self) && Actor_Exposed_ShouldClearCombatPath_SP(self) )
            Actor_ClearPath(self);
        if ( Actor_HasPath(self) )
        {
            Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY_OR_MOTION);
            Actor_MoveAlongPathWithTeam(self, true, true, true);
        }
        else
            Actor_Human_ExposedCombat_SP(self);
        break;
    case STATE_EXPOSED_NONCOMBAT:
        Actor_Human_ExposedNonCombat_SP(self);
        break;
    case STATE_EXPOSED_REACQUIRE_MOVE:
        self->pszDebugInfo = "exposed_reacquire_move";
        Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY_OR_MOTION_SIDESTEP);
        Actor_MoveAlongPathWithTeam(self, true, false, true);
        break;
    case STATE_EXPOSED_FLASHBANGED:
        self->pszDebugInfo = "exposed_flashbanged";
        Actor_Exposed_FlashBanged(self);
        break;
    default: break;
    }
    Actor_PostThink(self);
    return ACTOR_THINK_DONE;
}
#endif

void __fastcall Actor_Exposed_FindReacquireNode(actor_s *self)
{
    if ( Actor_GetTargetEntity(self) && self->eState[self->stateLevel] == AIS_EXPOSED )
        self->iPotentialReacquireNodeCount = 0;
    else
        self->iPotentialReacquireNodeCount = 0;
}

pathnode_t *__fastcall Actor_Exposed_GetReacquireNode(actor_s *self)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 320, 0, "%s", "self") )
        __debugbreak();
    if ( !self->sentient
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 321, 0, "%s", "self->sentient") )
    {
        __debugbreak();
    }
    if ( Actor_GetTargetEntity(self) && self->eState[self->stateLevel] == AIS_EXPOSED )
    {
        if ( self->iPotentialReacquireNodeCount )
        {
            if ( Actor_HasPath(self) || level.time >= self->iTeamMoveWaitTime )
            {
                return self->pPotentialReacquireNode[--self->iPotentialReacquireNodeCount];
            }
            else
            {
                self->iPotentialReacquireNodeCount = 0;
                return 0;
            }
        }
        else
        {
            return 0;
        }
    }
    else
    {
        self->iPotentialReacquireNodeCount = 0;
        return 0;
    }
}

char __fastcall Actor_Exposed_UseReacquireNode(actor_s *self, pathnode_t *pNode)
{
    float vPoint[3]; // [esp+14h] [ebp-1Ch] BYREF
    gentity_s *targetEnt; // [esp+20h] [ebp-10h]
    float vFrom[3]; // [esp+24h] [ebp-Ch] BYREF

    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 355, 0, "%s", "self") )
        __debugbreak();
    if ( !self->sentient
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 356, 0, "%s", "self->sentient") )
    {
        __debugbreak();
    }
    if ( Actor_KeepClaimedNode(self)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp",
                    357,
                    0,
                    "%s",
                    "!Actor_KeepClaimedNode( self )") )
    {
        __debugbreak();
    }
    targetEnt = Actor_GetTargetEntity(self);
    if ( !targetEnt || self->eState[self->stateLevel] != AIS_EXPOSED )
        return 0;
    if ( !Path_CanClaimNode(pNode, self->sentient) )
        return 0;
    vFrom[0] = pNode->constant.vOrigin[0];
    vFrom[1] = pNode->constant.vOrigin[1];
    vFrom[2] = pNode->constant.vOrigin[2];
    vFrom[2] = vFrom[2] + 64.0;
    Actor_GetTargetLookPosition(self, vPoint);
    if ( !Actor_CanSeePointFrom(self, vFrom, vPoint, 0.0, targetEnt->s.number) )
        return 0;
    if ( !Actor_FindPathToNode(self, pNode, 1) )
        return 0;
    Sentient_ClaimNode(self->sentient, pNode);
    self->iPotentialReacquireNodeCount = 0;
    Actor_SetSubState(self, STATE_EXPOSED_REACQUIRE_MOVE);
    return 1;
}

char __fastcall Actor_Exposed_ReacquireStepMove(actor_s *self, float fDist)
{
    float *v4; // ecx
    float *currentOrigin; // [esp+28h] [ebp-80h]
    float fScale; // [esp+30h] [ebp-78h]
    float vStepDir[3]; // [esp+34h] [ebp-74h] BYREF
    float fYaw; // [esp+40h] [ebp-68h]
    float vTraceEndPos[3]; // [esp+44h] [ebp-64h] BYREF
    float stepheight; // [esp+50h] [ebp-58h]
    gentity_s *targetEnt; // [esp+54h] [ebp-54h]
    float vStartPos[3]; // [esp+58h] [ebp-50h] BYREF
    float forward[3]; // [esp+64h] [ebp-44h] BYREF
    float vEyePos[3]; // [esp+70h] [ebp-38h] BYREF
    int iOrder; // [esp+7Ch] [ebp-2Ch]
    int i; // [esp+80h] [ebp-28h]
    float vCheckPos[3]; // [esp+84h] [ebp-24h] BYREF
    float vEnemyPos[3]; // [esp+90h] [ebp-18h] BYREF
    float vMovePos[3]; // [esp+9Ch] [ebp-Ch] BYREF

    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 401, 0, "%s", "self") )
        __debugbreak();
    if ( !self->sentient
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 402, 0, "%s", "self->sentient") )
    {
        __debugbreak();
    }
    targetEnt = Actor_GetTargetEntity(self);
    if ( !targetEnt || self->eState[self->stateLevel] != AIS_EXPOSED )
        return 0;
    fYaw = self->fDesiredBodyYaw + 90.0;
    Actor_GetTargetLookPosition(self, vEnemyPos);
    YawVectors(fYaw, vStepDir, 0);
    forward[0] = targetEnt->r.currentOrigin[0];
    forward[1] = targetEnt->r.currentOrigin[1];
    forward[2] = targetEnt->r.currentOrigin[2];
    currentOrigin = self->ent->r.currentOrigin;
    forward[0] = forward[0] - *currentOrigin;
    forward[1] = forward[1] - currentOrigin[1];
    forward[2] = forward[2] - currentOrigin[2];
    if ( (float)((float)(forward[0] * forward[0]) + (float)(forward[1] * forward[1])) <= 1.0 )
        return 0;
    Vec2Normalize(forward);
    forward[2] = 0.0f;
    vStepDir[0] = forward[1];
    vStepDir[1] = -forward[0];
    vStepDir[2] = 0.0f;
    Actor_GetEyePosition(self, vEyePos);
    v4 = self->ent->r.currentOrigin;
    vStartPos[0] = *v4;
    vStartPos[1] = v4[1];
    vStartPos[2] = v4[2];
    if ( vStepDir[2] != 0.0
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 448, 0, "%s", "!vStepDir[2]") )
    {
        __debugbreak();
    }
    iOrder = G_rand() & 1;
    for ( i = 0; i < 2; ++i )
    {
        fScale = factor[iOrder ^ i] * fDist;
        vCheckPos[0] = (float)(fScale * vStepDir[0]) + vEyePos[0];
        vCheckPos[1] = (float)(fScale * vStepDir[1]) + vEyePos[1];
        vCheckPos[2] = vEyePos[2];
        if ( Actor_CanSeePointFrom(self, vCheckPos, vEnemyPos, self->fMaxSightDistSqrd, targetEnt->s.number) )
        {
            vMovePos[0] = (float)(fScale * vStepDir[0]) + vStartPos[0];
            vMovePos[1] = (float)(fScale * vStepDir[1]) + vStartPos[1];
            vMovePos[2] = vStartPos[2];
            stepheight = self->Physics.prone ? 10.0f : 18.0f;
            if ( Path_PredictionTrace(vStartPos, vMovePos, 1023, self->Physics.iTraceMask | 4, vTraceEndPos, stepheight, 1)
                && Actor_PointAtGoal(vTraceEndPos, &self->codeGoal) )
            {
                Actor_FindPath(self, vTraceEndPos, 0, 0);
                if ( Actor_HasPath(self) )
                {
                    Actor_SetSubState(self, STATE_EXPOSED_REACQUIRE_MOVE);
                    Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY);
                    return 1;
                }
            }
        }
    }
    return 0;
}

void __fastcall Actor_Exposed_FindReacquireDirectPath(actor_s *self, bool ignoreSuppression)
{
    sentient_s *enemy; // [esp+8h] [ebp-10h]
    float vEnemyPos[3]; // [esp+Ch] [ebp-Ch] BYREF

    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 512, 0, "%s", "self") )
        __debugbreak();
    if ( !self->sentient
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 513, 0, "%s", "self->sentient") )
    {
        __debugbreak();
    }
    if ( Actor_GetTargetEntity(self) && self->eState[self->stateLevel] == AIS_EXPOSED )
    {
        Actor_GetTargetPosition(self, vEnemyPos);
        enemy = Actor_GetTargetSentient(self);
        if ( enemy )
            Actor_FindPathToSentient(self, enemy, !ignoreSuppression);
        else
            Actor_FindPath(self, vEnemyPos, 1, ignoreSuppression);
        if ( !Actor_PointAtGoal(vEnemyPos, &self->codeGoal) )
            Actor_ClipPathToGoal(self);
        Actor_BeginTrimPath(self);
    }
    else
    {
        Actor_ClearPath(self);
    }
}

void __fastcall Actor_Exposed_FindReacquireProximatePath(actor_s *self, bool ignoreSuppression)
{
    float fWithinDistSqrd; // [esp+1Ch] [ebp-10h]
    float vEnemyPos[3]; // [esp+20h] [ebp-Ch] BYREF

    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 549, 0, "%s", "self") )
        __debugbreak();
    if ( !self->sentient
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 550, 0, "%s", "self->sentient") )
    {
        __debugbreak();
    }
    if ( Actor_GetTargetEntity(self) && self->eState[self->stateLevel] == AIS_EXPOSED )
    {
        Actor_GetTargetPosition(self, vEnemyPos);
        fWithinDistSqrd = Vec3DistanceSq(self->ent->r.currentOrigin, vEnemyPos) * 0.25;
        if ( fWithinDistSqrd > (float)(self->fMaxSightDistSqrd * 0.0625) )
            fWithinDistSqrd = self->fMaxSightDistSqrd * 0.0625;
        Actor_FindPathInGoalWithLOS(self, vEnemyPos, fWithinDistSqrd, ignoreSuppression);
        Actor_BeginTrimPath(self);
    }
    else
    {
        Actor_ClearPath(self);
    }
}

char __fastcall Actor_Exposed_StartReacquireMove(actor_s *self)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 585, 0, "%s", "self") )
        __debugbreak();
    if ( !self->sentient
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 586, 0, "%s", "self->sentient") )
    {
        __debugbreak();
    }
    self->TrimInfo.iIndex = 0;
    self->TrimInfo.iDelta = 0;
    if ( !Actor_GetTargetEntity(self) || self->eState[self->stateLevel] != AIS_EXPOSED )
        return 0;
    if ( !Actor_HasPath(self) )
        return 0;
    Actor_SetSubState(self, STATE_EXPOSED_REACQUIRE_MOVE);
    return 1;
}

void __fastcall Actor_Exposed_FlashBanged(actor_s *self)
{
    PROF_SCOPED("flashbanged");

    if ( !self->flashBanged
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp",
                    766,
                    0,
                    "%s",
                    "self->flashBanged") )
    {
        __debugbreak();
    }
    Actor_SetAnimScript(
        self,
        &g_animScriptTable[self->species]->flashed,
        AI_MOVE_STOP,
        AI_ANIM_USE_BOTH_DELTAS,
        AI_ANIM_FUNCTION_STOP);
    Actor_SetOrientMode(self, AI_ORIENT_DONT_CHANGE);
}

int __cdecl Path_IsValidClaimNode(const pathnode_t *node)
{
    if ( !node && !Assert_MyHandler("c:\\projects_pc\\cod\\codsrc\\src\\game\\pathnode.h", 224, 0, "%s", "node") )
        __debugbreak();
    return node->constant.spawnflags & 0x8000;
}

void __fastcall Actor_Exposed_Touch(actor_s *self, gentity_s *pOther)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 920, 0, "%s", "self") )
        __debugbreak();
    if ( !self->sentient
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 921, 0, "%s", "self->sentient") )
    {
        __debugbreak();
    }
    if ( !pOther
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_exposed.cpp", 922, 0, "%s", "pOther") )
    {
        __debugbreak();
    }
    if ( pOther->sentient )
        Actor_GetPerfectInfo(self, pOther->sentient);
}

