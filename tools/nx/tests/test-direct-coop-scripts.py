"""Check experimental co-op script adjustments without starting the game."""
from pathlib import Path
import argparse
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
parser.add_argument('--fixtures', type=Path, help='Directory with extracted kisak-coop-*.gsc test inputs')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = root / 'build-nx/direct-coop-tests'
out.mkdir(parents=True, exist_ok=True)
cpp = out / 'patch.cpp'
cpp.write_text(r'''
#include "clientscript/cscr_direct_coop.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc, char **argv) {
    assert(argc == 4);
    std::ifstream in(argv[2], std::ios::binary);
    assert(in);
    std::string original((std::istreambuf_iterator<char>(in)), {});
    std::string source = original;
    assert(!Scr_PatchDirectCoopSource("maps/other.gsc", source));
    assert(source == original);
    assert(Scr_PatchDirectCoopSource(argv[1], source));
    assert(source != original);
    assert(source.find("kisak_direct_late_join") != std::string::npos);
    std::string twice = source;
    assert(!Scr_PatchDirectCoopSource(argv[1], twice));
    assert(twice == source);
    std::string damaged = original;
    const std::string anchor = std::string(argv[1]) == "maps/_callbackglobal.gsc"
        ? "if( IsDefined( level.custom_spawnPlayer ) )"
        : "if( players[i].sessionstate == \"spectator\" )";
    damaged.erase(damaged.find(anchor), anchor.size());
    std::string before = damaged;
    assert(!Scr_PatchDirectCoopSource(argv[1], damaged));
    assert(damaged == before);
    std::string duplicate = original + anchor;
    before = duplicate;
    assert(!Scr_PatchDirectCoopSource(argv[1], duplicate));
    assert(duplicate == before);
    std::ofstream output(argv[3], std::ios::binary);
    output.write(source.data(), source.size());
    std::cout << "PASS: " << argv[2] << '\n';
}
''', encoding='utf-8')
exe = out / 'patch.exe'
subprocess.run([args.compiler, '-std=c++17', '-O2', '-I'+str(root/'src'), str(cpp), '-o', str(exe)], check=True)
# Minimal inputs exercise the failure guards even without local game data.
fixtures = []
for name, text in {
    '_callbackglobal.gsc': 'self thread[[level.spawnClient]]();\nif( IsDefined( level.custom_spawnPlayer ) ) {}\n',
    '_zombiemode.gsc': 'flag_wait( "all_players_connected" );\nprintln( "sessions: mapname=", level.script, " gametype zom isserver 1 player_count=", get_players().size );\nself freezecontrols( true ); // first spawn only, intro_black_screen will pull them out of it\nif( players[i].sessionstate == "spectator" ) {}\nself waittill( "spawned_player" );\nself maps\\_zombiemode_score::player_reduce_points( "died" );\nif (players[i].score < 1500) players[i].score = 1500;\0',
}.items():
    file = out / name
    file.write_text(text, encoding='utf-8')
    fixtures.append(file)
if args.fixtures:
    fixtures += sorted(args.fixtures.glob('kisak-coop-*.gsc'))
count = 0
for fixture in fixtures:
    name = next((n for n in ('_callbackglobal.gsc', '_zombiemode.gsc') if fixture.name.endswith(n)), None)
    if not name:
        continue
    patched = out / ('patched-'+fixture.name)
    subprocess.run([str(exe), 'maps/'+name, str(fixture), str(patched)], check=True)
    text = patched.read_text(encoding='utf-8').rstrip('\0')
    # Strip comments/strings before checking delimiter balance. These are not
    # a substitute for compiling the GSC in a real map-loading test.
    stripped = re.sub(r'//[^\n]*|/\*.*?\*/|/#.*?#/|"(?:\\.|[^"\\])*"', '', text, flags=re.S)
    stack = []
    for c in stripped:
        if c in '({[':
            stack.append(c)
        elif c in ')}]':
            assert stack and stack.pop() == {')':'(', '}':'{', ']':'['}[c], (fixture, c)
    assert not stack, fixture
    if name == '_zombiemode.gsc':
        assert text.index('level waittill( "end_of_round" );', text.index('kisak_direct_wait_for_round()')) < text.index('self.kisak_direct_waiting_round = false;')
        assert text.index('flag_wait( "all_players_connected" );') < text.index('direct_players[direct_i].kisak_direct_initial_player = true;')
        assert text.index('direct_players[direct_i].kisak_direct_initial_player = true;') < text.index('level.kisak_direct_roster_ready = true;')
        assert 'points[index % points.size]' in text
        assert 'self.is_zombie = false;' in text
        assert text.count('self kisak_direct_set_spawn_points();') == 2
        assert 'self.score = 500;' in text
        assert 'self.score_total = self.score;' in text
        assert 'players[i].score < 1500' not in text and 'players[i].score = 1500;' not in text
        assert 'player_reduce_points( "died" )' not in text
        # The hook sits on every spawned_player notification; it is not limited
        # to first-spawn initialization or to the late-join round-wait thread.
        assert 'self waittill( "spawned_player" );\n        self kisak_direct_set_spawn_points();' in text
        # spawnSpectator ends its own thread on spawned_spectator. Calling it
        # synchronously would kill the join handler before its round wait.
        assert 'self thread [[level.spawnSpectator]]();' in text
        assert 'self waittill( "spawned_spectator" );' in text
        assert text.index('self.sessionstate = "playing";', text.index('kisak_direct_wait_for_round()')) < text.index('self.kisak_direct_waiting_round = false;')
    else:
        classification = 'IsDefined( level.kisak_direct_roster_ready ) &&\n        !IsDefined( self.kisak_direct_initial_player )'
        assert classification in text
        assert text.index(classification) < text.index('self thread maps\\_zombiemode::kisak_direct_wait_for_round();')
    count += 1
loader = (root/'src/clientscript/cscr_parser.cpp').read_text()
assert loader.count('sourceBuf = Scr_AdjustDirectCoop(inst, extFilename, sourceBuf, &len);') == 2
assert 'inst != SCRIPTINSTANCE_SERVER || !zombiemode || !zombiemode->current.enabled' in loader
print(f'{count} script inputs passed; no game launched.')
