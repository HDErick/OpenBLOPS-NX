"""Check client-only prop and bolt parent initialization against real engine code."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
parser.add_argument('--baseline', metavar='REF', help='Verify a Git revision reproduces the SP failure')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/script-mover-parent-tests'
output.mkdir(parents=True, exist_ok=True)

def read(name):
    if args.baseline:
        return subprocess.check_output(['git', 'show', args.baseline + ':' + name], cwd=root, text=True)
    return (root / name).read_text()

def function(code, signature):
    start = code.index(signature)
    end = code.index('{', start) + 1
    depth = 1
    while depth:
        depth += (code[end] == '{') - (code[end] == '}')
        end += 1
    return code[start:end]

header = read('src/qcommon/ent.h')
start = header.index('#ifdef KISAK_SP\nstruct LerpEntityStateScriptMover')
mover = header[start:header.index('#endif', start) + len('#endif')]
start = header.index('struct __declspec(align(4)) LerpEntityStatePlayer')
player = header[start:header.index('\n};', start) + 3]
destructible = function(read('src/physics/destructible.cpp'), 'void __cdecl CG_DestructibleClientOnlyCreate(')
bolt_code = read('src/cgame/cg_bolt.cpp')
init_bolt = function(bolt_code, 'void __cdecl CG_InitBolt(')
update_bolt = function(bolt_code, 'void __cdecl CG_UpdateBolt(')  # SP body, also valid with the MP layout.
get_parent = function(read('src/cgame_mp/cg_animscripted_mp.cpp'), 'int CG_GetEntityLinkParent_SP(')
source = output / 'parents.cpp'
source.write_text(r'''
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __declspec
#define __declspec(x)
#endif
#define __int8 char
#define __int16 short
#define _QWORD uint64_t
#define __debugbreak() assert(false)
constexpr int ENTITYNUM_NONE = 1023, ET_SCRIPTMOVER = 6, ET_ACTOR = 17, ET_PLAYER = 1;
''' + player + '\n' + mover + r'''
union LerpUnion {
    LerpEntityStateScriptMover scriptMover;
    LerpEntityStatePlayer player;
    struct { float gunAngles[2]; } turret;
    uint64_t createDynEnt[4];
};
struct Trajectory { float trBase[3]; };
struct DestructibleDef { void *model; };
struct Destructible { DestructibleDef *ddef; int entNum; } destructible;
struct Link { int linkEnt; };
struct centity_s {
    struct { int localClientNum; float origin[3], angles[3]; } pose;
    struct { int number, eType, animTreeIndex; struct {
        Trajectory pos, apos; unsigned eFlags; LerpUnion u;
    } lerp; } nextState;
    struct { Trajectory pos, apos; unsigned eFlags; } currentState;
    Destructible *destructible;
    Link *linkInfo;
    unsigned clientFlags;
};
static centity_s pool[10], parents[2];
static int allocated, processed;
centity_s *g_BoltEnt[1][10];
unsigned g_BoltIndex[1];
centity_s *CG_Spawn(int) { return &pool[allocated++]; }
centity_s *CG_GetEntity(int, int number) { assert(number < 2); return &parents[number]; }
Destructible *CG_GetFreeDestructible(int client, int entity) {
    assert(client == 0 && entity == 1189); return &destructible;
}
template<class... T> bool Assert_MyHandler(T...) { assert(false); return false; }
template<class... T> void CG_PreProcess_GetDObj(T...) { ++processed; }
struct actorInfo_t { int attachedEntTime, attachedEntNum, nextAttachedEntNum; } actor;
struct clientInfo_t { int attachedEntNum; } playerInfo;
struct cg_s { int time; } cg;
actorInfo_t *CG_GetActorLinkInfo_SP(int, int) { return &actor; }
clientInfo_t *CG_GetClientInfoForPlayerEnt_SP(int, int) { return &playerInfo; }
cg_s *CG_GetLocalClientGlobals(int) { return &cg; }
''' + destructible + '\n' + init_bolt + '\n' + update_bolt + '\n' + get_parent + r'''
static void test_prop(unsigned char fill) {
    centity_s ent{};
    ent.nextState.number = 1189;
    ent.pose.origin[0] = -829.2f; ent.pose.origin[1] = 1317.3f; ent.pose.origin[2] = -88;
    ent.pose.angles[1] = 142.6f;
    ent.nextState.lerp.eFlags = 0x40;
    std::memset(&ent.nextState.lerp.u, fill, sizeof(ent.nextState.lerp.u));
    DestructibleDef def{};
    CG_DestructibleClientOnlyCreate(&ent, &def);
    // The consumer must see NONE, never player 0 or a stale slot from reuse.
    assert(CG_GetEntityLinkParent_SP(0, &ent) == ENTITYNUM_NONE);
    assert(ent.nextState.eType == ET_SCRIPTMOVER);
    assert(ent.destructible->ddef == &def && ent.destructible->entNum == 1189);
    assert(ent.nextState.lerp.eFlags == (0x20000u | 0x40u));
    assert(std::memcmp(ent.nextState.lerp.pos.trBase, ent.pose.origin, sizeof(ent.pose.origin)) == 0);
    assert(std::memcmp(ent.currentState.pos.trBase, ent.pose.origin, sizeof(ent.pose.origin)) == 0);
    assert(std::memcmp(ent.nextState.lerp.apos.trBase, ent.pose.angles, sizeof(ent.pose.angles)) == 0);
    assert(std::memcmp(ent.currentState.apos.trBase, ent.pose.angles, sizeof(ent.pose.angles)) == 0);
    LerpEntityStateScriptMover expected{}; expected.attachedEntNum = ENTITYNUM_NONE;
    assert(std::memcmp(&expected, &ent.nextState.lerp.u.scriptMover, sizeof(expected)) == 0);
    auto *bytes = reinterpret_cast<unsigned char *>(&ent.nextState.lerp.u);
    for (size_t i = sizeof(expected); i < sizeof(LerpUnion); ++i) assert(bytes[i] == fill);
    // A real link to player 0 remains valid: the fix is initialization, not a ban on parent 0.
    ent.nextState.lerp.u.scriptMover.attachedEntNum = 0;
    assert(CG_GetEntityLinkParent_SP(0, &ent) == 0);
}
static void test_bolts() {
    allocated = 0;
    for (auto &ent : pool) { ent = {}; ent.nextState.lerp.u.player.meleeWeapon = 77; }
    g_BoltIndex[0] = 12;
    CG_InitBolt(0);
    assert(allocated == 10 && g_BoltIndex[0] == 0);
    for (auto *ent : g_BoltEnt[0]) {
        assert(ent->nextState.lerp.u.scriptMover.attachedEntNum == ENTITYNUM_NONE);
        assert(ent->nextState.eType == ET_SCRIPTMOVER);
        assert(ent->nextState.lerp.eFlags & 0x20u);
#ifdef KISAK_SP
        assert(ent->nextState.lerp.u.player.meleeWeapon == 77);
#endif
    }
    Link link{0}; auto *bolt = g_BoltEnt[0][0]; bolt->linkInfo = &link;
    bolt->nextState.lerp.u.scriptMover.attachedEntNum = 0;
    bolt->nextState.lerp.eFlags = 0;
    parents[0].clientFlags = 2;
    CG_UpdateBolt(0);
    assert(bolt->nextState.lerp.u.scriptMover.attachedEntNum == 0 && !bolt->nextState.lerp.eFlags);
    parents[0].clientFlags = 0;
    CG_UpdateBolt(0);
    assert(bolt->nextState.lerp.u.scriptMover.attachedEntNum == ENTITYNUM_NONE);
    assert(bolt->nextState.lerp.eFlags & 0x20u);
    bolt->nextState.lerp.u.scriptMover.attachedEntNum = 1022;
    bolt->nextState.lerp.eFlags = 0;
    CG_UpdateBolt(0);
    assert(bolt->nextState.lerp.u.scriptMover.attachedEntNum == 1022 && !bolt->nextState.lerp.eFlags);
}
int main() {
#ifdef KISAK_SP
    static_assert(offsetof(LerpEntityStateScriptMover, attachedEntNum) == 22);
#else
    static_assert(offsetof(LerpEntityStateScriptMover, attachedEntNum) == 18);
#endif
    static_assert(offsetof(LerpEntityStatePlayer, meleeWeapon) == 18);
    test_prop(0); test_prop(0xa5); assert(processed == 2);
    test_bolts();
    puts("prop parent initialization and bolt lifecycle passed");
}
''')
for name, defines in [('pc-sp', ['-DKISAK_SP']), ('nx-sp', ['-DKISAK_SP', '-DKISAK_NX']), ('mp', [])]:
    exe = output / (name + '.exe')
    subprocess.run([args.compiler, '-std=c++20', *defines, str(source), '-o', str(exe)], check=True)
    result = subprocess.run([str(exe)])
    if args.baseline:
        if name.endswith('-sp') and result.returncode == 0:
            raise SystemExit('Baseline unexpectedly passed the SP regression')
        if name == 'mp' and result.returncode != 0:
            raise SystemExit('Baseline MP compatibility check failed')
        print(name + ': expected baseline ' + ('failure' if result.returncode else 'pass'), flush=True)
    else:
        result.check_returncode()
        print(name + ': passed', flush=True)