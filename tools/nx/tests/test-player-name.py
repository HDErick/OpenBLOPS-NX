"""Exercise the shared playername.txt parser and startup-command override order."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
output = root / 'build-nx/player-name-tests'
output.mkdir(parents=True, exist_ok=True)
source = output / 'player-name.cpp'
source.write_text(r'''
#include <universal/local_player_name.h>
#include <cassert>
#include <string>
static void write(const char *name) {
    FILE *file = fopen("playername.txt", "wb"); assert(file);
    fputs(name, file); fclose(file);
}
int main() {
    remove("playername.txt");
    char name[32];
    assert(LocalPlayerName::Read("playername.txt", name, 5523));
    assert(!strcmp(name, "player5523"));
    assert(!LocalPlayerName::Read("playername.txt", name, 1));
    assert(!strcmp(name, "player5523"));
    write("  Test Player  \r\nignored");
    assert(!LocalPlayerName::Read("playername.txt", name, 1));
    assert(!strcmp(name, "Test Player"));
    write("Bad+\"Name\\;\t\x7f\n");
    assert(!LocalPlayerName::Read("playername.txt", name, 1));
    assert(!strcmp(name, "BadName"));
    write("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    assert(!LocalPlayerName::Read("playername.txt", name, 1));
    assert(strlen(name) == 31);
    for (const char *invalid : {"", "  \r\n", "abc", "Unknown Soldier", "xxUNKNOWN SOLDIERxx"}) {
        write(invalid);
        assert(LocalPlayerName::Read("playername.txt", name, 10001));
        assert(!strcmp(name, "player0001"));
    }
    // A read-only/unwritable path must still supply a valid fallback.
    assert(LocalPlayerName::Read("absent-directory/playername.txt", name, 42));
    assert(!strcmp(name, "player0042"));
    char cmdline[256] = "allowdupe nodump +set name \"User Override\" +devmap zombie_theater";
    assert(LocalPlayerName::ApplyDefault(cmdline, sizeof(cmdline), "File Player"));
    assert(std::string(cmdline) == "allowdupe nodump  +set name \"File Player\" +set name \"User Override\" +devmap zombie_theater");
    char empty[64] = "";
    assert(LocalPlayerName::ApplyDefault(empty, sizeof(empty), "File Player"));
    assert(!strcmp(empty, " +set name \"File Player\" "));
    char full[16] = "+devmap test";
    assert(!LocalPlayerName::ApplyDefault(full, sizeof(full), "File Player"));
    assert(!strcmp(full, "+devmap test"));
    puts("PASS: name persistence, validation, command-safe characters, length limits and explicit startup overrides");
}
''')
exe = output / 'player-name.exe'
subprocess.run([args.compiler, '-std=c++17', '-O2', '-I' + str(root / 'src'),
                str(source), '-o', str(exe)], check=True)
subprocess.run([str(exe)], cwd=output, check=True)
