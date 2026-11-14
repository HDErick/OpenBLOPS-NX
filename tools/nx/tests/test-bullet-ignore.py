"""Check which bullet hits Bullet_IgnoreHitEntity drops: SP follows retail (allies' bullets pass through
their own side and the player, hits on a player count near the eye only); MP only drops self-hits."""
from pathlib import Path
import argparse, subprocess
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
out = root / 'build-nx/bullet-ignore-tests'
out.mkdir(parents=True, exist_ok=True)

source = (root / 'src/game/bullet.cpp').read_text()
start = source.index('bool __cdecl Bullet_IgnoreHitEntity(')
end = source.index('{', start) + 1
depth = 1
while depth:
    depth += (source[end] == '{') - (source[end] == '}')
    end += 1

code = r'''
#include <cassert>
#include <cmath>
#include <cstdio>
#ifndef __cdecl
#define __cdecl
#endif
#define __debugbreak() assert(false)
template<class... T> bool Assert_MyHandler(T...) { return false; }
enum team_t { TEAM_FREE, TEAM_AXIS, TEAM_ALLIES };
struct gentity_s;
struct sentient_s { gentity_s *ent = nullptr; team_t eTeam = TEAM_FREE; };
struct tagInfo_s { gentity_s *parent = nullptr; };
struct playerState_s { float origin[3]{}; float viewHeight = 60; };
struct gclient_s { playerState_s ps; };
struct actor_s {};
struct gentity_s { gclient_s *client = nullptr; actor_s *actor = nullptr; sentient_s *sentient = nullptr; tagInfo_s *tagInfo = nullptr; };
struct BulletFireParams { float dir[3]{1, 0, 0}; };
struct BulletTraceResults { gentity_s *hitEnt = nullptr; float hitPos[3]{}; };
void G_GetPlayerViewOrigin(const playerState_s *ps, float *origin)
{
    origin[0] = ps->origin[0]; origin[1] = ps->origin[1]; origin[2] = ps->origin[2] + ps->viewHeight;
}
''' + source[start:end] + r'''
struct Sentient { gentity_s ent; sentient_s sentient; actor_s actor; };
static void make(Sentient &s, team_t team, bool ai) { s.sentient.eTeam = team; s.ent.sentient = &s.sentient; if (ai) s.ent.actor = &s.actor; }
static bool hit(gentity_s *attacker, gentity_s *target, float sideOffset = 0, float dirX = 1, float dirY = 0)
{
    BulletFireParams bp; bp.dir[0] = dirX; bp.dir[1] = dirY; bp.dir[2] = 0;
    BulletTraceResults br; br.hitEnt = target;
    // The bullet travels along dir and hits the target sideOffset units to its left of the eye line.
    const float eyeX = target->client ? target->client->ps.origin[0] : 0, eyeY = target->client ? target->client->ps.origin[1] : 0;
    br.hitPos[0] = eyeX - dirY * sideOffset; br.hitPos[1] = eyeY + dirX * sideOffset;
    return !Bullet_IgnoreHitEntity(&bp, &br, attacker);
}
int main()
{
    Sentient player, ally, enemy, otherAlly;
    make(player, TEAM_ALLIES, false); make(ally, TEAM_ALLIES, true); make(enemy, TEAM_AXIS, true); make(otherAlly, TEAM_ALLIES, true);
    gclient_s client; client.ps.origin[0] = 100; client.ps.origin[1] = 50; player.ent.client = &client;
    gentity_s vehicle, otherVehicle; tagInfo_s riding; riding.parent = &vehicle;

    assert(!hit(&enemy.ent, &enemy.ent));                         // a shooter never hits itself
    assert(hit(&enemy.ent, &player.ent));                          // an enemy's bullet through the eye line hurts
#ifdef KISAK_SP
    assert(!hit(&ally.ent, &player.ent));                          // an ally's bullet passes through the player
    assert(!hit(&ally.ent, &otherAlly.ent));                       // and through other allies
    assert(hit(&ally.ent, &enemy.ent) && hit(&enemy.ent, &ally.ent));
    assert(hit(&enemy.ent, &player.ent, 7.9f) && !hit(&enemy.ent, &player.ent, 8.1f));     // within 8 units of the eye
    assert(hit(&enemy.ent, &player.ent, -7.9f) && !hit(&enemy.ent, &player.ent, -8.1f));
    assert(hit(&enemy.ent, &player.ent, 7.0f, 0.6f, 0.8f) && !hit(&enemy.ent, &player.ent, 9.0f, 0.6f, 0.8f));
    assert(hit(&enemy.ent, &player.ent, 0, 0, 0));                 // a vertical shot: no horizontal direction to measure
    enemy.ent.tagInfo = &riding;                                   // an AI riding a vehicle does not hit it
    assert(!hit(&enemy.ent, &vehicle) && hit(&enemy.ent, &otherVehicle));
    enemy.ent.tagInfo = nullptr;
    assert(hit(&player.ent, &ally.ent));                           // the player can still hit allies (scripts handle that)
    assert(hit(&vehicle, &player.ent) && !hit(&vehicle, &player.ent, 9.0f));   // non-AI attackers get the eye test only
    puts("PASS: SP retail bullet filter: own side and the player skipped for AI shooters, linked parent skipped, 8-unit eye test on players");
#else
    assert(hit(&ally.ent, &player.ent) && hit(&ally.ent, &otherAlly.ent) && hit(&enemy.ent, &player.ent, 20.0f));
    puts("PASS: MP bullet filter unchanged: only self-hits are skipped");
#endif
}
'''
for mode, define in (('sp', ['-DKISAK_SP']), ('mp', [])):
    cpp = out / f'bullet-ignore-{mode}.cpp'
    exe = cpp.with_suffix('.exe')
    cpp.write_text(code)
    subprocess.run([a.compiler, '-std=c++17', '-Wall', '-Wno-unused-variable', *define, str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
