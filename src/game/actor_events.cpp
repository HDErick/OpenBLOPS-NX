#include "actor_events.h"
#ifdef KISAK_SP
#include "actor_badplace.h"
#include "actor_grenade.h"
#include <game_mp/g_utils_mp.h>
#include <server/sv_game.h>
#endif
#include <game_mp/g_main_mp.h>
#include "actor_senses.h"
#include "bullet.h"
#include <game_mp/actor_mp.h>
#include "actor_event_listeners.h"
#include <game_mp/g_spawn_mp.h>
#include <clientscript/cscr_vm.h>
#include <game_mp/g_misc_mp.h>
#include <clientscript/scr_const.h>

struct $97E41B13BD0B431546CEBD7708497565 // sizeof=0xC
{                                       // XREF: .rdata:g_ai_event_info/r
    const char *name;                   // XREF: Actor_DumpEvents+6E/r
    const dvar_s **defaultDistDvar;     // XREF: Actor_EventDefaultRadiusSqrd(ai_event_t)+F/r
                                        // Actor_EventDefaultRadiusSqrd(ai_event_t)+41/r
    float defaultHeight;
};

const $97E41B13BD0B431546CEBD7708497565 g_ai_event_info[28] =
{
  { NULL, NULL, 0.0 },
  { NULL, NULL, 0.0 },
  { "footstep", &ai_eventDistFootstep, 0.0 },
  { "footsteplite", &ai_eventDistFootstepLite, 0.0 },
  { "new_enemy", &ai_eventDistNewEnemy, 0.0 },
  { "pain", &ai_eventDistPain, 0.0 },
  { "react", &ai_eventDistReact, 0.0 },
  { "death", &ai_eventDistDeath, 0.0 },
  { "explosion", &ai_eventDistExplosion, 0.0 },
  { "grenade_ping", &ai_eventDistGrenadePing, 0.0 },
  { "projectile_ping", &ai_eventDistProjPing, 0.0 },
  { "gunshot", &ai_eventDistGunShot, 0.0 },
  { "silenced_shot", &ai_eventDistSilencedShot, 0.0 },
  { NULL, NULL, 0.0 },
  { NULL, NULL, 0.0 },
  { "bullet", &ai_eventDistBullet, 45.0 },
  { "bullet", &ai_eventDistBullet, 45.0 },
  { "bullet_react", &ai_eventDistBulletRunning, 45.0 },
  { "projectile_impact", &ai_eventDistProjImpact, 45.0 },
  { NULL, NULL, 0.0 },
  { NULL, NULL, 0.0 },
  { "badplacearc", &ai_eventDistBadPlace, 0.0 },
  { "badplacelimitedarc", &ai_eventDistBadPlace, 0.0 },
  { "reallybadplacearc", &ai_eventDistBadPlace, 0.0 },
  { NULL, NULL, 0.0 },
  { NULL, NULL, 0.0 },
  { "badplacevolume", &ai_eventDistBadPlace, 0.0 },
  { NULL, NULL, 0.0 }
};



#ifdef KISAK_SP
#include "actor_animapi.h"
#include "actor_state.h"
#include "actor_orientation.h"
#include "actor_script_cmd.h"
#include <clientscript/cscr_stringlist.h>
#include <bgame/bg_weapons.h>
#include <game_mp/g_scr_main_mp.h>
#include <bit>
#include <cstdint>

static int Actor_ElapsedTime_SP(int now, int then)
{
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(now) - static_cast<std::uint32_t>(then));
}

static int Actor_SelectSuppression_SP(const actor_s *self, const sentient_s *suppressor)
{
    for (int i = 0; i < 4; ++i)
        if (self->suppression[i].suppressor == suppressor)
            return i;
    int selected = 0;
    for (int i = 1; i < 4; ++i)
        if (self->suppression[i].time < self->suppression[selected].time)
            selected = i;
    return selected;
}

static bool Actor_CanSuppress_SP(actor_s *self)
{
    if (Actor_NearClaimNode(self, 32.0f))
        return true;
    pathnode_t *node = Sentient_NearestNode(self->sentient);
    return node && ((1u << (static_cast<unsigned int>(node->constant.type) & 31u)) & 0x83ffcu)
        && Actor_PointNearPoint(self->ent->r.currentOrigin, node->constant.vOrigin, 32.0f);
}

static void Actor_BulletWhizby_SP(actor_s *self, sentient_s *suppressor, const float *start, const float *end, const float *closest)
{
    Scr_AddVector(const_cast<float *>(closest), SCRIPTINSTANCE_SERVER);
    Scr_AddVector(const_cast<float *>(end), SCRIPTINSTANCE_SERVER);
    Scr_AddVector(const_cast<float *>(start), SCRIPTINSTANCE_SERVER);
    Scr_AddEntity(suppressor->ent, SCRIPTINSTANCE_SERVER);
    Scr_Notify(self->ent, scr_const.bulletwhizby, 4u);
}

static void Actor_LinePoint_SP(const float *point, const float *start, const float *end, float *out)
{
    const float dx = end[0] - start[0];
    const float dy = end[1] - start[1];
    const float dz = end[2] - start[2];
    const float length = (dx * dx + dz * dz) + dy * dy;
    if (length == 0.0f)
    {
        Vec3Copy(start, out);
        return;
    }
    const float scale = (((point[0] - start[0]) * dx + (point[2] - start[2]) * dz)
        + (point[1] - start[1]) * dy) / length;
    out[0] = dx * scale + start[0];
    out[1] = dy * scale + start[1];
    out[2] = dz * scale + start[2];
}

void Actor_Suppress_SP(actor_s *self, sentient_s *suppressor, const float *start, const float *end, const float *closest)
{
    if (self->sentient->eTeam == suppressor->eTeam)
    {
        if (self->ignoreSuppression)
            return;
        if (!suppressor->ent->client)
        {
            if (Actor_IsMoving(suppressor->ent->actor))
                return;
            const float z = start[2] - self->ent->r.currentOrigin[2];
            const float x = start[0] - self->ent->r.currentOrigin[0];
            const float y = start[1] - self->ent->r.currentOrigin[1];
            const float distance = (z * z + x * x) + y * y;
            const float limit = ai_friendlySuppressionDist->current.value;
            if (distance > limit * limit)
                return;
        }
    }
    else
    {
        float meter = self->suppressionMeter + 0.15f;
        if (1.0f - meter < 0.0f)
            meter = 1.0f;
        self->suppressionMeter = meter;
        if (!Actor_CanSuppress_SP(self))
        {
            Actor_BulletWhizby_SP(self, suppressor, start, end, closest);
            return;
        }
    }
    actor_suppression_s &record = self->suppression[Actor_SelectSuppression_SP(self, suppressor)];
    record.time = level.time;
    record.suppressor = suppressor;
    float x = start[1] - end[1];
    float y = end[0] - start[0];
    float distance = start[1] * y + start[0] * x;
    if (self->ent->r.currentOrigin[1] * y + self->ent->r.currentOrigin[0] * x > distance)
    {
        x = -x;
        y = -y;
        distance = -distance;
    }
    record.normal[0] = x;
    record.normal[1] = y;
    record.dist = distance;
    if (self->sentient->eTeam == suppressor->eTeam)
    {
        record.friendly = 1;
        if (self->eState[self->stateLevel] == AIS_EXPOSED && Actor_HasPath(self))
        {
            float point[3];
            Actor_LinePoint_SP(self->ent->r.currentOrigin, start, end, point);
            const float dx = point[0] - self->ent->r.currentOrigin[0];
            const float dy = point[1] - self->ent->r.currentOrigin[1];
            if (self->Path.lookaheadDir[1] * dy + self->Path.lookaheadDir[0] * dx > 0.0f)
                Actor_ClearPath(self);
        }
        return;
    }
    if (self->ignoreSuppression)
    {
        Actor_BulletWhizby_SP(self, suppressor, start, end, closest);
        return;
    }
    record.friendly = 0;
    if (!self->suppressionStartTime)
        self->suppressionStartTime = level.time;
    Scr_AddEntity(suppressor->ent, SCRIPTINSTANCE_SERVER);
    Scr_Notify(self->ent, scr_const.suppression, 1u);
}

void Actor_UpdateSuppression_SP(actor_s *self)
{
    const bool wasSuppressed = self->suppressionStartTime > 0;
    float meter = self->ignoreSuppression ? 0.0f : self->suppressionMeter - 0.01f;
    if (-meter >= 0.0f)
        meter = 0.0f;
    self->suppressionMeter = meter;
    bool remainsSuppressed = false;
    int now = level.time;
    for (int i = 0; i < 4; ++i)
    {
        actor_suppression_s &record = self->suppression[i];
        if (record.time <= 0)
            continue;
        const int duration = record.friendly ? ai_friendlyFireBlockDuration->current.integer
            : self->ignoreSuppression ? 0 : self->suppressionDuration;
        if (Actor_ElapsedTime_SP(now, record.time) >= duration)
        {
            record.time = 0;
            record.suppressor = NULL;
            record.friendly = 0;
            if (i != 3)
                now = level.time;
        }
        else if (!record.friendly)
            remainsSuppressed = true;
    }
    if (!remainsSuppressed)
    {
        if (wasSuppressed)
            Scr_Notify(self->ent, scr_const.suppression_end, 0u);
        self->suppressionStartTime = 0;
    }
}

void Actor_ClearSuppressionFrom_SP(actor_s *self, sentient_s *other)
{
    const bool wasSuppressed = self->suppressionStartTime > 0;
    bool remainsSuppressed = false;
    for (actor_suppression_s &record : self->suppression)
    {
        if (record.suppressor == other)
        {
            record.time = 0;
            record.suppressor = NULL;
            record.friendly = 0;
        }
        else if (record.time > 0 && !record.friendly)
            remainsSuppressed = true;
    }
    if (!remainsSuppressed)
    {
        if (wasSuppressed)
            Scr_Notify(self->ent, scr_const.suppression_end, 0u);
        self->suppressionStartTime = 0;
    }
}

int Actor_GetSuppressionPlanes_SP(const actor_s *self, float (*normal)[2], float *dist)
{
    int count = 0;
    for (const actor_suppression_s &record : self->suppression)
    {
        if (record.time != 0)
        {
            normal[count][0] = record.normal[0];
            normal[count][1] = record.normal[1];
            dist[count++] = record.dist;
        }
    }
    return count;
}

bool Actor_IsSuppressed_SP(const actor_s *self)
{
    return self->suppressionStartTime > 0;
}

bool Actor_HasActiveSuppression_SP(const actor_s *self)
{
    if ( self->suppressionStartTime > 0 )
        return true;
    for ( const actor_suppression_s &entry : self->suppression )
        if ( entry.time )
            return true;
    return false;
}

void __fastcall Actor_StateReact_SP(actor_s *, gentity_s *, const float *)
{
}

bool Actor_UsingCoverAnim_SP(const actor_s *self)
{
    const AnimScriptList &human = g_scr_data.humanAnim;
    const scr_animscript_t *script = self->pAnimScriptFunc;
    return script == &human.cover_left || script == &human.cover_right
        || script == &human.cover_stand || script == &human.cover_crouch
        || script == &human.cover_wide_left || script == &human.cover_wide_right
        || script == &human.cover_pillar;
}

void __cdecl Actor_React_SP(gentity_s *self, gentity_s *originator, const float *closest)
{
    actor_s *actor = self->actor;
    Vec3Copy(closest, actor->reactOrigin);
    Scr_SetStringFromCharString(&actor->damageWeapon, const_cast<char *>(BG_WeaponName(originator ? originator->s.weapon : 0)), SCRIPTINSTANCE_SERVER);
    AIFuncTable[actor->species][actor->eState[actor->stateLevel]].pfnReact(actor, originator, closest);
    self->sentient->lastAttacker.setEnt(originator);
    if (originator->sentient)
        Actor_WasAttackedBy(actor, originator->sentient);
    if (!actor->sentient->syncedMeleeEnt.isDefined() && actor->allowReact
        && actor->eState[actor->stateLevel] != AIS_DEATH
        && actor->eState[actor->stateLevel] != AIS_REACT && !Actor_UsingCoverAnim_SP(actor))
    {
        const unsigned short thread = Scr_ExecEntThread(self, g_scr_data.actorShouldReact, 0u);
        Scr_FreeThread(thread, SCRIPTINSTANCE_SERVER);
    }
}

static void Actor_AnimReact_SP(actor_s *self)
{
    Actor_SetAnimScript(self, &g_animScriptTable[self->species]->react, AI_MOVE_STOP,
        AI_ANIM_USE_BOTH_DELTAS, AI_ANIM_FUNCTION_REACT);
    self->bUseGoalWeight = false;
}

bool __fastcall Actor_React_Start_SP(actor_s *self, ai_state_t)
{
    self->iReactTime = level.time;
    Actor_ClearPath(self);
    Actor_AnimReact_SP(self);
    Scr_Notify(self->ent, scr_const.react, 0u);
    return true;
}

void __fastcall Actor_React_Finish_SP(actor_s *self, ai_state_t)
{
    if (self->sentient->lastAttacker.isDefined())
    {
        Actor_BroadcastTeamEvent(self->sentient, AI_EV_REACT);
        self->sentient->lastAttacker.setEnt(NULL);
    }
}

actor_think_result_t __fastcall Actor_React_Think_SP(actor_s *self)
{
    self->pszDebugInfo = "react";
    if (level.time > self->iReactTime && !Actor_IsAnimScriptAlive(self))
    {
        Actor_PopState(self);
        return ACTOR_THINK_REPEAT;
    }
    if (self->sentient->lastAttacker.isDefined() && Actor_ElapsedTime_SP(level.time, self->iReactTime) >= 500)
    {
        Actor_BroadcastTeamEvent(self->sentient, AI_EV_REACT);
        self->sentient->lastAttacker.setEnt(NULL);
    }
    Actor_PreThink(self);
    Actor_SetOrientMode(self, AI_ORIENT_DONT_CHANGE);
    Actor_AnimReact_SP(self);
    Actor_PostThink(self);
    return ACTOR_THINK_DONE;
}

#endif
#ifdef KISAK_SP
void Actor_ReceiveBadPlaceEvent_SP(actor_s *self, float awareness, ai_badplace_t badplace)
{
    if (self->flashBanged)
        return;
    const int random = G_rand();
    volatile float threshold = awareness * 32767.0f;
    if (static_cast<float>(random) > threshold)
        return;
    self->aiBadPlace = badplace;
}

void Actor_BroadcastArcEvent_SP(gentity_s *originator, ai_event_t event, int teamflags,
    float *origin, float radius, float angle0, float angle1, float halfheight)
{
    if (angle0 == angle1)
        return;
    if (!teamflags)
    {
        if (!originator->sentient)
            return;
        const team_t enemyTeam = Sentient_EnemyTeam(originator->sentient->eTeam);
        if (!enemyTeam)
            return;
        teamflags = 1 << enemyTeam;
    }
    for (actor_s *actor = Actor_FirstActor(teamflags); actor; actor = Actor_NextActor(actor, teamflags))
    {
        if (Actor_IsInsideArc(actor, origin, radius, angle0, angle1, halfheight))
        {
            if (event == AI_EV_BADPLACE_ARC)
                Actor_ReceiveBadPlaceEvent_SP(actor, actor->badPlaceAwareness, AI_BADPLACE_NORMAL);
            else if (event == AI_EV_REALLY_BADPLACE_ARC)
                Actor_ReceiveBadPlaceEvent_SP(actor, actor->badPlaceAwareness, AI_BADPLACE_REALLYBAD);
        }
    }
    for (int listener = Actor_EventListener_First(event, teamflags); listener >= 0;
        listener = Actor_EventListener_Next(listener, event, teamflags))
    {
        gentity_s *ent = Actor_EventListener_GetEntity(listener);
        if (IsPosInsideArc(ent->r.currentOrigin, 0.0f, origin, radius, angle0, angle1, halfheight))
            Actor_EventListener_NotifyToListener(ent, originator, event, origin);
    }
}

static float Actor_VolumeDistanceSquared_SP(const float *point, const float *origin)
{
    volatile float dx = point[0] - origin[0];
    volatile float dz = point[2] - origin[2];
    volatile float dy = point[1] - origin[1];
    volatile float xx = dx * dx;
    volatile float zz = dz * dz;
    volatile float yy = dy * dy;
    volatile float xz = xx + zz;
    return xz + yy;
}

void Actor_BroadcastVolumeEvent_SP(gentity_s *originator, ai_event_t event, int teamflags,
    gentity_s *volume, float radius)
{
    if (!teamflags)
    {
        if (!originator->sentient)
            return;
        const team_t enemyTeam = Sentient_EnemyTeam(originator->sentient->eTeam);
        if (!enemyTeam)
            return;
        teamflags = 1 << enemyTeam;
    }
    volatile float radiusSquared = radius * radius;
    for (actor_s *actor = Actor_FirstActor(teamflags); actor; actor = Actor_NextActor(actor, teamflags))
    {
        float *point = actor->ent->r.currentOrigin;
        if (Actor_VolumeDistanceSquared_SP(point, volume->r.currentOrigin) < radiusSquared &&
            SV_EntityContact(point, point, volume))
        {
            if (event == AI_EV_BADPLACE_VOLUME)
                Actor_ReceiveBadPlaceEvent_SP(actor, actor->badPlaceAwareness, AI_BADPLACE_NORMAL);
        }
    }
    for (int listener = Actor_EventListener_First(event, teamflags); listener >= 0;
        listener = Actor_EventListener_Next(listener, event, teamflags))
    {
        gentity_s *ent = Actor_EventListener_GetEntity(listener);
        float *point = ent->r.currentOrigin;
        if (Actor_VolumeDistanceSquared_SP(point, volume->r.currentOrigin) < radiusSquared &&
            SV_EntityContact(point, point, volume))
            Actor_EventListener_NotifyToListener(ent, originator, event, volume->r.currentOrigin);
    }
}
#endif
double __fastcall Actor_EventDefaultRadiusSqrd(ai_event_t eType)
{
    const dvar_s *dvar; // [esp+4h] [ebp-4h]

    if ( !g_ai_event_info[eType].defaultDistDvar
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    78,
                    0,
                    "%s",
                    "g_ai_event_info[eType].defaultDistDvar") )
    {
        __debugbreak();
    }
    dvar = *g_ai_event_info[eType].defaultDistDvar;
    if ( !dvar && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 81, 0, "%s", "dvar") )
        __debugbreak();
    return dvar->current.value * dvar->current.value;
}

double __fastcall Actor_EventDefaultHeightDiff(ai_event_t eType)
{
    return g_ai_event_info[eType].defaultHeight;
}

void __fastcall Actor_GetPerfectInfo(actor_s *self, sentient_s *pOther)
{
    Actor_UpdateLastKnownPos(self, pOther);
}

void __fastcall Actor_WasAttackedBy(actor_s *self, sentient_s *pOther)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 162, 0, "%s", "self") )
        __debugbreak();
    if ( !pOther && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 163, 0, "%s", "pOther") )
        __debugbreak();
    if ( (pOther < level.sentients || pOther >= &level.sentients[48])
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    164,
                    0,
                    "%s",
                    "pOther >= level.sentients && pOther < level.sentients + MAX_SENTIENTS") )
    {
        __debugbreak();
    }
    if ( pOther != &level.sentients[pOther - level.sentients]
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    165,
                    0,
                    "%s",
                    "pOther == level.sentients + (pOther - level.sentients)") )
    {
        __debugbreak();
    }
    self->sentientInfo[pOther - level.sentients].iLastAttackMeTime = level.time;
}

void __fastcall Actor_BroadcastTeamEvent(sentient_s *sentient, ai_event_t eType)
{
    if ( !sentient
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 181, 0, "%s", "sentient") )
    {
        __debugbreak();
    }
    if ( !sentient->ent
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 182, 0, "%s", "sentient->ent") )
    {
        __debugbreak();
    }
    Actor_BroadcastPointEvent(sentient->ent, eType, 1 << sentient->eTeam, sentient->ent->r.currentOrigin, 0.0);
}

void __fastcall Actor_BroadcastPointEvent(
                gentity_s *originator,
                ai_event_t eType,
                int teamFlags,
                float *vOrigin,
                float fRadiusSqrd)
{
    gentity_s *ent; // [esp+38h] [ebp-14h]
    team_t eEnemyTeam; // [esp+3Ch] [ebp-10h]
    actor_s *pActor; // [esp+40h] [ebp-Ch]
    int listenerIndex; // [esp+44h] [ebp-8h]
    float fDistSqrd; // [esp+48h] [ebp-4h]
    float fDistSqrda; // [esp+48h] [ebp-4h]

    if ( teamFlags > 15
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    223,
                    0,
                    "%s",
                    "teamFlags <= (1 << TEAM_NUM_TEAMS) - 1") )
    {
        __debugbreak();
    }
    if ( (eType <= AI_EV_FIRST_POINT_EVENT || eType >= AI_EV_LAST_POINT_EVENT)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    224,
                    0,
                    "%s",
                    "eType > AI_EV_FIRST_POINT_EVENT && eType < AI_EV_LAST_POINT_EVENT") )
    {
        __debugbreak();
    }
    if ( teamFlags )
        goto LABEL_15;
    if ( !originator
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 231, 0, "%s", "originator") )
    {
        __debugbreak();
    }
    if ( originator->sentient )
    {
        eEnemyTeam = Sentient_EnemyTeam(originator->sentient->eTeam);
        if ( eEnemyTeam )
        {
            teamFlags = 1 << eEnemyTeam;
LABEL_15:
            if ( fRadiusSqrd == 0.0 )
                fRadiusSqrd = Actor_EventDefaultRadiusSqrd(eType);
            for ( pActor = Actor_FirstActor(teamFlags); pActor; pActor = Actor_NextActor(pActor, teamFlags) )
            {
#ifndef KISAK_SP
                if ( !Actor_IsOnSameTeam(originator, pActor, teamFlags) )
                {
#endif
                    fDistSqrd = Vec3DistanceSq(pActor->ent->r.currentOrigin, vOrigin);
                    if ( fRadiusSqrd >= fDistSqrd )
                        Actor_ReceivePointEvent(pActor, originator, eType, vOrigin, fDistSqrd, fRadiusSqrd);
#ifndef KISAK_SP
                }
#endif
            }
            for ( listenerIndex = Actor_EventListener_First(eType, teamFlags);
                        listenerIndex >= 0;
                        listenerIndex = Actor_EventListener_Next(listenerIndex, eType, teamFlags) )
            {
                ent = Actor_EventListener_GetEntity(listenerIndex);
                fDistSqrda = Vec3DistanceSq(ent->r.currentOrigin, vOrigin);
                if ( fRadiusSqrd >= fDistSqrda )
                    Actor_EventListener_NotifyToListener(ent, originator, eType, vOrigin);
            }
        }
    }
}

char __cdecl Actor_IsOnSameTeam(gentity_s *originator, actor_s *pActor, int teamFlags)
{
    gentity_s *v4; // esi

    if ( pActor == originator->actor )
        return 1;
    //if ( !EntHandle::isDefined(&pActor->sentient->scriptOwner) )
    if ( !pActor->sentient->scriptOwner.isDefined() )
        return 0;
    //if ( EntHandle::ent(&pActor->sentient->scriptOwner) == originator )
    if ( pActor->sentient->scriptOwner.ent() == originator)
        return 1;
    if ( !teamFlags
        && originator->sentient
        //&& EntHandle::isDefined(&originator->sentient->scriptOwner)
        && originator->sentient->scriptOwner.isDefined()
        //&& (v4 = EntHandle::ent(&pActor->sentient->scriptOwner), v4 == EntHandle::ent(&originator->sentient->scriptOwner)) )
        && (v4 = pActor->sentient->scriptOwner.ent(), v4 == originator->sentient->scriptOwner.ent()))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void __fastcall Actor_BroadcastLineEvent(
                gentity_s *originator,
                gentity_s *hitEnt,
                ai_event_t eType,
                int teamFlags,
                const float *vStart,
                const float *vEnd,
                float fRadiusSqrd)
{
    float *v9; // [esp+2Ch] [ebp-60h]
    float *currentOrigin; // [esp+34h] [ebp-58h]
    gentity_s *ent; // [esp+48h] [ebp-44h]
    team_t eEnemyTeam; // [esp+4Ch] [ebp-40h]
    actor_s *pActor; // [esp+50h] [ebp-3Ch]
    float fScaledDist; // [esp+54h] [ebp-38h]
    float fScaledDista; // [esp+54h] [ebp-38h]
    float fMaxHeightDiff; // [esp+58h] [ebp-34h]
    int listenerIndex; // [esp+5Ch] [ebp-30h]
    float fDistSqrd; // [esp+60h] [ebp-2Ch]
    float fLineLenSqrd; // [esp+64h] [ebp-28h]
    float vClosest[3]; // [esp+68h] [ebp-24h] BYREF
    float vLine[3]; // [esp+74h] [ebp-18h]
    float vDelta[3]; // [esp+80h] [ebp-Ch]

    if ( teamFlags > 15
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    296,
                    0,
                    "%s",
                    "teamFlags <= (1 << TEAM_NUM_TEAMS) - 1") )
    {
        __debugbreak();
    }
    if ( (eType <= AI_EV_FIRST_LINE_EVENT || eType >= AI_EV_LAST_LINE_EVENT)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    297,
                    0,
                    "%s",
                    "eType > AI_EV_FIRST_LINE_EVENT && eType < AI_EV_LAST_LINE_EVENT") )
    {
        __debugbreak();
    }
    if ( !teamFlags )
    {
        if ( !originator->sentient )
            return;
        eEnemyTeam = Sentient_EnemyTeam(originator->sentient->eTeam);
        if ( eEnemyTeam == TEAM_FREE )
            return;
        teamFlags = 1 << eEnemyTeam;
    }
    if ( fRadiusSqrd == 0.0 )
        fRadiusSqrd = Actor_EventDefaultRadiusSqrd(eType);
    fMaxHeightDiff = Actor_EventDefaultHeightDiff(eType);
    vLine[0] = *vEnd - *vStart;
    vLine[1] = vEnd[1] - vStart[1];
    vLine[2] = vEnd[2] - vStart[2];
    fLineLenSqrd = (float)((float)(vLine[0] * vLine[0]) + (float)(vLine[1] * vLine[1])) + (float)(vLine[2] * vLine[2]);
    for ( pActor = Actor_FirstActor(teamFlags); pActor; pActor = Actor_NextActor(pActor, teamFlags) )
    {
        if ( !Actor_IsOnSameTeam(originator, pActor, teamFlags) )
        {
            currentOrigin = pActor->ent->r.currentOrigin;
            vDelta[0] = *currentOrigin - *vStart;
            vDelta[1] = currentOrigin[1] - vStart[1];
            vDelta[2] = currentOrigin[2] - vStart[2];
            fScaledDist = (float)((float)(vDelta[0] * vLine[0]) + (float)(vDelta[1] * vLine[1]))
                                    + (float)(vDelta[2] * vLine[2]);
            if ( fScaledDist >= 0.0 )
            {
                if ( fScaledDist < fLineLenSqrd )
                {
                    if ( fLineLenSqrd == 0.0
                        && !Assert_MyHandler(
                                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                                    365,
                                    0,
                                    "%s",
                                    "fLineLenSqrd") )
                    {
                        __debugbreak();
                    }
                    vClosest[0] = (float)((float)(fScaledDist / fLineLenSqrd) * vLine[0]) + *vStart;
                    vClosest[1] = (float)((float)(fScaledDist / fLineLenSqrd) * vLine[1]) + vStart[1];
                    vClosest[2] = (float)((float)(fScaledDist / fLineLenSqrd) * vLine[2]) + vStart[2];
                }
                else
                {
                    vClosest[0] = *vEnd;
                    vClosest[1] = vEnd[1];
                    vClosest[2] = vEnd[2];
                }
                v9 = pActor->ent->r.currentOrigin;
                vDelta[0] = *v9 - vClosest[0];
                vDelta[1] = v9[1] - vClosest[1];
                vDelta[2] = v9[2] - vClosest[2];
                vDelta[2] = (float)((float)(48.0 + 0.0) * 0.5) + vDelta[2];
                fDistSqrd = (float)(vDelta[0] * vDelta[0]) + (float)(vDelta[1] * vDelta[1]);
                if ( fRadiusSqrd >= fDistSqrd
                    && fabs(fMaxHeightDiff) > fabs(vDelta[2]) )
                {
                    Actor_ReceiveLineEvent(pActor, originator, hitEnt, eType, vStart, vEnd, vClosest, fDistSqrd, fRadiusSqrd);
                }
            }
        }
    }
    for ( listenerIndex = Actor_EventListener_First(eType, teamFlags);
                listenerIndex >= 0;
                listenerIndex = Actor_EventListener_Next(listenerIndex, eType, teamFlags) )
    {
        ent = Actor_EventListener_GetEntity(listenerIndex);
        vDelta[0] = ent->r.currentOrigin[0] - *vStart;
        vDelta[1] = ent->r.currentOrigin[1] - vStart[1];
        vDelta[2] = ent->r.currentOrigin[2] - vStart[2];
        fScaledDista = (float)((float)(vDelta[0] * vLine[0]) + (float)(vDelta[1] * vLine[1]))
                                 + (float)(vDelta[2] * vLine[2]);
        if ( fScaledDista >= 0.0 )
        {
            if ( fScaledDista < fLineLenSqrd )
            {
                if ( fLineLenSqrd == 0.0
                    && !Assert_MyHandler(
                                "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                                410,
                                0,
                                "%s",
                                "fLineLenSqrd") )
                {
                    __debugbreak();
                }
                vClosest[0] = (float)((float)(fScaledDista / fLineLenSqrd) * vLine[0]) + *vStart;
                vClosest[1] = (float)((float)(fScaledDista / fLineLenSqrd) * vLine[1]) + vStart[1];
                vClosest[2] = (float)((float)(fScaledDista / fLineLenSqrd) * vLine[2]) + vStart[2];
            }
            else
            {
                vClosest[0] = *vEnd;
                vClosest[1] = vEnd[1];
                vClosest[2] = vEnd[2];
            }
            vDelta[0] = ent->r.currentOrigin[0] - vClosest[0];
            vDelta[1] = ent->r.currentOrigin[1] - vClosest[1];
            vDelta[2] = ent->r.currentOrigin[2] - vClosest[2];
            if ( fRadiusSqrd >= (float)((float)(vDelta[0] * vDelta[0]) + (float)(vDelta[1] * vDelta[1]))
                && fabs(fMaxHeightDiff) > fabs(vDelta[2]) )
            {
                Actor_EventListener_NotifyToListener(ent, originator, eType, vClosest);
            }
        }
    }
}

void __fastcall Actor_ReceivePointEvent(
                actor_s *self,
                gentity_s *originator,
                ai_event_t eType,
                float *vOrigin,
                float fDistSqrd,
                float fRadiusSqrd)
{
    gentity_s *v6; // eax
    gentity_s *v7; // eax
    gentity_s *v8; // eax
    gentity_s *v9; // eax

    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 784, 0, "%s", "self") )
        __debugbreak();
    if ( !vOrigin
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 785, 0, "%s", "vOrigin") )
    {
        __debugbreak();
    }
    if ( (fDistSqrd < 0.0 || fRadiusSqrd < fDistSqrd)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    786,
                    0,
                    "%s",
                    "fDistSqrd >= 0 && fDistSqrd <= fRadiusSqrd") )
    {
        __debugbreak();
    }
    if ( (eType <= AI_EV_FIRST_POINT_EVENT || eType >= AI_EV_LAST_POINT_EVENT)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    787,
                    0,
                    "%s",
                    "eType > AI_EV_FIRST_POINT_EVENT && eType < AI_EV_LAST_POINT_EVENT") )
    {
        __debugbreak();
    }
    switch ( eType )
    {
        case AI_EV_FOOTSTEP:
        case AI_EV_FOOTSTEP_LITE:
            if ( !originator
                && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 793, 0, "%s", "originator") )
            {
                __debugbreak();
            }
            if ( !originator->sentient
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                            794,
                            0,
                            "%s",
                            "originator->sentient") )
            {
                __debugbreak();
            }
            if ( Actor_CaresAboutInfo(self, originator->sentient) )
            {
                Actor_EventFootstep(self, originator->sentient, vOrigin);
                break;
            }
            return;
        case AI_EV_NEW_ENEMY:
            if ( !originator
                && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 809, 0, "%s", "originator") )
            {
                __debugbreak();
            }
            if ( !originator->sentient
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                            810,
                            0,
                            "%s",
                            "originator->sentient") )
            {
                __debugbreak();
            }
            Actor_EventNewEnemy(self, originator->sentient);
            break;
        case AI_EV_PAIN:
            if ( !originator
                && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 816, 0, "%s", "originator") )
            {
                __debugbreak();
            }
            if ( !originator->sentient
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                            817,
                            0,
                            "%s",
                            "originator->sentient") )
            {
                __debugbreak();
            }
            //if ( !EntHandle::isDefined(&originator->sentient->lastAttacker)
            if ( !originator->sentient->lastAttacker.isDefined()
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                            818,
                            0,
                            "%s",
                            "originator->sentient->lastAttacker.isDefined()") )
            {
                __debugbreak();
            }
            //if ( !EntHandle::ent(&originator->sentient->lastAttacker)->sentient )
            if ( !originator->sentient->lastAttacker.ent()->sentient)
                return;
            //v6 = EntHandle::ent(&originator->sentient->lastAttacker);
            v6 = originator->sentient->lastAttacker.ent();
            if ( !Actor_CaresAboutInfo(self, v6->sentient) )
                return;
            //v7 = EntHandle::ent(&originator->sentient->lastAttacker);
            v7 = originator->sentient->lastAttacker.ent();
            Actor_EventPain(self, originator->sentient, v7->sentient);
            break;
        case AI_EV_REACT:
            break;
        case AI_EV_DEATH:
            if ( !originator
                && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 845, 0, "%s", "originator") )
            {
                __debugbreak();
            }
            if ( !originator->sentient
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                            846,
                            0,
                            "%s",
                            "originator->sentient") )
            {
                __debugbreak();
            }
            //if ( !EntHandle::isDefined(&originator->sentient->lastAttacker) )
            if ( !originator->sentient->lastAttacker.isDefined() )
                return;
            //if ( !EntHandle::ent(&originator->sentient->lastAttacker)->sentient )
            if ( !originator->sentient->lastAttacker.ent()->sentient)
                return;
            //v8 = EntHandle::ent(&originator->sentient->lastAttacker);
            v8 = originator->sentient->lastAttacker.ent();
            if ( !Actor_CaresAboutInfo(self, v8->sentient) )
                return;
            //v9 = EntHandle::ent(&originator->sentient->lastAttacker);
            v9 = originator->sentient->lastAttacker.ent();
            Actor_EventDeath(self, originator->sentient, v9->sentient);
            break;
        case AI_EV_EXPLOSION:
            Actor_EventExplosion(self, originator, vOrigin);
            break;
        case AI_EV_GRENADE_PING:
            goto LABEL_60;
        case AI_EV_PROJECTILE_PING:
            if ( !originator
                && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 865, 0, "%s", "originator") )
            {
                __debugbreak();
            }
            //if ( EntHandle::isDefined(&originator->parent) && EntHandle::ent(&originator->parent) == self->ent )
            if ( originator->parent.isDefined() && originator->parent.ent() == self->ent)
                return;
LABEL_60:
            Actor_EventGrenadePing(self, originator, vOrigin);
            break;
        case AI_EV_GUNSHOT:
        case AI_EV_SILENCED_SHOT:
            if ( !originator
                && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 874, 0, "%s", "originator") )
            {
                __debugbreak();
            }
            if ( !originator->sentient
                && !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                            875,
                            0,
                            "%s",
                            "originator->sentient") )
            {
                __debugbreak();
            }
            if ( originator == self->ent || !Actor_CaresAboutInfo(self, originator->sentient) )
                return;
            Actor_EventGunshot(self, originator->sentient, vOrigin);
            break;
        default:
            if ( !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                            887,
                            0,
                            "Actor_ReceivePointEvent: unhandled case\n") )
                __debugbreak();
            return;
    }
    Actor_DumpEvents(self, eType, originator);
}

void __cdecl Actor_DumpEvents(actor_s *self, ai_event_t event, gentity_s *originator)
{
    int number; // [esp+0h] [ebp-8h]

    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 103, 0, "%s", "self") )
        __debugbreak();
    if ( g_dumpAIEvents->current.integer == self->ent->s.number )
    {
        if ( originator )
            number = originator->s.number;
        else
            number = -1;
        Com_Printf(
            18,
            "%d ^3 %s^7:    from entity^5 %d ^7at time^5 %d\n",
            self->ent->s.number,
            g_ai_event_info[event].name,
            number,
            level.time);
    }
}

void __fastcall Actor_EventFootstep(actor_s *self, sentient_s *originator, const float *vOrigin)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 586, 0, "%s", "self") )
        __debugbreak();
    if ( !originator
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 587, 0, "%s", "originator") )
    {
        __debugbreak();
    }
    if ( !vOrigin
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 588, 0, "%s", "vOrigin") )
    {
        __debugbreak();
    }
    Actor_UpdateLastKnownPos(self, originator);
}

void __fastcall Actor_EventNewEnemy(actor_s *self, sentient_s *originator)
{
    sentient_s *enemy; // [esp+8h] [ebp-4h]

    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 604, 0, "%s", "self") )
        __debugbreak();
    if ( !originator
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 605, 0, "%s", "originator") )
    {
        __debugbreak();
    }
    if ( !originator->ent
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 606, 0, "%s", "originator->ent") )
    {
        __debugbreak();
    }
    //if ( !EntHandle::isDefined(&originator->targetEnt)
    if ( !originator->targetEnt.isDefined()
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    607,
                    0,
                    "%s",
                    "originator->targetEnt.isDefined()") )
    {
        __debugbreak();
    }
    //if ( !EntHandle::ent(&originator->targetEnt)->sentient
    if ( !originator->targetEnt.ent()->sentient
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    608,
                    0,
                    "%s",
                    "originator->targetEnt.ent()->sentient") )
    {
        __debugbreak();
    }
    //enemy = EntHandle::ent(&originator->targetEnt)->sentient;
    enemy = originator->targetEnt.ent()->sentient;
    if ( originator->ent->actor )
        SentientInfo_Copy(self, originator->ent->actor, enemy - level.sentients);
    else
        Actor_GetPerfectInfo(self, enemy);
}

void __fastcall Actor_EventPain(actor_s *self, sentient_s *pCasualty, sentient_s *pAttacker)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 634, 0, "%s", "self") )
        __debugbreak();
    if ( !pCasualty
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 635, 0, "%s", "pCasualty") )
    {
        __debugbreak();
    }
    if ( !pAttacker
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 636, 0, "%s", "pAttacker") )
    {
        __debugbreak();
    }
    Actor_WasAttackedBy(self, pAttacker);
    Actor_GetPerfectInfo(self, pAttacker);
}

void __fastcall Actor_EventDeath(actor_s *self, sentient_s *pCasualty, sentient_s *pAttacker)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 675, 0, "%s", "self") )
        __debugbreak();
    if ( !pCasualty
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 676, 0, "%s", "pCasualty") )
    {
        __debugbreak();
    }
    if ( !pAttacker
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 677, 0, "%s", "pAttacker") )
    {
        __debugbreak();
    }
    Actor_GetPerfectInfo(self, pAttacker);
}

void __cdecl Actor_EventExplosion(actor_s *self, gentity_s *originator, float *vOrigin)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 692, 0, "%s", "self") )
        __debugbreak();
    if ( !vOrigin
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 693, 0, "%s", "vOrigin") )
    {
        __debugbreak();
    }
    if ( originator )
        Scr_AddEntity(originator, SCRIPTINSTANCE_SERVER);
    else
        Scr_AddUndefined(SCRIPTINSTANCE_SERVER);
    Scr_AddVector(vOrigin, SCRIPTINSTANCE_SERVER);
    Scr_Notify(self->ent, scr_const.explode, 2u);
    ScrNotify_FaceEvent(self->ent, scr_const.explode);
}

void __cdecl Actor_EventGrenadePing(actor_s *self, gentity_s *originator, const float *vOrigin)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 717, 0, "%s", "self") )
        __debugbreak();
    if ( !originator
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 718, 0, "%s", "originator") )
    {
        __debugbreak();
    }
    if ( !vOrigin
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 719, 0, "%s", "vOrigin") )
    {
        __debugbreak();
    }
#ifdef KISAK_SP
    Actor_Grenade_Ping_SP(self, originator);
#endif
}

void __fastcall Actor_EventGunshot(actor_s *self, sentient_s *originator, const float *vOrigin)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 737, 0, "%s", "self") )
        __debugbreak();
    if ( !originator
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 738, 0, "%s", "originator") )
    {
        __debugbreak();
    }
    if ( !vOrigin
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 739, 0, "%s", "vOrigin") )
    {
        __debugbreak();
    }
    Actor_GetPerfectInfo(self, originator);
}

#ifdef KISAK_SP
void __fastcall Actor_ReceiveLineEvent(
                actor_s *self,
                gentity_s *originator,
                gentity_s *hitEnt,
                ai_event_t eType,
                const float *vStart,
                const float *vEnd,
                const float *vClosest,
                float fDistSqrd,
                float fRadiusSqrd)
{
    iassert(self && originator && vStart && vEnd && vClosest);
    iassert(fDistSqrd >= 0.0f && fDistSqrd <= fRadiusSqrd);
    if (eType == AI_EV_PROJECTILE_IMPACT)
    {
        if (originator->sentient)
        {
            Actor_WasAttackedBy(self, originator->sentient);
            Actor_GetPerfectInfo(self, originator->sentient);
        }
        return;
    }
    if (eType != AI_EV_BULLET && eType != AI_EV_BLOCK_FRIENDLIES && eType != AI_EV_BULLET_REACT)
        return;
    if (originator == self->ent)
        return;
    if (eType != AI_EV_BULLET_REACT)
        Actor_EventBullet(self, originator, vStart, vEnd, vClosest, fDistSqrd, fRadiusSqrd, DO_SUPPRESSION);
    if (originator == self->ent)
        return;
    if (!ai_reactFeature->current.enabled || self->species != AI_SPECIES_HUMAN || eType == AI_EV_BLOCK_FRIENDLIES)
        return;
    if (hitEnt == self->ent)
    {
        self->lastBulletHitTime = level.time;
        return;
    }
    if (self->lastBulletHitTime != -1 && self->lastBulletHitTime == level.time)
        return;
    void (*react)(gentity_s *, gentity_s *, const float *) = entityHandlers[self->ent->handler].react;
    if (react)
        react(self->ent, originator, vClosest);
}
#else
void __fastcall Actor_ReceiveLineEvent(
                actor_s *self,
                gentity_s *originator,
                gentity_s *hitEnt,
                ai_event_t eType,
                const float *vStart,
                const float *vEnd,
                const float *vClosest,
                float fDistSqrd,
                float fRadiusSqrd)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 904, 0, "%s", "self") )
        __debugbreak();
    if ( !vStart && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 905, 0, "%s", "vStart") )
        __debugbreak();
    if ( !vEnd && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 906, 0, "%s", "vEnd") )
        __debugbreak();
    if ( !vClosest
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 907, 0, "%s", "vClosest") )
    {
        __debugbreak();
    }
    if ( (fDistSqrd < 0.0 || fRadiusSqrd < fDistSqrd)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    908,
                    0,
                    "%s",
                    "fDistSqrd >= 0 && fDistSqrd <= fRadiusSqrd") )
    {
        __debugbreak();
    }
    if ( (eType <= AI_EV_FIRST_LINE_EVENT || eType >= AI_EV_LAST_LINE_EVENT)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    909,
                    0,
                    "%s",
                    "eType > AI_EV_FIRST_LINE_EVENT && eType < AI_EV_LAST_LINE_EVENT") )
    {
        __debugbreak();
    }
    if ( eType < AI_EV_BULLET )
        goto LABEL_31;
    if ( eType > AI_EV_BLOCK_FRIENDLIES )
    {
        if ( eType != AI_EV_PROJECTILE_IMPACT )
        {
LABEL_31:
            if ( !Assert_MyHandler(
                            "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                            959,
                            0,
                            "Actor_ReceiveLineEvent: unhandled case\n") )
                __debugbreak();
            return;
        }
        Actor_EventBullet(self, originator, vStart, vEnd, vClosest, fDistSqrd, fRadiusSqrd, DONT_SUPPRESS);
LABEL_34:
        Actor_DumpEvents(self, eType, originator);
        return;
    }
    if ( !originator
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 915, 0, "%s", "originator") )
    {
        __debugbreak();
    }
    if ( originator != self->ent )
    {
        Actor_EventBullet(self, originator, vStart, vEnd, vClosest, fDistSqrd, fRadiusSqrd, DO_SUPPRESSION);
        goto LABEL_34;
    }
}
#endif

void __fastcall Actor_EventBullet(
                actor_s *self,
                gentity_s *originator,
                const float *vStart,
                const float *vEnd,
                const float *vClosest,
                float fDistSqrd,
                float fRadiusSqrd,
                PARM_SUPPRESSION suppression)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 755, 0, "%s", "self") )
        __debugbreak();
    if ( !originator
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 756, 0, "%s", "originator") )
    {
        __debugbreak();
    }
    if ( !vStart && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 757, 0, "%s", "vStart") )
        __debugbreak();
    if ( !vEnd && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 758, 0, "%s", "vEnd") )
        __debugbreak();
    if ( !vClosest
        && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp", 759, 0, "%s", "vClosest") )
    {
        __debugbreak();
    }
    if ( (fDistSqrd < 0.0 || fRadiusSqrd < fDistSqrd)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_events.cpp",
                    760,
                    0,
                    "%s",
                    "fDistSqrd >= 0 && fDistSqrd <= fRadiusSqrd") )
    {
        __debugbreak();
    }
    if ( originator->sentient )
    {
        Actor_WasAttackedBy(self, originator->sentient);
        Actor_GetPerfectInfo(self, originator->sentient);
#ifdef KISAK_SP
        if (suppression == DO_SUPPRESSION)
            Actor_Suppress_SP(self, originator->sentient, vStart, vEnd, vClosest);
#endif
    }
}

