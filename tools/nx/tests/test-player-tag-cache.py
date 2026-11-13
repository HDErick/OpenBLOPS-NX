"""Regress SP remote-player tag queries across model removal/recreation."""
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/c/msys64/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
out = root / 'build-nx/player-tag-tests'
out.mkdir(parents=True, exist_ok=True)
source = (root / 'src/cgame_mp/cg_main_mp.cpp').read_text()
def function(name):
    start = source.index('int __cdecl '+name+'(') if name != 'CachedTag_CalcTagPos' else source.index('void __cdecl '+name+'(')
    end = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]
code = r'''
#include <cassert>
#include <cstring>
#include <stdexcept>
#include <cstdio>
#ifndef __cdecl
#define __cdecl
#endif
#define __debugbreak() assert(false)
constexpr int ERR_DROP=1, SCRIPTINSTANCE_SERVER=0;
bool Assert_MyHandler(...) { assert(false); return false; }
struct cached_client_tag_t { int time; float lastLocalTagOrigin[3]; bool visible; int predictedTime; float predictedPosition[3], predictedAngles[3]; };
struct ClientTagCache { cached_client_tag_t aim_head_tag, aim_highest_tag; };
struct centity_s { struct { int number, eType; } nextState; struct { int localClientNum; float origin[3]; } pose; ClientTagCache *clientTagCache; };
struct cg_s { int time; } cg{1000};
struct DObj {} object;
bool modelReady = true, tagReady = true;
int tagReads = 0;
struct { unsigned aim_vis_bone=1, j_head=1, aim_highest_bone=2; } scr_const;
DObj *Com_GetClientDObj(int entity, int client) { assert(entity == 1 && client == 0); return modelReady ? &object : nullptr; }
cg_s *CG_GetLocalClientGlobals(int client) { assert(client == 0); return &cg; }
bool CG_DObjGetWorldTagPos(const void *, DObj *obj, unsigned, float *pos) { assert(obj); ++tagReads; pos[0]=12; pos[1]=24; pos[2]=100; return tagReady; }
char *SL_ConvertToString(unsigned, int) { static char name[]="j_head"; return name; }
[[noreturn]] void Com_Error(int, const char *, ...) { throw std::runtime_error("fatal tag lookup"); }
'''
# Dependencies need declarations because the real translation unit uses headers.
code += r'''
int CachedTag_NoCache_GetTagPos(const centity_s *, unsigned, float *);
int CachedTag_UpdateTagInternal(const centity_s *, cached_client_tag_t *, unsigned, int, bool);
void CachedTag_CalcTagPos(const centity_s *, cached_client_tag_t *, float *);
'''
for name in ('CachedTag_GetCachedTagPos', 'CachedTag_UpdateTagInternal', 'CachedTag_CalcTagPos', 'CachedTag_NoCache_GetTagPos', 'CachedTag_GetTagPos'):
    code += function(name) + '\n'
code += r'''
int main() {
    ClientTagCache cache{};
    centity_s ent{{1,1},{0,{10,20,30}},&cache};
    float pos[3]{};
    assert(CachedTag_GetTagPos(&ent,1,pos,200,false) == 1000);
    assert(tagReads == 1 && pos[2] == 100);
    cg.time = 1100;
    ent.pose.origin[0] += 10;
    assert(CachedTag_GetTagPos(&ent,1,pos,200,false) == 1000);
    assert(tagReads == 1 && pos[0] == 22);
    modelReady = false;
#ifdef KISAK_SP
    // This is inside the throttle window: checking only at update time would
    // keep using the deleted model's cached tag and later hit the fatal path.
    pos[0]=pos[1]=pos[2]=-999;
    assert(CachedTag_GetTagPos(&ent,1,pos,200,false) == 0);
    assert(cache.aim_head_tag.time == 0 && cache.aim_head_tag.lastLocalTagOrigin[2] == 0);
    assert(pos[2] == -999 && tagReads == 1);
    assert(CachedTag_GetTagPos(&ent,1,pos,200,true) == 0);
    ent.clientTagCache = nullptr;
    assert(CachedTag_GetTagPos(&ent,1,pos,200,true) == 0);
    ent.clientTagCache = &cache;
    modelReady = true;
    assert(CachedTag_GetTagPos(&ent,1,pos,200,false) == 1100);
    assert(tagReads == 2 && pos[2] == 100);
    // A real missing bone on a present model remains an error.
    tagReady = false;
    bool threw = false;
    try { CachedTag_GetTagPos(&ent,1,pos,200,true); } catch(const std::runtime_error &) { threw=true; }
    assert(threw);
#else
    // Preserve existing MP behavior outside this SP correction.
    cg.time=1300;
    bool threw = false;
    try { CachedTag_GetTagPos(&ent,1,pos,200,true); } catch(const std::runtime_error &) { threw=true; }
    assert(threw);
#endif
    puts("PASS: actual player tag cache functions");
}
'''
cpp = out / 'tags.cpp'
cpp.write_text(code, encoding='utf-8')
for name, definitions in [('pc-sp',['-DWIN32','-DKISAK_SP']), ('nx-sp',['-DKISAK_NX','-DKISAK_SP']), ('legacy-mp',['-DWIN32','-DKISAK_MP'])]:
    exe = out / (name+'.exe')
    subprocess.run([args.compiler,'-std=c++17','-O2',*definitions,str(cpp),'-o',str(exe)], check=True)
    subprocess.run([str(exe)],check=True)
    print(name+': passed',flush=True)
