"""Exercise the real snapshot handoff with dense SP maps and sparse entity numbers."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/snapshot-capacity-tests'
output.mkdir(parents=True, exist_ok=True)
header = (root / 'src/cgame_mp/cg_local_mp.h').read_text()
start = header.index('struct snapshot_s')
snapshot = header[start:header.index('\n};', start) + 3]
code = (root / 'src/client_mp/cl_cgame_mp.cpp').read_text()
start = code.index('int __cdecl CL_GetSnapshot(')
end = code.index('{', start) + 1
depth = 1
while depth:
    depth += (code[end] == '{') - (code[end] == '}')
    end += 1
get_snapshot = code[start:end]
source = output / 'snapshot.cpp'
source.write_text(r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <string>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#define __int16 short
#define _WORD unsigned short
#define __debugbreak() assert(false)
constexpr int MAX_GENTITIES = 1024, MAX_ACTORS = 32, ERR_DROP = 1;
struct playerState_s { int marker; };
struct entityState_s { int number, marker; };
struct clientState_s { int marker; };
struct MatchState { int marker; };
struct AnimCommand_SP { int marker; };
struct actorState_s { int marker; };
''' + snapshot + r'''
struct clSnapshot_t {
    int messageNum = 40;
    bool valid = true;
    int parseEntitiesNum, parseClientsNum, snapFlags, serverCommandNum, ping;
    int serverTime, physicsTime, parseMatchStateNum, numEntities, numClients;
    playerState_s ps;
    int numAnimCommands; AnimCommand_SP animCommands[1024];
    int numActors; actorState_s actors[MAX_ACTORS];
};
struct clientActive_t {
    clSnapshot_t snap, snapshots[32];
    int parseEntitiesNum, parseClientsNum;
    MatchState parseMatchStates[32];
    entityState_s parseEntities[2048];
    clientState_s parseClients[2048];
} client;
clientActive_t *CL_GetLocalClientGlobals(int) { return &client; }
struct dvar_s { struct { bool enabled; } current; } statmon;
const dvar_s *com_statmon = &statmon;
static int warnings;
void StatMon_Warning(int, int, char *) { ++warnings; }
template<class... T> void Com_DPrintf(T...) { ++warnings; }
template<class... T> void Com_Printf(T...) {}
template<class... T> void Com_Error(T...) { assert(false); }
template<class... T> bool Assert_MyHandler(T...) { assert(false); return false; }
template<class... T> const char *va(T...) { return "assert detail"; }
static bool demo;
bool Demo_IsPlaying() { return demo; }
bool Demo_ShouldEntityBeVisibleToClient(int number) { return number != 650; }
''' + get_snapshot + r'''
static snapshot_s destination;
static void setup(const std::vector<int> &numbers, int first = 0) {
    client = {};
    warnings = 0;
    demo = false;
    client.snap.messageNum = 40;
    auto &frame = client.snapshots[40 & 31];
    frame.valid = true;
    frame.parseEntitiesNum = first;
    frame.numEntities = static_cast<int>(numbers.size());
    frame.serverTime = 12345;
    frame.physicsTime = 12340;
    frame.ps.marker = 42;
    frame.numClients = 1;
    frame.numAnimCommands = 1;
    frame.animCommands[0].marker = 17;
    frame.numActors = 1;
    frame.actors[0].marker = 19;
    client.parseEntitiesNum = first + frame.numEntities;
    for (int i = 0; i < frame.numEntities; ++i)
        client.parseEntities[(first + i) & 2047] = {numbers[i], i + 100};
    client.parseClients[0].marker = 55;
    client.parseMatchStates[0].marker = 66;
    std::memset(&destination, 0xcc, sizeof(destination));
}
int main() {
#ifdef KISAK_SP
    static_assert(std::size(destination.entities) == 1024, "SP must hold all server entities");
#else
    static_assert(std::size(destination.entities) == 512, "MP capacity must stay unchanged");
#endif
    const int capacity = static_cast<int>(std::size(destination.entities));
    for (int count : {0, 511, 512, 513, 640, 1023, 1024}) {
        std::vector<int> numbers;
        for (int i = 0; i < count; ++i) numbers.push_back(i);
        setup(numbers, 2040); // Copy across the parse-entity ring boundary too.
        assert(CL_GetSnapshot(0, 40, &destination));
        const int expected = count < capacity ? count : capacity;
        assert(destination.numEntities == expected);
        assert((warnings != 0) == (count > capacity));
        for (int i = 0; i < expected; ++i) {
            assert(destination.entities[i].number == i);
            assert(destination.entities[i].marker == i + 100);
        }
        assert(destination.numClients == 1 && destination.clients[0].marker == 55);
        assert(destination.matchState.marker == 66 && destination.ps.marker == 42);
#ifdef KISAK_SP
        assert(destination.numAnimCommands == 1 && destination.animCommands[0].marker == 17);
        assert(destination.animCommandPointersInitialized == 0);
        assert(destination.numActors == 1 && destination.actors[0].marker == 19);
#endif
    }
    std::vector<int> dense;
    for (int i = 100; i < 700; ++i) dense.push_back(i);
    dense.insert(dense.end(), {825, 836, 837}); // Zombie, grenade and explosion event.
    setup(dense);
    assert(CL_GetSnapshot(0, 40, &destination));
#ifdef KISAK_SP
    assert(destination.numEntities == 603 && !warnings);
    assert(destination.entities[600].number == 825);
    assert(destination.entities[601].number == 836);
    assert(destination.entities[602].number == 837);
    demo = true;
    assert(CL_GetSnapshot(0, 40, &destination));
    assert(destination.numEntities == 602);
    assert(destination.entities[601].number == 837);
    for (int i = 0; i < destination.numEntities; ++i)
        assert(destination.entities[i].number != 650);
#endif
    client.snapshots[40 & 31].valid = false;
    assert(!CL_GetSnapshot(0, 40, &destination));
    puts("PASS: dense snapshots, sparse high entity IDs, ring wrap, capacity limits and metadata integrity");
}
''')
for platform, defines in [('pc-sp', ['-DKISAK_SP']), ('nx-sp', ['-DKISAK_SP', '-DKISAK_NX']), ('mp', ['-DKISAK_MP'])]:
    exe = output / (platform + '.exe')
    subprocess.run([args.compiler, '-std=c++17', '-O2', *defines, str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
