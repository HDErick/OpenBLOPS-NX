"""Exercise periodic sound reports after the owning asset memory is released."""
from pathlib import Path
import argparse
import re
import subprocess

root = Path(__file__).resolve().parents[3]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
parser.add_argument('--source', type=Path, default=root / 'src/nx/nx_snd.cpp')
args = parser.parse_args()
output = root / 'build-nx/sound-lifecycle-tests'
output.mkdir(parents=True, exist_ok=True)
source = args.source.read_text()
has_tallies = 'struct NxStartTally' in source


def function(name):
    match = re.search(r'^void ' + name + r'\([^;]*?\)\s*\{', source, re.M)
    assert match, name
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


declarations = ''
names = ['report']
if has_tallies:
    for name in ('NxStartTally', 'NxCallerTally'):
        declarations += re.search(r'struct ' + name + r' \{.*?\};', source).group() + '\n'
    declarations += 'NxStartTally s_tally[64], s_cutTally[64]; NxCallerTally s_loopStops[16];\n'
    names = ['noteTally', 'noteCaller', 'printLoopStops', 'printTop', 'report']
generated = r'''
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cassert>
using u64 = uint64_t;
using u32 = uint32_t;
struct snd_alias_t { const char *name; };
struct MemoryInfo { u64 addr; };
void svcQueryMemory(MemoryInfo *out, u32 *, u64) { out->addr = 0; }
u64 armGetSystemTick() { static u64 time; return time += 11000000000ull; }
u64 armTicksToNs(u64 ticks) { return ticks; }
unsigned s_nStartPcm, s_nStartAdpcm, s_nStartWma, s_nRefusedWma, s_nRefusedOther, s_nStarved;
unsigned s_peakRunning, s_nEnded, s_nCut, s_nWmaErrors, s_nLoopStops;
u64 s_mixTicks, s_reportTick;
char s_lastRefused[64];
'''
generated += declarations + '\n'.join(function(name) for name in names)
generated += r'''
int main(int argc, char **argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        auto *zone = static_cast<char *>(VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        assert(zone);
        auto *alias = reinterpret_cast<snd_alias_t *>(zone);
        strcpy(zone + 128, "frontend_sound_alias");
        alias->name = zone + 128;
        report();
        RECORD_ALIAS
        DWORD old;
        assert(VirtualProtect(zone, 4096, PAGE_NOACCESS, &old));
        s_nStartWma = 2;
        s_nEnded = 6;
        report();
        assert(s_nStartWma == 0 && s_nEnded == 0);
        assert(VirtualFree(zone, 0, MEM_RELEASE));
    }
}
'''
record = '''if (argc > 1 && !strcmp(argv[1], "caller")) noteCaller(0x1234, alias);
        else noteTally(s_tally, alias, false);''' if has_tallies else '(void)argc; (void)argv;'
generated = generated.replace('RECORD_ALIAS', record)
cpp, exe = output / 'sound-lifecycle.cpp', output / 'sound-lifecycle.exe'
cpp.write_text(generated)
subprocess.run([args.compiler, '-std=c++17', '-O2', str(cpp), '-o', str(exe)], check=True)
for mode in ('alias', 'caller'):
    with (output / (mode + '.log')).open('wb') as log:
        subprocess.run([str(exe), mode], stdout=log, stderr=subprocess.STDOUT, check=True)
print('PASS: sound reports retain aggregate counters without accessing released asset memory')
