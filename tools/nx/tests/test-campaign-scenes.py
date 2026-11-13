"""Check Numbers' script handoff adjustment without launching the game."""
from pathlib import Path
import argparse
import mmap
import os
import re
import struct
import subprocess
import zlib

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--compiler', default='C:/msys64/ucrt64/bin/g++.exe')
p.add_argument('--kbz', type=Path, default=Path('F:/pluto_t5_full_game/kbz/kowloon.kbz'))
a = p.parse_args()
root = Path(__file__).resolve().parents[3]
out = root / 'build-nx/campaign-scene-tests'
out.mkdir(parents=True, exist_ok=True)

def function(text, name):
    m = re.search(r'^[^\n]*\b' + re.escape(name) + r'\([^;]*?\)\s*\{', text, re.M)
    assert m, name
    start = m.start()
    end, depth = text.index('{', start) + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

# Use the shipped scene when available, and always exercise a small fixture.
fixtures = ['level waittill("interrogation_midway_through");\nwait( 35 );\nwait( 18 );\nlevel thread maps\\kowloon_gas_leak::event2();\nflag_set( "event2" );\n']
if a.kbz.is_file():
    with a.kbz.open('rb') as stream, mmap.mmap(stream.fileno(), 0, access=mmap.ACCESS_READ) as b:
        nb = struct.unpack_from('<I', b, 8)[0]
        sizes = struct.unpack_from('<'+'I'*nb, b, 12)
        nr, na = struct.unpack_from('<II', b, 12+4*nb)
        pos, blocks = 20+4*nb, []
        for i, n in enumerate(sizes):
            blocks.append(pos if i != 1 else None)
            pos += n if i != 1 else 0
        rel = {(sb,so):(tb,to) for sb,so,tb,to in
               (struct.unpack_from('<BIBI', b, j) for j in range(pos,pos+nr*10,10))}
        pos += nr*10
        def base(ref):
            return blocks[ref[0]]+ref[1]
        for _ in range(na):
            kind, ab, ao = struct.unpack_from('<IBI', b, pos)
            pos += 9
            np = rel.get((ab, ao))
            if not np:
                continue
            nameoff = base(np)
            name = b[nameoff:b.find(b'\0',nameoff)].decode()
            if kind == 36 and name in ('maps/kowloon_interrogate.gsc','maps/kowloon_anim.gsc'):
                length = struct.unpack_from('<i', b, base((ab,ao))+8)[0]
                offset = base(rel[(ab,ao+16)])
                text = zlib.decompress(b[offset+8:offset+length]).decode('latin1')
                if name.endswith('_interrogate.gsc'):
                    fixtures.append(text)
                else:
                    body = function(text, 'start_player_interrogation_animation')
                    assert body.index('self Unlink();') < body.index('self ShowViewModel();') < body.index('player_hands delete();')
                    assert 'self.player_hands = player_hands;' in body
            if kind == 4 and name == 'int_kowloon_b01_intro_player_out':
                offset = base((ab,ao))
                frames = struct.unpack_from('<H', b, offset+18)[0]
                fps = struct.unpack_from('<f', b, offset+52)[0]
                assert frames/fps > 35+18
                print(f'Confirmed stock animation overlap: {frames/fps:.1f}s camera, 53s combat, {frames/fps-53:.1f}s overlap')

is_defined = function((root/'src/game_mp/g_scr_main_mp.cpp').read_text(), 'GScr_IsDefined')
cpp = out/'scenes.cpp'
cpp.write_text(r'''
#include <clientscript/cscr_campaign_scenes.h>
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstdio>
int valueType=0,pointerType=19,result=-1;
constexpr int SCRIPTINSTANCE_SERVER=0;
int Scr_GetType(int,int){return valueType;}
int Scr_GetPointerType(int,int){return pointerType;}
void Scr_AddInt(int n,int){result=n;}
bool Assert_MyHandler(const char*,int,int,const char*,...){assert(false);return true;}
void __debugbreak(){assert(false);}
''' + is_defined + r'''
int main(int argc,char** argv){
 assert(argc==3);
 std::ifstream in(argv[1],std::ios::binary);assert(in);
 std::string original((std::istreambuf_iterator<char>(in)),{}),source=original;
 assert(!Scr_PatchCampaignSceneSource("maps/other.gsc",source)&&source==original);
 assert(Scr_PatchCampaignSceneSource("maps/kowloon_interrogate.gsc",source));
 const auto guard=source.find("while ( IsDefined( kisak_interrogation_player )");
 const auto start=source.find("level thread maps\\kowloon_gas_leak::event2();");
 const auto flag=source.find("flag_set( \"event2\" );");
 assert(guard<start&&start<flag);
 assert(source.find("kisak_interrogation_player.player_hands")<start);
 // Removing only the inserted guard must recover the original, byte for byte.
 auto restored=source;
 const auto begin=restored.find("// Wait for the camera to unlink");
 restored.erase(begin,start-begin);assert(restored==original);
 auto twice=source;assert(!Scr_PatchCampaignSceneSource("maps/kowloon_interrogate.gsc",twice)&&twice==source);
 for(const char* anchor:{"level waittill(\"interrogation_midway_through\");","wait( 18 );","level thread maps\\kowloon_gas_leak::event2();","flag_set( \"event2\" );"}){
  auto missing=original;missing.erase(missing.find(anchor),std::char_traits<char>::length(anchor));auto saved=missing;
  assert(!Scr_PatchCampaignSceneSource("maps/kowloon_interrogate.gsc",missing)&&missing==saved);
  auto duplicate=original+anchor;saved=duplicate;
  assert(!Scr_PatchCampaignSceneSource("maps/kowloon_interrogate.gsc",duplicate)&&duplicate==saved);
 }
 // Real script IsDefined recognizes a deleted camera entity as unavailable.
 valueType=1;pointerType=19;GScr_IsDefined();assert(result==1);
 pointerType=18;GScr_IsDefined();assert(result==0);
 valueType=0;GScr_IsDefined();assert(result==0);
 std::ofstream output(argv[2],std::ios::binary);output.write(source.data(),source.size());
 puts("PASS: combat handoff ordering, unchanged scene bytes, mods/duplicate guards, deleted-camera detection");
}
''')
exe = out/'scenes.exe'
env = os.environ.copy()
env['PATH'] = str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
subprocess.run([a.compiler,'-std=c++20','-O2','-I'+str(root/'src'),str(cpp),'-o',str(exe)],check=True,env=env)
for i, fixture in enumerate(fixtures):
    file, patched = out/f'input-{i}.gsc', out/f'patched-{i}.gsc'
    file.write_bytes(fixture.encode('latin1'))
    subprocess.run([str(exe),str(file),str(patched)],check=True,env=env)
    stripped = re.sub(r'//[^\n]*|/\*.*?\*/|/#.*?#/|"(?:\\.|[^"\\])*"', '', patched.read_text(encoding='latin1'), flags=re.S)
    stack = []
    for char in stripped:
        if char in '({[':
            stack.append(char)
        elif char in ')}]':
            assert stack and stack.pop() == {')':'(', '}':'{', ']':'['}[char]
    assert not stack
loader = (root/'src/clientscript/cscr_parser.cpp').read_text()
assert loader.count('sourceBuf = Scr_AdjustCampaignScenes(inst, extFilename, sourceBuf, &len);') == 2
assert 'inst != SCRIPTINSTANCE_SERVER || (zombiemode && zombiemode->current.enabled)' in loader
print(f'PASS: {len(fixtures)} script inputs; disk/KBZ loading hooks; no game launched (GSC compilation still requires gameplay testing)')
