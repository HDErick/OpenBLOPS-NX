#include "actor_aim.h"
#include "actor_senses.h"
#include "g_weapon.h"
#include <clientscript/cscr_stringlist.h>
#include <game_mp/g_main_mp.h>
#include <game_mp/actor_mp.h>
#ifdef KISAK_SP
#include <game_mp/g_utils_mp.h>
#include "actor_events.h"
#include "bullet.h"
#include <bgame/bg_weapons_def.h>
#include <bgame/bg_misc.h>
#include <qcommon/common.h>
#endif

void __cdecl Actor_FillWeaponParms(actor_s *self, weaponParms *wp)
{
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_aim.cpp", 437, 0, "%s", "self") )
        __debugbreak();
    if ( !wp && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_aim.cpp", 438, 0, "%s", "wp") )
        __debugbreak();
    if ( ((LODWORD(self->vLookForward[0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(self->vLookForward[1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(self->vLookForward[2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_aim.cpp",
                    440,
                    0,
                    "%s",
                    "!IS_NAN((self->vLookForward)[0]) && !IS_NAN((self->vLookForward)[1]) && !IS_NAN((self->vLookForward)[2])") )
    {
        __debugbreak();
    }
    if ( ((LODWORD(self->vLookRight[0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(self->vLookRight[1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(self->vLookRight[2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_aim.cpp",
                    441,
                    0,
                    "%s",
                    "!IS_NAN((self->vLookRight)[0]) && !IS_NAN((self->vLookRight)[1]) && !IS_NAN((self->vLookRight)[2])") )
    {
        __debugbreak();
    }
    if ( ((LODWORD(self->vLookUp[0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(self->vLookUp[1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(self->vLookUp[2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_aim.cpp",
                    442,
                    0,
                    "%s",
                    "!IS_NAN((self->vLookUp)[0]) && !IS_NAN((self->vLookUp)[1]) && !IS_NAN((self->vLookUp)[2])") )
    {
        __debugbreak();
    }
    wp->forward[0] = self->vLookForward[0];
    wp->forward[1] = self->vLookForward[1];
    wp->forward[2] = self->vLookForward[2];
    wp->right[0] = self->vLookRight[0];
    wp->right[1] = self->vLookRight[1];
    wp->right[2] = self->vLookRight[2];
    wp->up[0] = self->vLookUp[0];
    wp->up[1] = self->vLookUp[1];
    wp->up[2] = self->vLookUp[2];
    if ( Actor_GetMuzzleInfo(self, wp->muzzleTrace, wp->gunForward) )
    {
        wp->forward[0] = wp->gunForward[0];
        wp->forward[1] = wp->gunForward[1];
        wp->forward[2] = wp->gunForward[2];
    }
    else
    {
        Actor_GetEyePosition(self, wp->muzzleTrace);
        wp->gunForward[0] = wp->forward[0];
        wp->gunForward[1] = wp->forward[1];
        wp->gunForward[2] = wp->forward[2];
    }
    if ( ((LODWORD(wp->muzzleTrace[0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(wp->muzzleTrace[1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(wp->muzzleTrace[2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_aim.cpp",
                    458,
                    0,
                    "%s",
                    "!IS_NAN((wp->muzzleTrace)[0]) && !IS_NAN((wp->muzzleTrace)[1]) && !IS_NAN((wp->muzzleTrace)[2])") )
    {
        __debugbreak();
    }
    if ( ((LODWORD(wp->gunForward[0]) & 0x7F800000) == 0x7F800000
         || (LODWORD(wp->gunForward[1]) & 0x7F800000) == 0x7F800000
         || (LODWORD(wp->gunForward[2]) & 0x7F800000) == 0x7F800000)
        && !Assert_MyHandler(
                    "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_aim.cpp",
                    459,
                    0,
                    "%s",
                    "!IS_NAN((wp->gunForward)[0]) && !IS_NAN((wp->gunForward)[1]) && !IS_NAN((wp->gunForward)[2])") )
    {
        __debugbreak();
    }
}

#ifdef KISAK_SP
static float Actor_ShotLength_SP(const float *v, int first, int second, int third)
{
    const volatile float a = v[first] * v[first];
    const volatile float b = v[second] * v[second];
    const volatile float c = v[third] * v[third];
    const volatile float ab = a + b;
    return sqrtf(ab + c);
}

static void Actor_NormalizeShot_SP(float *v, int first, int second, int third)
{
    float length = Actor_ShotLength_SP(v, first, second, third);
    if (-length >= 0.0f)
        length = 1.0f;
    const float inverse = 1.0f / length;
    for (int i = 0; i != 3; ++i)
        v[i] = v[i] * inverse;
}

static void Actor_ShotSide_SP(const float *direction, const float *up, float *side,
    int first, int second, int third)
{
    const volatile float yz = direction[1] * up[2];
    const volatile float zy = direction[2] * up[1];
    const volatile float zx = direction[2] * up[0];
    const volatile float xz = direction[0] * up[2];
    const volatile float xy = direction[0] * up[1];
    const volatile float yx = direction[1] * up[0];
    side[0] = yz - zy;
    side[1] = zx - xz;
    side[2] = xy - yx;
    Actor_NormalizeShot_SP(side, first, second, third);
}

static float Actor_PlayerLateralAccuracy_SP(actor_s *self, sentient_s *enemy)
{
    float origin[3], target[3], direction[3];
    Sentient_GetOrigin(self->sentient, origin);
    Sentient_GetOrigin(enemy, target);
    for (int i = 0; i != 3; ++i)
        direction[i] = target[i] - origin[i];
    Actor_NormalizeShot_SP(direction, 2, 1, 0);
    const float *velocity = enemy->ent->client->ps.velocity;
    const volatile float z = velocity[2] * direction[2];
    const volatile float y = velocity[1] * -direction[0];
    const volatile float x = velocity[0] * direction[1];
    const volatile float zy = z + y;
    const float speed = fabsf(zy + x);
    const float limit = ai_accu_player_lateral_speed->current.value;
    float capped = speed;
    if (speed - limit >= 0.0f)
        capped = limit;
    float bounded = 0.0f;
    if (!(-speed >= 0.0f))
        bounded = capped;
    const volatile float ratio = bounded / limit;
    float accuracy = 1.0f - ratio;
    if (0.1f > accuracy)
        accuracy = 0.1f;
    return accuracy;
}

static float Actor_WeaponGraphAccuracy_SP(actor_s *self, sentient_s *enemy,
    const WeaponVariantDef *weapon, bool player)
{
    float origin[3], target[3], direction[3];
    Sentient_GetOrigin(self->sentient, origin);
    Sentient_GetOrigin(enemy, target);
    for (int i = 0; i != 3; ++i)
        direction[i] = origin[i] - target[i];
    float fraction = Actor_ShotLength_SP(direction, 2, player ? 0 : 1, player ? 1 : 0);
    if (player)
        fraction = fraction * ai_accuracyDistScale->current.value;
    fraction = fraction * 0.00025f;
    if (fraction > 1.0f)
        fraction = 1.0f;
    const int graph = player ? 1 : 0;
    const WeaponDef *def = weapon->weapDef;
    if (!def->accuracyGraphKnots[graph] || !def->accuracyGraphKnotCount[graph])
    {
        const char *name = BG_WeaponName(BG_GetWeaponIndex(weapon));
        if (player)
            Com_Error(ERR_DROP, "\x15No 'AI vs Player' accuracy graph for weapon '%s'", name);
        else
            Com_Error(ERR_DROP, "\x15No 'AI vs AI' accuracy graph for weapon '%s'", name);
    }
    return static_cast<float>(GraphGetValueFromFraction(def->accuracyGraphKnotCount[graph],
        def->accuracyGraphKnots[graph], fraction));
}

static float Actor_ShotAccuracy_SP(actor_s *self, float accuracyMod, const weaponParms *wp)
{
    sentient_s *enemy = Actor_GetTargetSentient(self);
    const bool player = enemy->ent->client != nullptr;
    const float graph = Actor_WeaponGraphAccuracy_SP(self, enemy, wp->weapVariantDef, player);
    float stance = 1.0f, lateral = 1.0f, sight = 1.0f;
    if (player)
    {
        const int flags = enemy->ent->client->ps.pm_flags;
        stance = flags & 1 ? 0.5f : (flags & 2 ? 0.75f : 1.0f);
        lateral = Actor_PlayerLateralAccuracy_SP(self, enemy);
        sight = self->playerSightAccuracy;
    }
    volatile float accuracy = self->accuracy * enemy->attackerAccuracy;
    accuracy = accuracy * accuracyMod;
    accuracy = accuracy * graph;
    accuracy = accuracy * stance;
    accuracy = accuracy * lateral;
    accuracy = accuracy * sight;
    float capped = 1.0f;
    if (!(accuracy - 1.0f >= 0.0f))
        capped = accuracy;
    float result = 0.0f;
    if (!(-accuracy >= 0.0f))
        result = capped;
    self->debugLastAccuracy = result;
    self->debugWeaponAccuracy = graph;
    return result;
}

static float Actor_ShotTargetBias_SP(const sentient_s *enemy)
{
    return enemy->ent->actor ? 6.0f : 10.0f;
}

static void Actor_ShootHitTarget_SP(sentient_s *enemy, unsigned short tag,
    float accuracy, int perfectAim, weaponParms *wp)
{
    float point[3], tagMatrix[4][3];
    if (tag && G_DObjGetWorldTagMatrix(enemy->ent, tag, tagMatrix))
        Vec3Copy(tagMatrix[3], point);
    else
        Sentient_GetEyePosition(enemy, point);
    const float bias = Actor_ShotTargetBias_SP(enemy);
    if (!perfectAim)
    {
        float direction[3], side[3];
        for (int i = 0; i != 3; ++i)
            direction[i] = point[i] - wp->muzzleTrace[i];
        Actor_NormalizeShot_SP(direction, 1, 2, 0);
        Actor_ShotSide_SP(direction, wp->up, side, 2, 1, 0);
        float horizontalScale, verticalScale;
        if (enemy->ent->client)
        {
            horizontalScale = 8.0f;
            verticalScale = -44.0f;
        }
        else
        {
            const float inverseAccuracy = 1.0f - accuracy;
            const volatile float width = inverseAccuracy * 0.9f;
            const volatile float baseWidth = width + 0.1f;
            horizontalScale = baseWidth * 15.0f;
            const volatile float height = (inverseAccuracy + 1.0f) * 0.5f;
            verticalScale = height * -44.0f;
        }
        const float horizontal = static_cast<float>(G_crandom() * horizontalScale);
        const float vertical = static_cast<float>(static_cast<double>(G_random()) * verticalScale + bias);
        for (int i = 0; i != 3; ++i)
        {
            const volatile float sideways = side[i] * horizontal;
            const volatile float offset = sideways + point[i];
            const volatile float upward = wp->up[i] * vertical;
            point[i] = upward + offset;
        }
    }
    for (int i = 0; i != 3; ++i)
        wp->forward[i] = point[i] - wp->muzzleTrace[i];
    Actor_NormalizeShot_SP(wp->forward, 0, 1, 2);
}

static void Actor_ShootHit_SP(actor_s *self, float accuracy, int perfectAim, weaponParms *wp)
{
    ++self->hitCount;
    self->missCount = 0;
    sentient_s *sentient = self->sentient;
    unsigned short tag = 0;
    if (sentient->scriptTargetEnt.number &&
        sentient->targetEnt.number == sentient->scriptTargetEnt.number)
        tag = sentient->scriptTargetTag;
    sentient_s *enemy = sentient->targetEnt.ent()->sentient;
    Actor_ShootHitTarget_SP(enemy, tag, accuracy, perfectAim, wp);
}

static void Actor_ShootMiss_SP(sentient_s *enemy, float accuracy, weaponParms *wp)
{
    static float missRadius = 6969.0f;
    if (missRadius == 6969.0f)
    {
        const volatile float square = 15.0f * 15.0f;
        const volatile float twoSquares = square * 2.0f;
        missRadius = sqrtf(twoSquares * 3.0f);
    }
    float point[3], direction[3], side[3];
    Sentient_GetEyePosition(enemy, point);
    for (int i = 0; i != 3; ++i)
        direction[i] = point[i] - wp->muzzleTrace[i];
    Actor_NormalizeShot_SP(direction, 1, 2, 0);
    Actor_ShotSide_SP(direction, wp->up, side, 2, 1, 0);
    const int sign = G_random() > 0.5f ? 1 : -1;
    float horizontal, inverseAccuracy = 0.0f;
    if (enemy->ent->client)
        horizontal = static_cast<float>((static_cast<double>(G_random()) + 1.0) * 8.0 * sign);
    else
    {
        inverseAccuracy = 1.0f - accuracy;
        horizontal = static_cast<float>((static_cast<double>(G_random()) * inverseAccuracy * 10.0 + missRadius) * sign);
    }
    for (int i = 0; i != 3; ++i)
    {
        const volatile float setback = direction[i] * -16.0f;
        const volatile float offset = setback + point[i];
        const volatile float sideways = side[i] * horizontal;
        point[i] = sideways + offset;
    }
    float vertical;
    if (enemy->ent->client)
    {
        const float bias = Actor_ShotTargetBias_SP(enemy);
        vertical = static_cast<float>(bias - static_cast<double>(G_random()) * 44.0);
    }
    else
        vertical = static_cast<float>(G_crandom() * inverseAccuracy * -22.0);
    for (int i = 0; i != 3; ++i)
    {
        const volatile float upward = wp->up[i] * vertical;
        const volatile float offset = upward + point[i];
        wp->forward[i] = offset - wp->muzzleTrace[i];
    }
    Actor_NormalizeShot_SP(wp->forward, 0, 1, 2);
}

static void Actor_ShootNonSentient_SP(const float *target, weaponParms *wp)
{
    float direction[3], side[3], point[3];
    for (int i = 0; i != 3; ++i)
        direction[i] = target[i] - wp->muzzleTrace[i];
    Actor_NormalizeShot_SP(direction, 2, 0, 1);
    Actor_ShotSide_SP(direction, wp->up, side, 1, 2, 0);
    const float horizontal = static_cast<float>(G_crandom() * 8.0);
    for (int i = 0; i != 3; ++i)
    {
        const volatile float sideways = side[i] * horizontal;
        point[i] = sideways + target[i];
    }
    const float vertical = static_cast<float>(G_crandom() * 8.0);
    for (int i = 0; i != 3; ++i)
    {
        const volatile float upward = wp->up[i] * vertical;
        const volatile float offset = upward + point[i];
        wp->forward[i] = offset - wp->muzzleTrace[i];
    }
    Actor_NormalizeShot_SP(wp->forward, 0, 1, 2);
}

static void Actor_RotateShot_SP(float *out, const float *axis, const float *point, float degrees)
{
    float squared[3], right[3], up[3];
    for (int i = 0; i != 3; ++i)
        squared[i] = axis[i] * axis[i];
    int least = squared[0] > squared[1] ? 1 : 0;
    if (squared[least] > squared[2])
        least = 2;
    for (int i = 0; i != 3; ++i)
        right[i] = axis[i] * -axis[least];
    right[least] = right[least] + 1.0f;
    Actor_NormalizeShot_SP(right, 2, 1, 0);
    const volatile float yz = right[1] * axis[2];
    const volatile float zy = right[2] * axis[1];
    const volatile float zx = right[2] * axis[0];
    const volatile float xz = right[0] * axis[2];
    const volatile float xy = right[0] * axis[1];
    const volatile float yx = right[1] * axis[0];
    up[0] = yz - zy;
    up[1] = zx - xz;
    up[2] = xy - yx;
    float m[3][3], im[3][3], zrot[3][3] = {}, temporary[3][3], rotation[3][3];
    for (int i = 0; i != 3; ++i)
    {
        m[i][0] = right[i];
        m[i][1] = up[i];
        m[i][2] = axis[i];
        im[0][i] = right[i];
        im[1][i] = up[i];
        im[2][i] = axis[i];
    }
    const float radians = degrees * 0.01745329238474369f;
    const float sine = static_cast<float>(sin(static_cast<double>(radians)));
    const float cosine = static_cast<float>(cos(static_cast<double>(radians)));
    zrot[0][0] = cosine;
    zrot[0][1] = sine;
    zrot[1][0] = -sine;
    zrot[1][1] = cosine;
    zrot[2][2] = 1.0f;
    for (int r = 0; r != 3; ++r)
        for (int c = 0; c != 3; ++c)
        {
            const volatile float y = m[r][1] * zrot[1][c];
            const volatile float z = m[r][2] * zrot[2][c];
            const volatile float x = m[r][0] * zrot[0][c];
            const volatile float yzProduct = y + z;
            temporary[r][c] = yzProduct + x;
        }
    for (int r = 0; r != 3; ++r)
        for (int c = 0; c != 3; ++c)
        {
            const volatile float y = temporary[r][1] * im[1][c];
            const volatile float x = temporary[r][0] * im[0][c];
            const volatile float z = temporary[r][2] * im[2][c];
            const volatile float yxProduct = y + x;
            rotation[r][c] = yxProduct + z;
        }
    for (int r = 0; r != 3; ++r)
    {
        const volatile float x = rotation[r][0] * point[0];
        const volatile float z = rotation[r][2] * point[2];
        const volatile float y = rotation[r][1] * point[1];
        const volatile float xzProduct = x + z;
        out[r] = xzProduct + y;
    }
}

static void Actor_ClampShotDirection_SP(weaponParms *wp)
{
    const volatile float y = wp->gunForward[1] * wp->forward[1];
    const volatile float z = wp->gunForward[2] * wp->forward[2];
    const volatile float x = wp->gunForward[0] * wp->forward[0];
    const volatile float yz = y + z;
    const float alignment = fabsf(yz + x);
    if (0.9659199714660645f > alignment)
    {
        float axis[3], result[3];
        Actor_ShotSide_SP(wp->gunForward, wp->forward, axis, 2, 0, 1);
        Actor_RotateShot_SP(result, axis, wp->gunForward, 15.0f);
        Vec3Copy(result, wp->forward);
    }
}

static void Actor_ClearPlayerMissRay_SP(const float *start, const float *origin,
    const float *mins, const float *maxs, float *forward)
{
    // The stock miss offset can still leave the final bullet ray inside the
    // player's collision bounds. Keep rays that already clear the player.
    float enterDistance = 0.0f, exitDistance = 1.0e30f;
    for (int i = 0; i < 3; ++i)
    {
        const float low = origin[i] + mins[i] - start[i];
        const float high = origin[i] + maxs[i] - start[i];
        if (fabsf(forward[i]) < 1.0e-6f)
        {
            if (low > 0.0f || high < 0.0f)
                return;
            continue;
        }
        const float a = low / forward[i], b = high / forward[i];
        enterDistance = fmaxf(enterDistance, fminf(a, b));
        exitDistance = fminf(exitDistance, fmaxf(a, b));
        if (enterDistance > exitDistance)
            return;
    }

    const float x = origin[0] + (mins[0] + maxs[0]) * 0.5f - start[0];
    const float y = origin[1] + (mins[1] + maxs[1]) * 0.5f - start[1];
    const float distance = sqrtf(x * x + y * y);
    if (distance <= 0.0f)
        return;
    const float dx = x / distance, dy = y / distance;
    const float halfX = (maxs[0] - mins[0]) * 0.5f;
    const float halfY = (maxs[1] - mins[1]) * 0.5f;
    const float closest = distance - fabsf(dx) * halfX - fabsf(dy) * halfY;
    if (closest <= 0.0f)
        return;

    // Use the same side of the player as the original miss. Clear the
    // projected bounds at their nearest depth so a diagonal ray also misses.
    const float side = copysignf((fabsf(dy) * halfX + fabsf(dx) * halfY + 1.0f) / closest,
        -dy * forward[0] + dx * forward[1]);
    const float horizontal = dx * forward[0] + dy * forward[1];
    if (horizontal <= 0.0f)
        return;
    const float elevation = forward[2] / horizontal;
    forward[0] = dx - dy * side;
    forward[1] = dy + dx * side;
    forward[2] = elevation;
    Actor_NormalizeShot_SP(forward, 0, 1, 2);
}

void __cdecl Actor_StopShoot_SP(actor_s *self)
{
    self->ent->s.lerp.eFlags &= ~0x40u;
}

void __cdecl Actor_Shoot_SP(actor_s *self, float accuracyMod, const float *target, int fireEvent)
{
    if (self->lastShotTime == level.time)
        return;
    self->lastShotTime = level.time;
    gentity_s *ent = self->ent;
    const unsigned int weapon = G_GetWeaponIndexForName(SL_ConvertToString(self->weaponName, SCRIPTINSTANCE_SERVER));
    weaponParms wp;
    Actor_FillWeaponParms(self, &wp);
    Weapon_SetWeaponParamsWeapon(&wp, weapon);
    gentity_s *enemyEnt = Actor_GetTargetEntity(self);
    bool missedCampaignPlayer = false;
    if (target)
    {
        for (int i = 0; i != 3; ++i)
            wp.forward[i] = target[i] - wp.muzzleTrace[i];
        Actor_NormalizeShot_SP(wp.forward, 0, 2, 1);
    }
    else if (!enemyEnt)
        Vec3Copy(wp.gunForward, wp.forward);
    else if (enemyEnt->sentient)
    {
        Actor_BroadcastTeamEvent(self->sentient, AI_EV_NEW_ENEMY);
        const float accuracy = Actor_ShotAccuracy_SP(self, accuracyMod, &wp);
        const float random = G_random();
        const bool aimToHit = accuracy > random || self->perfectAim;
        if (wp.weapDef->weapType == WEAPTYPE_BULLET && G_CubaDebugEnabled_SP())
        {
            Com_Printf(15, "CUBA_SHOT time=%i attacker=%i target=%i weapon=%s chance=%.4f roll=%.4f "
                "perfect=%i aim_hit=%i graph=%.4f sight=%.4f attacker_team=%i target_team=%i\n", level.time, ent->s.number, enemyEnt->s.number,
                BG_WeaponName(weapon), accuracy, random, self->perfectAim, aimToHit,
                self->debugWeaponAccuracy, self->playerSightAccuracy,
                static_cast<int>(self->sentient->eTeam), static_cast<int>(enemyEnt->sentient->eTeam));
        }
        if (aimToHit)
            Actor_ShootHit_SP(self, accuracy, self->perfectAim, &wp);
        else
        {
            ++self->missCount;
            Actor_ShootMiss_SP(self->sentient->targetEnt.ent()->sentient, accuracy, &wp);
            missedCampaignPlayer = enemyEnt->client && !zombiemode->current.enabled
                && wp.weapDef->weapType == WEAPTYPE_BULLET;
        }
    }
    else
    {
        float centroid[3];
        G_EntityCentroid(enemyEnt, centroid);
        Actor_ShootNonSentient_SP(centroid, &wp);
    }
    Actor_ClampShotDirection_SP(&wp);
    if (missedCampaignPlayer)
    {
        const auto &bounds = enemyEnt->r;
        Actor_ClearPlayerMissRay_SP(wp.muzzleTrace, bounds.currentOrigin,
            bounds.mins, bounds.maxs, wp.forward);
    }
    if (!target && enemyEnt && enemyEnt->client && wp.weapDef->weapType == WEAPTYPE_BULLET && G_CubaDebugEnabled_SP())
    {
        const auto &bounds = enemyEnt->r;
        Com_Printf(15, "CUBA_SHOT_RAY time=%i attacker=%i target=%i start=(%.3f %.3f %.3f) dir=(%.6f %.6f %.6f) "
            "origin=(%.3f %.3f %.3f) mins=(%.1f %.1f %.1f) maxs=(%.1f %.1f %.1f)\n",
            level.time, ent->s.number, enemyEnt->s.number, wp.muzzleTrace[0], wp.muzzleTrace[1], wp.muzzleTrace[2],
            wp.forward[0], wp.forward[1], wp.forward[2], bounds.currentOrigin[0], bounds.currentOrigin[1],
            bounds.currentOrigin[2], bounds.mins[0], bounds.mins[1], bounds.mins[2],
            bounds.maxs[0], bounds.maxs[1], bounds.maxs[2]);
    }
    ent->s.weapon = static_cast<unsigned char>(weapon);
    if (wp.weapDef->weapType == WEAPTYPE_BULLET)
    {
        if (self->sentient->eTeam == TEAM_ALLIES && G_CubaDebugEnabled_SP())
        {
            Com_Printf(15, "CUBA_ALLY_SHOT time=%i attacker=%i target=%i weapon=%s explicit=%i\n",
                level.time, ent->s.number, enemyEnt ? enemyEnt->s.number : ENTITYNUM_NONE,
                BG_WeaponName(weapon), target != nullptr);
        }
        Bullet_Fire(ent, 0.0f, &wp, ent, level.time);
        G_AddEvent(ent, fireEvent ? EV_FIRE_WEAPON : EV_FIRE_WEAPON_LASTSHOT, 0);
    }
    else if (wp.weapDef->weapType == WEAPTYPE_PROJECTILE)
    {
        if (wp.weapDef->weapClass == WEAPCLASS_GRENADE)
            Weapon_GrenadeLauncher_Fire(ent, weapon, ent->s.weaponModel, &wp);
        else
            Weapon_RocketLauncher_Fire(ent, weapon, 0.0f, &wp, vec3_origin, enemyEnt, nullptr);
        G_AddEvent(ent, EV_FIRE_WEAPON, 0);
    }
    else if (wp.weapDef->weapType == WEAPTYPE_GAS)
    {
        Weapon_Flamethrower_Fire(ent, &wp);
        G_AddEvent(ent, EV_FIRE_WEAPON, 0);
    }
}
#endif
void __cdecl Actor_InitWeaponParms(actor_s *self, unsigned int weapon, weaponParms *wp)
{
    Actor_FillWeaponParms(self, wp);
    Weapon_SetWeaponParamsWeapon(wp, weapon);
}

gentity_s *__cdecl Actor_Melee(actor_s *self, const float *direction)
{
    char *v2; // eax
    float v4; // [esp+10h] [ebp-8Ch]
    float v5; // [esp+2Ch] [ebp-70h]
    float enemyEye[3]; // [esp+38h] [ebp-64h] BYREF
    int weapon; // [esp+44h] [ebp-58h]
    sentient_s *enemy; // [esp+48h] [ebp-54h]
    weaponParms wp; // [esp+4Ch] [ebp-50h] BYREF
    gentity_s *ent; // [esp+98h] [ebp-4h]

    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_aim.cpp", 955, 0, "%s", "self") )
        __debugbreak();
    if ( !self && !Assert_MyHandler("C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_aim.cpp", 961, 0, "%s", "self") )
        __debugbreak();
    ent = self->ent;
    v2 = SL_ConvertToString(self->weaponName, SCRIPTINSTANCE_SERVER);
    weapon = G_GetWeaponIndexForName(v2);
    if ( self->lastShotTime == level.time )
    {
#ifndef KISAK_SP
        Com_PrintError(
            18,
            "ERROR: Attempt for same actor (entnum %d) to shoot/melee more than once in a frame.\n",
            ent->s.number);
#endif
        return 0;
    }
    else
    {
        self->lastShotTime = level.time;
        enemy = Actor_GetTargetSentient(self);
        Actor_InitWeaponParms(self, weapon, &wp);
        Actor_GetEyePosition(self, wp.muzzleTrace);
        if ( enemy )
        {
#ifdef KISAK_SP
            Sentient_GetEyePosition(enemy, enemyEye);
#else
            Sentient_GetThirdPersonEyePosition(enemy, enemyEye);
#endif
            wp.forward[0] = enemyEye[0] - wp.muzzleTrace[0];
            wp.forward[1] = enemyEye[1] - wp.muzzleTrace[1];
            wp.forward[2] = enemyEye[2] - wp.muzzleTrace[2];
            if ( direction )
            {
                v4 = Vec2Length(direction);
                v5 = Vec2Length(wp.forward) / v4;
                wp.forward[0] = v5 * *direction;
                wp.forward[1] = v5 * direction[1];
            }
            Vec3Normalize(wp.forward);
        }
        else if ( direction )
        {
            wp.forward[0] = *direction;
            wp.forward[1] = direction[1];
            wp.forward[2] = direction[2];
            Vec3Normalize(wp.forward);
        }
        ent->s.weapon = weapon;
        if ( ent->s.weapon != weapon
            && !Assert_MyHandler(
                        "C:\\projects_pc\\cod\\codsrc\\src\\game\\actor_aim.cpp",
                        1014,
                        0,
                        "%s",
                        "ent->s.weapon == weapon") )
        {
            __debugbreak();
        }
#ifdef KISAK_SP
        if ( self->species != AI_SPECIES_DOG && self->species != AI_SPECIES_ZOMBIE_DOG )
            return Weapon_Melee(self->ent, &wp, 64.0f, 0.0f, 0.0f, level.time);
#endif
        return Weapon_Melee(
                         self->ent,
                         &wp,
                         ai_meleeRange->current.value,
                         ai_meleeWidth->current.value,
                         ai_meleeHeight->current.value,
                         level.time);
    }
}

