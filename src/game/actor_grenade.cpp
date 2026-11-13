#include "actor_grenade.h"
#include "g_missile.h"
#include <game_mp/g_main_mp.h>
#ifdef KISAK_SP
#include <game_mp/actor_mp.h>
#include <game_mp/g_utils_mp.h>
#include <bgame/bg_weapons.h>
#include <bgame/bg_weapons_def.h>
#include <game_mp/g_spawn_mp.h>
#include <game_mp/g_combat_mp.h>
#include <clientscript/cscr_vm.h>
#include <clientscript/scr_const.h>
#include "actor_turret.h"
#include "bullet.h"
#include "actor_navigation.h"
#include "actor_events.h"
#include <bgame/bg_misc.h>
#include <server/sv_world.h>
#include <qcommon/cm_trace.h>
#include <universal/com_math_anglevectors.h>
#include <clientscript/cscr_stringlist.h>
#include "sentient.h"
#include "actor_orientation.h"
#include <qcommon/dobj_management.h>
#include <xanim/xmodel_utils.h>
#include <xanim/dobj.h>
#include <cmath>
#include "actor_exposed.h"
#include "actor_state.h"
#include "actor_animapi.h"
#include "actor_team_move.h"
#include "actor_senses.h"
#include "actor_threat.h"
#include "g_debug.h"
#include <cstdlib>
#endif

#ifdef KISAK_SP
void Actor_Grenade_Predict_SP(gentity_s *grenade)
{
    if ( grenade->missile.predictedLandPos[0] != 0.0f
        || grenade->missile.predictedLandPos[1] != 0.0f
        || grenade->missile.predictedLandPos[2] != 0.0f )
        return;
    const int mask = grenade->clipmask;
    grenade->clipmask = mask & 0xFDFF3FFF;
    float landing[3];
    int rest;
    const int result = G_PredictMissile(grenade, grenade->nextthink - level.time,
        landing, 1, &rest);
    grenade->missile.predictedTimeAtRest = rest;
    const float *position = result ? landing : grenade->r.currentOrigin;
    for ( int i = 0; i < 3; ++i )
        grenade->missile.predictedLandPos[i] = position[i];
    grenade->clipmask = mask;
}

static bool Actor_Grenade_Aware_SP(actor_s *self)
{
    if ( self->grenadeAwareness == 0.0f )
        return false;
    if ( Actor_GetTargetEntity(self) )
        return !(static_cast<float>(G_rand()) > self->grenadeAwareness * 32767.0f);
    return true;
}

static bool Actor_Grenade_Discard_SP(actor_s *self, gentity_s *grenade)
{
    if ( !Actor_HasPath(self)
        || Actor_PointAtGoal(self->ent->r.currentOrigin, &self->scriptGoal) )
        return false;
    const float *landing = grenade->missile.predictedLandPos;
    const float *origin = self->ent->r.currentOrigin;
    const float x = landing[0] - origin[0];
    const float y = landing[1] - origin[1];
    float length = sqrtf(x * x + y * y);
    if ( -length >= 0.0f )
        length = 1.0f;
    const float inverse = 1.0f / length;
    const float dot = self->Path.lookaheadDir[1] * (inverse * y)
        + self->Path.lookaheadDir[0] * (inverse * x);
    if ( dot < 0.0f )
        return true;
    if ( grenade->parent.isDefined() )
    {
        const sentient_s *parent = grenade->parent.ent()->sentient;
        if ( parent && parent->eTeam != Sentient_EnemyTeam(self->sentient->eTeam) )
            return false;
    }
    if ( !(dot > 0.7f) )
        return false;
    const double dx = static_cast<double>(origin[0]) - landing[0];
    const double dy = static_cast<double>(origin[1]) - landing[1];
    const double dz = static_cast<double>(origin[2]) - landing[2];
    return (dx * dx + dz * dz) + dy * dy > 100.0;
}

void Actor_Grenade_Ping_SP(actor_s *self, gentity_s *grenade)
{
    if ( self->flashBanged
        || (self->eState[self->stateLevel] == AIS_GRENADE_RESPONSE
            && self->eSubState[self->stateLevel] == STATE_GRENADE_THROWBACK) )
        return;
    if ( self->pGrenade.isDefined() )
    {
        const float *cached = self->pGrenade.ent()->missile.predictedLandPos;
        if ( cached[0] != 0.0f || cached[1] != 0.0f || cached[2] != 0.0f )
            return;
    }
    if ( BG_GetWeaponDef(grenade->s.weapon)->bNoPing || !Actor_Grenade_Aware_SP(self) )
        return;
    self->pGrenade.setEnt(grenade);
    if ( grenade->parent.isDefined() )
    {
        const sentient_s *parent = grenade->parent.ent()->sentient;
        if ( parent && parent->eTeam == Sentient_EnemyTeam(self->sentient->eTeam) )
        {
            Scr_AddEntity(grenade, SCRIPTINSTANCE_SERVER);
            Scr_Notify(self->ent, scr_const.grenadedanger, 1);
        }
    }
    Actor_Grenade_Predict_SP(grenade);
    if ( Actor_Grenade_Discard_SP(self, grenade) )
        self->pGrenade.setEnt(NULL);
    else
    {
        float eye[3];
        Sentient_GetEyePosition(self->sentient, eye);
        if ( Actor_GrenadeThreatSafe_SP(self, eye) )
            self->pGrenade.setEnt(NULL);
    }
}

static int Actor_Grenade_FindAway_SP(actor_s *self, const float *danger, float radius,
    const float *avoidNormal, float avoidDist, bool suppression, int negotiation)
{
    Actor_ClearPath(self);
    float normal[5][2], dist[5];
    int planes = suppression ? Actor_GetSuppressionPlanes_SP(self, normal, dist) : 0;
    if ( avoidDist != 0.0f )
    {
        normal[planes][0] = avoidNormal[0];
        normal[planes][1] = avoidNormal[1];
        dist[planes++] = avoidDist;
    }
    return Path_FindAwayNotCrossPlanes_SP(&self->Path, self->sentient->eTeam,
        self->ent->r.currentOrigin, danger, radius, normal, dist, planes, negotiation);
}

static int Actor_Grenade_FindEscape_SP(actor_s *self, const float *danger,
    float radius, int negotiation)
{
    Actor_ClearPath(self);
    return Path_FindEscape_SP(&self->Path, self->sentient->eTeam,
        self->ent->r.currentOrigin, danger, radius, negotiation);
}

float g_grenadeHintPoints_SP[512][3];

void G_SpawnInfoGrenadeHint_SP(gentity_s *self, SpawnVar *)
{
    if ( static_cast<unsigned>(level.numGrenadeHints) >= 512u )
        Com_Error(ERR_DROP, "\x15MAX_GRENADE_HINTS (%i) exceeded\n", 512);
    Vec3Copy(self->r.currentOrigin, g_grenadeHintPoints_SP[level.numGrenadeHints]);
    ++level.numGrenadeHints;
    G_FreeEntity(self);
}

static bool Actor_Grenade_CanThrow_SP(actor_s *self, const float *target, unsigned weapon)
{
    const float distanceSq = static_cast<float>(BG_GetWeaponDef(weapon)->iExplosionRadius) * 1.1f;
    const team_t enemy = Sentient_EnemyTeam(self->sentient->eTeam);
    const int teams = ~(1 << enemy);
    for ( sentient_s *other = Sentient_FirstSentient(teams); other;
        other = Sentient_NextSentient(other, teams) )
    {
        float origin[3];
        Sentient_GetOrigin(other, origin);
        const float z = target[2] - origin[2];
        const float x = target[0] - origin[0];
        const float y = target[1] - origin[1];
        if ( distanceSq >= (z * z + x * x) + y * y )
            return false;
    }
    return true;
}

static bool Actor_Grenade_FindHint_SP(const float *from, const float *target,
    unsigned, float *output)
{
    const float angle = (ai_debugGrenadeHintArc->current.value * 0.5f + 180.0f)
        * 0.01745329238474369f;
    const float limit = static_cast<float>(std::cos(static_cast<double>(angle)));
    float best = 3.4028234663852886e38f;
    int selected = -1;
    for ( int i = 0; i < level.numGrenadeHints; ++i )
    {
        const float *hint = g_grenadeHintPoints_SP[i];
        if ( target[2] > hint[2] + 10.0f )
            continue;
        const float tx = hint[0] - target[0];
        const float ty = hint[1] - target[1];
        const float length = sqrtf(tx * tx + ty * ty);
        const float targetScale = 1.0f / (-length >= 0.0f ? 1.0f : length);
        const float targetX = targetScale * tx;
        const float targetY = targetScale * ty;
        if ( length > 192.0f )
            continue;
        const float fx = hint[0] - from[0];
        const float fy = hint[1] - from[1];
        const float fromLength = sqrtf(fy * fy + fx * fx);
        const float fromScale = 1.0f / (-fromLength >= 0.0f ? 1.0f : fromLength);
        const float dot = (fromScale * fx) * targetX + (fy * fromScale) * targetY;
        if ( dot > limit )
            continue;
        if ( best > length )
        {
            selected = i;
            best = length;
        }
    }
    if ( selected < 0 )
        return false;
    Vec3Copy(g_grenadeHintPoints_SP[selected], output);
    return true;
}

static bool Actor_Grenade_ValidateThrow_SP(actor_s *self, const float *start,
    const float *target, const float *velocity)
{
    col_context_t context(0x0280F091);
    context.passEntityNum0 = self->ent->s.number;
    const float gravity = bg_gravity->current.value;
    const float dz = start[2] - target[2];
    float apexTime, halfRise, halfFall;
    if ( velocity[2] > 0.0f )
    {
        const float inverse = 1.0f / gravity;
        apexTime = inverse * velocity[2];
        const float discriminant = (dz * inverse) * 2.0f + apexTime * apexTime;
        halfRise = apexTime * 0.5f;
        if ( 0.0f >= discriminant )
            return false;
        halfFall = sqrtf(discriminant) * 0.5f;
    }
    else
    {
        const float discriminant = (dz * gravity) * 2.0f + velocity[2] * velocity[2];
        apexTime = 0.0f;
        halfRise = 0.0f;
        if ( 0.0f >= discriminant )
            return false;
        halfFall = ((sqrtf(discriminant) + velocity[2]) / gravity) * 0.5f;
    }
    const float halfEndTime = halfFall + apexTime;
    const float endTime = halfEndTime + halfFall;
    if ( fabsf((velocity[0] * endTime + start[0]) - target[0]) > 0.1f
        || fabsf((velocity[1] * endTime + start[1]) - target[1]) > 0.1f )
        return false;

    const float mins[3] = { -1.0f, -1.0f, -1.0f };
    float maxs[3] = { 1.0f, 1.0f, 1.0f };
    float apex[3];
    int hitNum = 0;
    if ( apexTime > 0.0f )
    {
        maxs[2] = ((halfRise * halfRise) * gravity) * 0.125f + 1.0f;
        float rising[3] = { velocity[0] * halfRise + start[0],
            velocity[1] * halfRise + start[1],
            ((halfRise * halfRise) * gravity) * 1.5f + start[2] };
        SV_SightTraceCapsule(&hitNum, start, mins, maxs, rising, &context);
        if ( hitNum )
            return false;
        apex[0] = velocity[0] * apexTime + start[0];
        apex[1] = velocity[1] * apexTime + start[1];
        apex[2] = ((gravity * apexTime) * apexTime) * 0.5f + start[2];
        SV_SightTraceCapsule(&hitNum, rising, mins, maxs, apex, &context);
        if ( hitNum )
            return false;
    }
    else
        Vec3Copy(start, apex);
    maxs[2] = ((halfFall * halfFall) * gravity) * 0.125f + 1.0f;
    float falling[3] = { velocity[0] * halfEndTime + start[0],
        velocity[1] * halfEndTime + start[1],
        (velocity[2] - (halfEndTime * gravity) * 0.5f) * halfEndTime + start[2] };
    SV_SightTraceCapsule(&hitNum, apex, mins, maxs, falling, &context);
    if ( hitNum )
        return false;
    trace_t trace;
    G_TraceCapsule(&trace, falling, mins, maxs, target, self->ent->s.number,
        0x0280F091, &context);
    if ( trace.fraction == 1.0f )
        return true;
    const unsigned short hit = Trace_GetEntityHitId(&trace);
    if ( hit == ENTITYNUM_WORLD )
    {
        float point[3];
        for ( int i = 0; i < 3; ++i )
            point[i] = (target[i] - falling[i]) * trace.fraction + falling[i];
        const double x = static_cast<double>(target[0]) - point[0];
        const double y = static_cast<double>(target[1]) - point[1];
        const double z = static_cast<double>(target[2]) - point[2];
        if ( (x * x + z * z) + y * y < 16.0 )
            return true;
        return 900.0f > halfEndTime && trace.normal.vec.v[2] > 0.5f;
    }
    const sentient_s *sentient = g_entities[hit].sentient;
    return sentient && sentient->eTeam == Sentient_EnemyTeam(self->sentient->eTeam);
}

static bool Actor_Grenade_MinEnergy_SP(actor_s *self, const float *start,
    const float *target, float *velocity)
{
    const float x = target[0] - start[0];
    const float y = target[1] - start[1];
    const float z = target[2] - start[2];
    const float distance = sqrtf((y * y + x * x) + z * z);
    const float gravity = bg_gravity->current.value;
    if ( (z + distance) * gravity > 810000.0f )
        return false;
    const float time = sqrtf((distance / gravity) * 2.0f);
    const float inverse = 1.0f / time;
    velocity[0] = inverse * x;
    velocity[1] = inverse * y;
    velocity[2] = inverse * z + (time * gravity) * 0.5f;
    return Actor_Grenade_ValidateThrow_SP(self, start, target, velocity);
}

static bool Actor_Grenade_FixedSpeed_SP(actor_s *self, const float *start,
    const float *target, bool maxTime, float *velocity)
{
    const float x = target[0] - start[0];
    const float y = target[1] - start[1];
    const float z = target[2] - start[2];
    const float squared = (y * y + x * x) + z * z;
    if ( 1.0f > squared )
        return false;
    const float gravity = bg_gravity->current.value;
    const float energy = 810000.0f - gravity * z;
    const float discriminant = energy * energy - (gravity * gravity) * squared;
    if ( 0.0f > discriminant )
        return false;
    const float time = sqrtf((maxTime ? sqrtf(discriminant) + energy
        : energy - sqrtf(discriminant)) * 2.0f) / gravity;
    const float inverse = 1.0f / time;
    velocity[0] = inverse * x;
    velocity[1] = inverse * y;
    velocity[2] = inverse * z + (gravity * time) * 0.5f;
    return Actor_Grenade_ValidateThrow_SP(self, start, target, velocity);
}

static bool Actor_Grenade_Unlimited_SP(actor_s *self, const float *start,
    const float *target, float *velocity)
{
    const float x = target[0] - start[0];
    const float y = target[1] - start[1];
    const float z = target[2] - start[2];
    const float gravity = bg_gravity->current.value;
    const float distance = sqrtf((z * z + y * y) + x * x);
    const float time = sqrtf((distance * gravity) * 2.0f) / gravity;
    const float inverse = 1.0f / time;
    velocity[0] = inverse * x;
    velocity[1] = inverse * y;
    velocity[2] = (gravity * time) * 0.5f + inverse * z;
    return Actor_Grenade_ValidateThrow_SP(self, start, target, velocity);
}

static bool Actor_Grenade_HintArc_SP(actor_s *self, const float *start,
    const float *target, float *velocity)
{
    const float x = target[0] - start[0];
    const float y = target[1] - start[1];
    const float z = target[2] - start[2];
    const float targetLength = sqrtf(x * x + y * y);
    for ( unsigned i = 0; i < static_cast<unsigned>(level.numGrenadeHints); ++i )
    {
        const float *hint = g_grenadeHintPoints_SP[i];
        const float hx = hint[0] - start[0];
        const float hy = hint[1] - start[1];
        const float hz = hint[2] - start[2];
        if ( 0.0f > (hx * x + hy * y) + hz * z )
            continue;
        const float length = sqrtf(hy * hy + hx * hx);
        const float inverse = 1.0f / (-length >= 0.0f ? 1.0f : length);
        const float dirx = inverse * hx;
        const float diry = inverse * hy;
        if ( length >= targetLength || length == 0.0f )
            continue;
        float aligned[3] = { dirx * targetLength + start[0],
            diry * targetLength + start[1], target[2] };
        const float errorx = target[0] - aligned[0];
        const float errory = target[1] - aligned[1];
        if ( errory * errory + errorx * errorx > 36864.0f )
            continue;
        const float denominator = length * z - hz * targetLength;
        if ( denominator >= 0.0f )
            continue;
        const float coefficient = (bg_gravity->current.value / denominator) * -0.5f;
        const float horizontal = sqrtf((((targetLength - length) * length)
            * coefficient) * targetLength);
        const float vertical = (((hz * targetLength) * targetLength - (length * length) * z)
            / horizontal) * coefficient;
        if ( vertical * vertical + horizontal * horizontal > 810000.0f )
            continue;
        velocity[0] = dirx * horizontal;
        velocity[1] = diry * horizontal;
        velocity[2] = vertical;
        if ( Actor_Grenade_ValidateThrow_SP(self, start, aligned, velocity) )
            return true;
    }
    return false;
}

static void Actor_Grenade_AdjustTarget_SP(const float *start, const float *target,
    unsigned weapon, float *adjusted)
{
    const WeaponDef *definition = BG_GetWeaponDef(weapon);
    if ( definition->bProjImpactExplode )
    {
        Vec3Copy(target, adjusted);
        return;
    }
    float traceStart[3] = { target[0], target[1], target[2] + 1.0f };
    float traceEnd[3] = { target[0], target[1], target[2] - 1.0f };
    trace_t trace;
    G_MissileTrace(&trace, traceStart, traceEnd, ENTITYNUM_NONE, 0x811, weapon);
    const unsigned surface = trace.fraction == 1.0f ? 0 : (trace.sflags >> 20) & 63;
    const float parallel = definition->parallelBounce[surface];
    const float perpendicular = definition->perpendicularBounce[surface];
    const float scale = 1.0f - ((perpendicular - parallel) * 0.9397f + parallel);
    for ( int i = 0; i < 3; ++i )
        adjusted[i] = (target[i] - start[i]) * scale + start[i];
}

bool Actor_Grenade_GetMagicVelocity_SP(actor_s *self, const float *origin,
    const float *target, float *velocity)
{
    float adjusted[3];
    Actor_Grenade_AdjustTarget_SP(origin, target, self->iGrenadeWeaponIndex, adjusted);
    return Actor_Grenade_MinEnergy_SP(self, origin, adjusted, velocity)
        || Actor_Grenade_FixedSpeed_SP(self, origin, adjusted, false, velocity)
        || Actor_Grenade_FixedSpeed_SP(self, origin, adjusted, true, velocity)
        || Actor_Grenade_Unlimited_SP(self, origin, adjusted, velocity)
        || Actor_Grenade_HintArc_SP(self, origin, adjusted, velocity);
}

static void Actor_Grenade_ThrowStart_SP(actor_s *self, const float *origin,
    const float *armOffset, float *start)
{
    float axis[3][3];
    AnglesToAxis(self->ent->r.currentAngles, axis);
    for ( int i = 0; i < 3; ++i )
        start[i] = origin[i] + ((armOffset[0] * axis[0][i]
            + armOffset[1] * axis[1][i]) + armOffset[2] * axis[2][i]);
}

bool Actor_Grenade_CheckThrow_SP(actor_s *self, const float *origin,
    const float *armOffset, const float *target, unsigned method,
    float *start, float *velocity, float randomRange, bool allowHeld)
{
    if ( 0.0f >= bg_gravity->current.value )
        return false;
    if ( self->pGrenade.isDefined() && !allowHeld )
        return false;
    if ( !Actor_Grenade_CanThrow_SP(self, target, self->iGrenadeWeaponIndex) )
        return false;
    float randomized[3] = { target[0], target[1], target[2] };
    if ( randomRange != 0.0f )
    {
        randomized[0] = static_cast<float>(G_flrand(target[0] - randomRange,
            target[0] + randomRange));
        randomized[1] = static_cast<float>(G_flrand(target[1] - randomRange,
            target[1] + randomRange));
    }
    Actor_Grenade_ThrowStart_SP(self, origin, armOffset, start);
    trace_t trace;
    G_MissileTrace(&trace, start, randomized, ENTITYNUM_NONE, 0x811,
        self->iGrenadeWeaponIndex);
    float adjusted[3];
    if ( trace.fraction == 1.0f )
        Actor_Grenade_AdjustTarget_SP(start, randomized,
            self->iGrenadeWeaponIndex, adjusted);
    else if ( level.numGrenadeHints
        && Actor_Grenade_FindHint_SP(start, target, method, adjusted) )
    {
        for ( int i = 0; i < 3; ++i )
            adjusted[i] = static_cast<float>(G_flrand(adjusted[i] - 15.0f,
                adjusted[i] + 15.0f));
    }
    else
        Vec3Copy(randomized, adjusted);
    const float x = adjusted[0] - start[0];
    const float y = adjusted[1] - start[1];
    const float z = adjusted[2] - start[2];
    if ( (z * z + x * x) + y * y == 0.0f )
        return false;
    if ( method == scr_const.grenade_min_energy )
        return Actor_Grenade_MinEnergy_SP(self, start, adjusted, velocity);
    if ( method == scr_const.grenade_min_time )
        return Actor_Grenade_FixedSpeed_SP(self, start, adjusted, false, velocity);
    if ( method == scr_const.grenade_max_time )
        return Actor_Grenade_FixedSpeed_SP(self, start, adjusted, true, velocity);
    if ( method == scr_const.grenade_infinite_energy )
        return Actor_Grenade_Unlimited_SP(self, start, adjusted, velocity);
    Scr_Error(va("checkGrenadeThrow: method must be 'min energy', 'min time', or 'max time' - value passed in was %s",
        SL_ConvertToString(method, SCRIPTINSTANCE_SERVER)), SCRIPTINSTANCE_SERVER);
    return false;
}

bool Actor_Grenade_CheckThrowEnemy_SP(actor_s *self, const float *origin,
    const float *armOffset, unsigned method, float *start, float *velocity,
    float randomRange, bool allowHeld)
{
    sentient_s *enemy = Actor_GetTargetSentient(self);
    if ( !enemy || 0.0f >= bg_gravity->current.value )
        return false;
    if ( self->pGrenade.isDefined() && !allowHeld )
        return false;
    float targetOrigin[3], targetVelocity[3], target[3];
    Sentient_GetOrigin(enemy, targetOrigin);
    Sentient_GetVelocity(enemy, targetVelocity);
    for ( int i = 0; i < 3; ++i )
        self->vGrenadeTargetPos[i] = target[i] = targetVelocity[i] + targetOrigin[i];
    self->bGrenadeTargetValid = true;
    Scr_SetString(&self->GrenadeTossMethod, method, SCRIPTINSTANCE_SERVER);
    return Actor_Grenade_CheckThrow_SP(self, origin, armOffset, target, method,
        start, velocity, randomRange, allowHeld);
}

void Actor_Grenade_Pickup_SP(actor_s *self)
{
    gentity_s *grenade = self->pGrenade.ent();
    const WeaponDef *definition = BG_GetWeaponDef(static_cast<unsigned char>(grenade->s.weapon));
    const char *model = XModelGetName(definition->worldModel[grenade->s.weaponModel]);
    G_EntDetach(self->ent, model, scr_const.tag_inhand);
    G_EntAttach(self->ent, const_cast<char *>(model), scr_const.tag_inhand, 0);
    if ( self->vGrenadeTossVel[0] != 0.0f || self->vGrenadeTossVel[1] != 0.0f )
        Actor_FaceVector(&self->CodeOrient, self->vGrenadeTossVel);
    Actor_SetOrientMode(self, AI_ORIENT_DONT_CHANGE);
    grenade->r.svFlags |= 1;
    grenade->s.lerp.pos.trType = TR_STATIONARY;
    grenade->parent.setEnt(self->ent);
}

static void Actor_Grenade_Detach_SP(actor_s *self)
{
    gentity_s *grenade = self->pGrenade.ent();
    const WeaponDef *definition = BG_GetWeaponDef(static_cast<unsigned char>(grenade->s.weapon));
    const char *model = XModelGetName(definition->worldModel[grenade->s.weaponModel]);
    G_EntDetach(self->ent, model, scr_const.tag_inhand);
    float position[3];
    G_DObjGetWorldTagPos_SP(self->ent, scr_const.tag_inhand, position);
    Vec3Copy(position, grenade->r.currentOrigin);
    Vec3Copy(position, grenade->s.lerp.pos.trBase);
    for ( int i = 0; i < 3; ++i )
    {
        grenade->s.lerp.pos.trDelta[i] = 0.0f;
        grenade->missile.predictedLandPos[i] = 0.0f;
    }
    grenade->activator = NULL;
    grenade->r.svFlags &= ~1;
}

void Actor_Grenade_Cleanup_SP(actor_s *self)
{
    if ( !self->pGrenade.isDefined() )
        return;
    gentity_s *grenade = self->pGrenade.ent();
    if ( grenade->activator != self->ent )
        return;
    if ( grenade->r.svFlags & 1 )
    {
        Actor_Grenade_Detach_SP(self);
        G_InitGrenadeEntity(self->ent, grenade);
        G_InitGrenadeMovement(grenade, grenade->r.currentOrigin, vec3_origin,
            1, static_cast<WeapRotateType>(0));
        SV_LinkEntity(self->pGrenade.ent());
    }
    self->pGrenade.ent()->activator = NULL;
}

void Actor_Grenade_Throw_SP(actor_s *self)
{
    if ( self->ent->health <= 0
        || (!self->pGrenade.isDefined() && !self->bGrenadeTossValid) )
    {
        self->bGrenadeTossValid = false;
        Scr_SetString(&self->GrenadeTossMethod, 0, SCRIPTINSTANCE_SERVER);
        return;
    }
    float start[3], velocity[3];
    bool valid = false;
    if ( self->bGrenadeTargetValid )
    {
        float hand[3];
        if ( !G_DObjGetWorldTagPos(self->ent, scr_const.tag_inhand, hand) )
        {
            Scr_Error(va("Missing tag [%s] on entity [%d] (%s)\n",
                SL_ConvertToString(scr_const.tag_inhand, SCRIPTINSTANCE_SERVER),
                self->ent->s.number,
                DObjGetName(Com_GetServerDObj(self->ent->s.number))),
                SCRIPTINSTANCE_SERVER);
            return;
        }
        valid = Actor_Grenade_CheckThrow_SP(self, hand, vec3_origin,
            self->vGrenadeTargetPos, self->GrenadeTossMethod, start, velocity,
            0.0f, true);
    }
    if ( !valid )
    {
        Vec3Copy(self->vGrenadeTossPos, start);
        Vec3Copy(self->vGrenadeTossVel, velocity);
    }
    if ( self->pGrenade.isDefined()
        && self->eState[self->stateLevel] == AIS_GRENADE_RESPONSE )
    {
        Actor_Grenade_Detach_SP(self);
        gentity_s *grenade = self->pGrenade.ent();
        gentity_s *owner = grenade->r.ownerNum.ent();
        G_InitGrenadeEntity(self->ent, grenade);
        G_InitGrenadeMovement(self->pGrenade.ent(), start, velocity,
            1, static_cast<WeapRotateType>(0));
        if ( owner && owner->client )
        {
            const float x = start[0] - owner->r.currentOrigin[0];
            const float y = start[1] - owner->r.currentOrigin[1];
            const float distance = sqrtf(y * y + x * x);
            const float speed = sqrtf(velocity[1] * velocity[1]
                + velocity[0] * velocity[0]);
            const float residual = static_cast<float>(grenade->nextthink - level.time)
                * 0.001f - distance / speed;
            if ( residual > 0.0f && residual < 1.0f )
                grenade->nextthink += 1000;
            else if ( residual < 0.0f && residual > -0.7f )
                grenade->nextthink -= 1000;
        }
    }
    else
    {
        if ( !self->iGrenadeWeaponIndex )
            Scr_Error(va("Actor [%s] doesn't have a grenade weapon set.",
                SL_ConvertToString(self->ent->classname, SCRIPTINSTANCE_SERVER)),
                SCRIPTINSTANCE_SERVER);
        const WeaponDef *definition = BG_GetWeaponDef(self->iGrenadeWeaponIndex);
        G_FireGrenade(self->ent, start, velocity, self->iGrenadeWeaponIndex,
            0, 1, definition->aiFuseTime);
        if ( self->iGrenadeAmmo > 0 )
            --self->iGrenadeAmmo;
    }
    self->bGrenadeTossValid = false;
    Scr_SetString(&self->GrenadeTossMethod, 0, SCRIPTINSTANCE_SERVER);
}

static bool Actor_Grenade_PathSegment_SP(float dx, float dy, const float *dir,
    float length, float radiusSq)
{
    const float cross = dy * dir[0] - dir[1] * dx;
    if ( !(radiusSq > cross * cross) )
        return false;
    const float along = dir[1] * dy + dir[0] * dx;
    if ( along < 0.0f )
        return radiusSq > dx * dx + dy * dy;
    if ( length >= along )
        return true;
    const float x = dir[0] * length + dx;
    const float y = dir[1] * length + dy;
    return radiusSq > x * x + y * y;
}

static bool Actor_Grenade_PathIntersects_SP(const path_t *path, const float *origin,
    const float *danger, float radiusSq)
{
    const float length = path->fLookaheadDist;
    const float ex = path->lookaheadDir[0] * length + origin[0];
    const float ey = path->lookaheadDir[1] * length + origin[1];
    const float dx = danger[0] - ex;
    const float dy = danger[1] - ey;
    const float cross = dy * path->lookaheadDir[0] - path->lookaheadDir[1] * dx;
    if ( radiusSq > cross * cross )
    {
        const float along = path->lookaheadDir[1] * dy + path->lookaheadDir[0] * dx;
        if ( along < 0.0f )
        {
            if ( radiusSq > dx * dx + dy * dy )
                return true;
        }
        else if ( length >= along )
            return true;
        else
        {
            const float y = origin[1] - danger[1];
            const float x = origin[0] - danger[0];
            if ( radiusSq > y * y + x * x )
                return true;
        }
    }
    if ( path->fLookaheadDistToNextNode != 0.0f )
    {
        const pathpoint_t *point = &path->pts[path->lookaheadNextNode];
        if ( Actor_Grenade_PathSegment_SP(danger[0] - point->vOrigPoint[0],
            danger[1] - point->vOrigPoint[1], point->fDir2D,
            path->fLookaheadDistToNextNode, radiusSq) )
            return true;
    }
    for ( int i = path->lookaheadNextNode - 1; i >= 0; --i )
    {
        const pathpoint_t *point = &path->pts[i];
        if ( Actor_Grenade_PathSegment_SP(danger[0] - point->vOrigPoint[0],
            danger[1] - point->vOrigPoint[1], point->fDir2D, point->fOrigLength, radiusSq) )
            return true;
    }
    return false;
}

static pathnode_t *Actor_Grenade_FindCover_SP(actor_s *self, const float *danger, float radius)
{
    Actor_HasPath(self);
    pathsort_t nodes[256];
    const int count = Path_NodesInCylinder(self->ent->r.currentOrigin, 512.0f, 80.0f,
        nodes, 256, 0x83ffcu);
    const float radiusSq = radius * radius;
    pathnode_t *best = NULL;
    float bestScore = 0.0f;
    for ( int i = 0; i < count; ++i )
    {
        pathnode_t *node = nodes[i].node;
        const float dx = node->constant.vOrigin[0] - danger[0];
        const float dy = node->constant.vOrigin[1] - danger[1];
        if ( dy * dy + dx * dx <= radiusSq )
        {
            const float eye = ((1u << node->constant.type) & 0x2020u)
                || (node->constant.spawnflags & 8u) ? 20.0f : 36.0f;
            if ( G_CanDamage_SP(self->ent, node->constant.vOrigin, NULL, danger, radius,
                1.0f, NULL, eye, false, 0x802013) )
                continue;
        }
        if ( !Actor_Human_CoverUsable_SP(self, node) )
            continue;
        const float score = Actor_Human_GrenadeCoverScore_SP(self, node);
        if ( score > bestScore )
        {
            bestScore = score;
            best = node;
        }
    }
    return best;
}

static float Actor_Grenade_AvoidPlane_SP(actor_s *self, float *normal)
{
    const gentity_s *target = Actor_GetTargetEntity(self);
    if ( !target )
        return 0.0f;
    const float *origin = self->ent->r.currentOrigin;
    const float *landing = self->pGrenade.ent()->missile.predictedLandPos;
    normal[0] = target->r.currentOrigin[0] - origin[0];
    normal[1] = target->r.currentOrigin[1] - origin[1];
    const float x = target->r.currentOrigin[0] - landing[0];
    const float y = target->r.currentOrigin[1] - landing[1];
    const float lengthSq = normal[0] * normal[0] + normal[1] * normal[1];
    const float *point = normal[0] * x + normal[1] * y > lengthSq ? origin : landing;
    const float dot = point[1] * normal[1] + point[0] * normal[0];
    float distance = sqrtf(lengthSq) * 15.0f + dot;
    if ( dot > distance )
    {
        normal[0] = -normal[0];
        normal[1] = -normal[1];
        distance = -distance;
    }
    const bool showPlane = ai_debugEntIndex->current.integer == self->ent->s.number
        ? ai_showSuppression->current.integer > 0
        : ai_showSuppression->current.integer == self->ent->s.number;
    if ( showPlane )
    {
        const float color[4] = {1.0f, 0.7f, 0.0f, 1.0f};
        G_DebugPlane_SP(normal, distance, origin, color, 100.0f, 0, 100);
    }
    return distance;
}

static float s_grenadeHintSortOrigin[3];

static int Actor_Grenade_HintCompare_SP(const void *left, const void *right)
{
    const float *a = static_cast<const float *>(left);
    const float *b = static_cast<const float *>(right);
    const float ax = a[0] - s_grenadeHintSortOrigin[0];
    const float ay = a[1] - s_grenadeHintSortOrigin[1];
    const float az = a[2] - s_grenadeHintSortOrigin[2];
    const float bx = b[0] - s_grenadeHintSortOrigin[0];
    const float by = b[1] - s_grenadeHintSortOrigin[1];
    const float bz = b[2] - s_grenadeHintSortOrigin[2];
    const float da = (ax * ax + az * az) + ay * ay;
    const float db = (bz * bz + bx * bx) + by * by;
    return db > da ? -1 : da > db ? 1 : 0;
}

static bool Actor_Grenade_FallbackHint_SP(actor_s *self, const float *origin, float *velocity)
{
    if ( !level.numGrenadeHints )
        return false;
    Vec3Copy(origin, s_grenadeHintSortOrigin);
    qsort(g_grenadeHintPoints_SP, level.numGrenadeHints, sizeof(g_grenadeHintPoints_SP[0]),
        Actor_Grenade_HintCompare_SP);
    for ( unsigned int i = 0; i < static_cast<unsigned int>(level.numGrenadeHints); ++i )
    {
        const float *hint = g_grenadeHintPoints_SP[i];
        const float x = hint[0] - origin[0];
        const float y = hint[1] - origin[1];
        const float z = hint[2] - origin[2];
        const float lengthSq = (y * y + x * x) + z * z;
        if ( 1.0f > lengthSq )
            continue;
        const float gravity = bg_gravity->current.value;
        const float energy = 810000.0f - gravity * z;
        const float discriminant = energy * energy - (gravity * gravity) * lengthSq;
        if ( 0.0f > discriminant )
            continue;
        const float time = sqrtf((energy - sqrtf(discriminant)) * 2.0f) / gravity;
        const float inverse = 1.0f / time;
        velocity[0] = inverse * x;
        velocity[1] = inverse * y;
        velocity[2] = inverse * z + (gravity * time) * 0.5f;
        if ( Actor_Grenade_ValidateThrow_SP(self, origin, hint, velocity) )
            return true;
    }
    return false;
}

static void Actor_Grenade_Approach_SP(const float *landing, const float *target, float *approach)
{
    const float x = landing[0] - target[0];
    const float y = landing[1] - target[1];
    const float length = sqrtf(x * x + y * y);
    const float divisor = -length >= 0.0f ? 1.0f : length;
    const float inverse = 1.0f / divisor;
    approach[0] = (inverse * x) * 29.5f + landing[0];
    approach[1] = (inverse * y) * 29.5f + landing[1];
    approach[2] = landing[2];
}

static bool Actor_Grenade_TryMethods_SP(actor_s *self, const float *origin,
    const float *target, float *start, float *velocity)
{
    const float arm[3] = {0.0f, 0.0f, 40.0f};
    const unsigned int methods[3] = {scr_const.grenade_min_energy,
        scr_const.grenade_min_time, scr_const.grenade_max_time};
    for ( unsigned int method : methods )
    {
        Scr_SetString(&self->GrenadeTossMethod, method, SCRIPTINSTANCE_SERVER);
        if ( Actor_Grenade_CheckThrow_SP(self, origin, arm, target, method,
            start, velocity, 0.0f, true) )
            return true;
    }
    return false;
}

static bool Actor_Grenade_TryAcquire_SP(actor_s *self)
{
    gentity_s *grenade = self->pGrenade.ent();
    if ( !grenade->parent.isDefined() || !grenade->parent.ent()->sentient )
        return false;
    gentity_s *parent = grenade->parent.ent();
    if ( parent->sentient->eTeam != Sentient_EnemyTeam(self->sentient->eTeam)
        || BG_GetWeaponDef(grenade->s.weapon)->weapType == WEAPTYPE_PROJECTILE )
        return false;
    float landing[3];
    Vec3Copy(grenade->missile.predictedLandPos, landing);
    const float dy = landing[1] - self->ent->r.currentOrigin[1];
    const float dx = landing[0] - self->ent->r.currentOrigin[0];
    if ( sqrtf(dy * dy + dx * dx) > 150.0f )
        return false;
    float target[3], approach[3], start[3], velocity[3];
    Sentient_GetOrigin(parent->sentient, target);
    const bool atLanding = Actor_PointAt(self->ent->r.currentOrigin, landing);
    if ( atLanding )
        Vec3Copy(landing, approach);
    else
        Actor_Grenade_Approach_SP(landing, target, approach);
    if ( !Actor_Grenade_TryMethods_SP(self, landing, target, start, velocity) )
    {
        sentient_s *enemy = Actor_GetTargetSentient(self);
        bool solved = false;
        if ( enemy )
        {
            Sentient_GetOrigin(enemy, target);
            solved = Actor_Grenade_TryMethods_SP(self, landing, target, start, velocity);
            if ( solved && !atLanding )
                Actor_Grenade_Approach_SP(landing, target, approach);
        }
        if ( !solved )
        {
            self->bGrenadeTargetValid = false;
            if ( !Actor_Grenade_FallbackHint_SP(self, landing, velocity) )
                return false;
        }
    }
    Actor_FindPath(self, approach, false, true);
    if ( !Actor_HasPath(self) )
        return false;
    if ( self->goalPosChanged )
        Scr_Notify(self->ent, scr_const.goal_changed, 0);
    if ( !Actor_GetTargetEntity(self) )
    {
        Actor_GetPerfectInfo(self, parent->sentient);
        Sentient_SetEnemy(self->sentient, parent, true);
    }
    self->bGrenadeTossValid = true;
    Vec3Copy(landing, self->vGrenadeTossPos);
    Vec3Copy(velocity, self->vGrenadeTossVel);
    if ( self->bGrenadeTargetValid )
        Vec3Copy(target, self->vGrenadeTargetPos);
    if ( grenade->activator != self->ent )
    {
        grenade->nextthink += 1000;
        grenade->activator = self->ent;
        Actor_SetSubState(self, STATE_GRENADE_ACQUIRE);
    }
    return true;
}

static void Actor_Grenade_Decide_SP(actor_s *self, bool force)
{
    gentity_s *grenade = self->pGrenade.ent();
    const bool own = grenade->parent.isDefined() && grenade->parent.ent() == self->ent;
    Actor_Grenade_Cleanup_SP(self);
    if ( !Actor_GetTargetEntity(self) && grenade->parent.isDefined() )
    {
        gentity_s *parent = grenade->parent.ent();
        sentient_s *sentient = parent->sentient;
        if ( sentient && sentient != self->sentient && sentient->eTeam != self->sentient->eTeam
            && !Actor_CheckIgnore(sentient, self->sentient) )
        {
            Actor_GetPerfectInfo(self, sentient);
            Sentient_SetEnemy(self->sentient, parent, true);
        }
    }
    const float radius = static_cast<float>(BG_GetWeaponDef(grenade->s.weapon)->iExplosionRadius) * 1.1f;
    if ( !force && Actor_HasPath(self)
        && !Actor_Grenade_PathIntersects_SP(&self->Path, self->ent->r.currentOrigin,
            grenade->missile.predictedLandPos, radius * radius) )
    {
        Actor_SetSubState(self, STATE_GRENADE_FLEE);
        return;
    }
    if ( own )
    {
        Actor_Human_FindPathToGoal_SP(self);
        if ( Actor_HasPath(self) && Actor_GrenadeThreatSafe_SP(self, self->Path.vFinalGoal) )
        {
            Actor_SetSubState(self, STATE_GRENADE_FLEE);
            return;
        }
    }
    else
    {
        pathnode_t *node = Actor_Grenade_FindCover_SP(self, grenade->missile.predictedLandPos, radius);
        if ( node && Actor_FindPathToNode(self, node, 1) )
        {
            Actor_ClearKeepClaimedNode(self);
            Sentient_ClaimNode(self->sentient, node);
            Actor_SetSubState(self, STATE_GRENADE_TAKECOVER);
            return;
        }
    }
    float normal[2];
    const float distance = Actor_Grenade_AvoidPlane_SP(self, normal);
    Actor_Grenade_FindAway_SP(self, grenade->missile.predictedLandPos, radius, normal, distance, true, true);
    if ( Actor_HasPath(self) )
    {
        Actor_SetSubState(self, STATE_GRENADE_FLEE);
        return;
    }
    if ( Actor_IsSuppressed_SP(self) )
    {
        Actor_Grenade_FindAway_SP(self, grenade->missile.predictedLandPos, radius, normal, distance, false, true);
        if ( Actor_HasPath(self) )
        {
            Actor_SetSubState(self, STATE_GRENADE_FLEE);
            return;
        }
    }
    Actor_Grenade_FindEscape_SP(self, grenade->missile.predictedLandPos, radius, true);
    Actor_SetSubState(self, Actor_HasPath(self) ? STATE_GRENADE_FLEE : STATE_GRENADE_COWER);
}

static void Actor_Grenade_Move_SP(actor_s *self, scr_animscript_t *stop)
{
    if ( self->Path.iPathEndTime && self->Path.iPathEndTime - level.time < 200 )
        Actor_SetAnimScript(self, stop, AI_MOVE_STOP_SOON, AI_ANIM_MOVE_CODE, AI_ANIM_FUNCTION_MOVE);
    else
        Actor_SetAnimScript(self, &g_animScriptTable[self->species]->move, AI_MOVE_RUN,
            AI_ANIM_MOVE_CODE, AI_ANIM_FUNCTION_MOVE);
    self->bUseGoalWeight = false;
}

static void Actor_Grenade_Cower_SP(actor_s *self)
{
    Actor_SetOrientMode(self, AI_ORIENT_DONT_CHANGE);
    Actor_AnimStop(self, &g_animScriptTable[self->species]->grenade_cower);
}

static void Actor_Grenade_Combat_SP(actor_s *self)
{
    if ( !Actor_GetTargetEntity(self) )
    {
        Actor_SetSubState(self, STATE_GRENADE_COWER);
        Actor_Grenade_Cower_SP(self);
        return;
    }
    Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY);
    Actor_Human_ExposedCombatAnim_SP(self);
}

static void Actor_Grenade_Attack_SP(actor_s *self)
{
    if ( !Actor_GetTargetEntity(self) )
    {
        Actor_SetSubState(self, STATE_GRENADE_COWER);
        Actor_Grenade_Cower_SP(self);
        return;
    }
    Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY);
    Actor_AnimSpecific(self, self->grenadeCoverAnim, AI_ANIM_USE_BOTH_DELTAS, true);
}

static void Actor_Grenade_FleeNotice_SP(actor_s *self)
{
    if ( self->pGrenade.isDefined() && !self->grenadeFleeNotice.isDefined() )
    {
        self->grenadeFleeNotice.setEnt(self->pGrenade.ent());
        Scr_AddString(BG_WeaponName(self->pGrenade.ent()->s.weapon),
            SCRIPTINSTANCE_SERVER);
        Scr_Notify(self->ent, scr_const.grenade_flee, 1);
    }
}

static void Actor_Grenade_Flee_SP(actor_s *self)
{
    if ( !Actor_HasPath(self) )
    {
        if ( Actor_GrenadeThreatSafe_SP(self, self->ent->r.currentOrigin) )
        {
            Actor_SetSubState(self, STATE_GRENADE_COMBAT);
            Actor_Grenade_Combat_SP(self);
        }
        else
        {
            Actor_SetSubState(self, STATE_GRENADE_COWER);
            Actor_Grenade_Cower_SP(self);
        }
        return;
    }
    Actor_Grenade_FleeNotice_SP(self);
    Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY_OR_MOTION);
    Actor_TeamMoveBlockedClear(self);
    Actor_MoveAlongPathWithTeam(self, true, false, false);
}

static void Actor_Grenade_Cover_SP(actor_s *self)
{
    if ( !Actor_HasPath(self) )
    {
        pathnode_t *node = self->sentient->pClaimedNode;
        if ( node )
        {
            Actor_Human_NodeAnimUsable_SP(self, node, true, &self->grenadeCoverAnim);
            if ( self->grenadeCoverAnim && Actor_GrenadeThreatSafe_SP(self, self->ent->r.currentOrigin) )
            {
                Actor_SetSubState(self, STATE_GRENADE_COVERATTACK);
                Actor_Grenade_Attack_SP(self);
                return;
            }
        }
        Actor_SetSubState(self, STATE_GRENADE_FLEE);
        Actor_Grenade_Flee_SP(self);
        return;
    }
    Actor_Grenade_FleeNotice_SP(self);
    Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY_OR_MOTION);
    Actor_TeamMoveBlockedClear(self);
    Actor_MoveAlongPathWithTeam(self, true, false, false);
}

static actor_think_result_t Actor_Grenade_ReturnThrow_SP(actor_s *self)
{
    Actor_CheckCollisions(self);
    Actor_SetAnimScript(self, &g_animScriptTable[self->species]->grenade_return_throw,
        AI_MOVE_STOP, AI_ANIM_MOVE_CODE, AI_ANIM_FUNCTION_STOP);
    self->bUseGoalWeight = true;
    if ( Actor_IsAnimScriptAlive(self) )
    {
        Actor_UpdateOriginAndAngles(self);
        return ACTOR_THINK_DONE;
    }
    Actor_SetState(self, AIS_EXPOSED);
    if ( self->pGrenade.isDefined() && self->pGrenade.ent()->activator == self->ent
        && (self->pGrenade.ent()->s.lerp.eFlags & 1u) )
        Scr_Error(va("ai classname '%s' at (%g %g %g) finished the grenade return script without throwing a grenade",
            SL_ConvertToString(self->ent->classname, SCRIPTINSTANCE_SERVER), self->ent->r.currentOrigin[0],
            self->ent->r.currentOrigin[1], self->ent->r.currentOrigin[2]), SCRIPTINSTANCE_SERVER);
    return ACTOR_THINK_REPEAT;
}

static bool Actor_Grenade_Reacquire_SP(actor_s *self, bool force)
{
    gentity_s *grenade = self->pGrenade.ent();
    if ( !force )
    {
        const float dy = grenade->r.currentOrigin[1] - self->ent->r.currentOrigin[1];
        const float dx = grenade->r.currentOrigin[0] - self->ent->r.currentOrigin[0];
        if ( 22500.0f >= dy * dy + dx * dx && Actor_Grenade_TryAcquire_SP(self) )
            return false;
    }
    if ( grenade->activator == self->ent
        && Actor_GrenadeThreatSafe_SP(self, self->ent->r.currentOrigin) )
    {
        grenade->activator = NULL;
        self->pGrenade.setEnt(NULL);
        Actor_SetState(self, AIS_EXPOSED);
        return true;
    }
    Actor_Grenade_Decide_SP(self, true);
    return false;
}

static actor_think_result_t Actor_Grenade_Acquire_SP(actor_s *self)
{
    if ( self->pPileUpEnt )
    {
        Actor_Grenade_Decide_SP(self, true);
        actor_s *other = self->pPileUpEnt->actor;
        if ( other && other->pGrenade.isDefined() && other->pGrenade.ent() == self->pGrenade.ent()
            && other->eState[other->stateLevel] == AIS_GRENADE_RESPONSE )
            Actor_Grenade_Reacquire_SP(other, false);
        if ( self->eSubState[self->stateLevel] == STATE_GRENADE_ACQUIRE )
            Actor_SetSubState(self, STATE_GRENADE_COWER);
        return ACTOR_THINK_REPEAT;
    }
    if ( self->Physics.iHitEntnum != ENTITYNUM_NONE )
    {
        Actor_Grenade_Decide_SP(self, true);
        if ( self->eSubState[self->stateLevel] == STATE_GRENADE_ACQUIRE )
            Actor_SetSubState(self, STATE_GRENADE_COWER);
        return ACTOR_THINK_REPEAT;
    }
    if ( Actor_HasPath(self) )
    {
        Actor_SetOrientMode(self, AI_ORIENT_TO_ENEMY_OR_MOTION);
        Actor_Grenade_Move_SP(self, &g_animScriptTable[self->species]->combat);
        Actor_PostThink(self);
        return ACTOR_THINK_DONE;
    }
    const float dy = self->Path.vFinalGoal[1] - self->ent->r.currentOrigin[1];
    const float dx = self->Path.vFinalGoal[0] - self->ent->r.currentOrigin[0];
    if ( !self->bThrowbackGrenades || dy * dy + dx * dx > 100.0f )
        return Actor_Grenade_Reacquire_SP(self, true) ? ACTOR_THINK_REPEAT : ACTOR_THINK_DONE;
    if ( fabsf(self->ent->r.currentOrigin[2] - self->Path.vFinalGoal[2]) > 32.0f
        && Actor_Grenade_Reacquire_SP(self, true) )
        return ACTOR_THINK_REPEAT;
    gentity_s *grenade = self->pGrenade.ent();
    float direction[3];
    for ( int i = 0; i < 3; ++i )
        direction[i] = grenade->missile.predictedLandPos[i] - self->ent->r.currentOrigin[i];
    if ( direction[0] * direction[0] + direction[1] * direction[1] >= 1.0f )
        Actor_FaceVector(&self->CodeOrient, direction);
    Actor_SetOrientMode(self, AI_ORIENT_DONT_CHANGE);
    self->ScriptOrient.eMode = AI_ORIENT_INVALID;
    if ( grenade->missile.predictedTimeAtRest <= level.time )
    {
        grenade->missile.grenadeReturnThrow = true;
        Actor_SetSubState(self, STATE_GRENADE_THROWBACK);
        return Actor_Grenade_ReturnThrow_SP(self);
    }
    Actor_PostThink(self);
    return ACTOR_THINK_DONE;
}

bool __fastcall Actor_Grenade_Start_SP(actor_s *self, ai_state_t)
{
    if ( !self->pGrenade.isDefined() )
        return false;
    gentity_s *grenade = self->pGrenade.ent();
    if ( grenade->activator || self->bGrenadeTossValid || grenade->missile.grenadeReturnThrow
        || !Actor_Grenade_TryAcquire_SP(self) )
        Actor_Grenade_Decide_SP(self, false);
    return true;
}

void __fastcall Actor_Grenade_Finish_SP(actor_s *self, ai_state_t)
{
    Actor_Grenade_Cleanup_SP(self);
}

bool __fastcall Actor_Grenade_Resume_SP(actor_s *self, ai_state_t)
{
    return self->pGrenade.isDefined() && self->bGrenadeTossValid;
}

actor_think_result_t __fastcall Actor_Grenade_Think_SP(actor_s *self)
{
    if ( !self->pGrenade.isDefined() || self->flashBanged )
    {
        Actor_SetState(self, AIS_EXPOSED);
        return ACTOR_THINK_REPEAT;
    }
    Actor_PreThink(self);
    const ai_substate_t substate = self->eSubState[self->stateLevel];
    if ( (substate == STATE_GRENADE_TAKECOVER || substate == STATE_GRENADE_COVERATTACK)
        && !self->sentient->pClaimedNode )
        Actor_Grenade_Decide_SP(self, true);
    switch ( self->eSubState[self->stateLevel] )
    {
    case STATE_GRENADE_FLEE:
        self->pszDebugInfo = "grenade_flee";
        Actor_Grenade_Flee_SP(self);
        break;
    case STATE_GRENADE_TAKECOVER:
        self->pszDebugInfo = "grenade_takecover";
        Actor_Grenade_Cover_SP(self);
        break;
    case STATE_GRENADE_COWER:
        self->pszDebugInfo = "grenade_cower";
        Actor_Grenade_Cower_SP(self);
        break;
    case STATE_GRENADE_COMBAT:
        self->pszDebugInfo = "grenade_combat";
        Actor_Grenade_Combat_SP(self);
        break;
    case STATE_GRENADE_COVERATTACK:
        self->pszDebugInfo = "grenade_coverattack";
        Actor_Grenade_Attack_SP(self);
        break;
    case STATE_GRENADE_ACQUIRE:
        self->pszDebugInfo = "grenade_acquire";
        return Actor_Grenade_Acquire_SP(self);
    case STATE_GRENADE_THROWBACK:
        self->pszDebugInfo = "grenade_throwback";
        return Actor_Grenade_ReturnThrow_SP(self);
    }
    Actor_PostThink(self);
    return ACTOR_THINK_DONE;
}

void __fastcall Actor_Grenade_Pain_SP(actor_s *, gentity_s *, int, const float *, int,
    const float *, hitLocation_t)
{
}

void __fastcall Actor_Grenade_React_SP(actor_s *, gentity_s *, const float *)
{
}
#endif
