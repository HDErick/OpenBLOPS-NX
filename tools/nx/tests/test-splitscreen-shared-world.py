"""Test shared rope/glass decoding for two local connections without a game."""
from pathlib import Path
import argparse
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler',default='/c/msys64/ucrt64/bin/g++.exe')
args=parser.parse_args();root=Path(__file__).resolve().parents[3]
out=root/'build-nx/splitscreen-guest-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,signature):
    s=(root/path).read_text();a=s.index(signature);b=s.index('{',a)+1;depth=1
    while depth:depth+=(s[b]=='{')-(s[b]=='}');b+=1
    return s[a:b]
code=r'''
#include <cassert>
#include <cstdio>
#include <vector>
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __thiscall
#define __thiscall
#endif
struct msg_t {std::vector<int> bits,shorts,longs;std::vector<float> floats;unsigned bit=0,shortIndex=0,longIndex=0,floatIndex=0;bool discarded=false;};
int MSG_ReadBit(msg_t *m){return m->bits.at(m->bit++);}
int MSG_ReadBits(msg_t *m,unsigned){return MSG_ReadBit(m);}
int MSG_ReadShort(msg_t *m){return m->shorts.at(m->shortIndex++);}
int MSG_ReadLong(msg_t *m){return m->longs.at(m->longIndex++);}
float MSG_ReadFloat(msg_t *m){return m->floats.at(m->floatIndex++);}
void MSG_Discard(msg_t *m){m->discarded=true;}
template<class... T>void Com_PrintError(T...){}
bool CL_LocalClient_IsFirstActive(int client){return client==0;}
constexpr int CONSTRAINT_ROPE=1,ATTACH_POINT_ENT=1;
struct PhysConstraint {int type=CONSTRAINT_ROPE,attach_point_type1=ATTACH_POINT_ENT,attach_point_type2=ATTACH_POINT_ENT,target_index1=-100,target_index2=-200;};
struct {int num_constraints=2;PhysConstraint constraints[2];} cm;
int ropeCreates=0;
void CreateRopes(int client){assert(client==0);++ropeCreates;}
namespace GlassState {enum State {PRISTINE=0,CRACKED=1,SHATTERED=2};}
int glassWrites=0;
struct GlassClient {void SetState(int client,GlassState::State,const float *,float *,bool){assert(client==0);++glassWrites;}};
struct GlassesClient {unsigned numGlasses=2;GlassClient glasses[2];void ParseSnapshot(int localClientNum,msg_t *msg,bool gameState);};
'''
code+=function('src/physics/rope_gamestate.cpp','void __cdecl Rope_ReadGameState(')+'\n'
code+=function('src/glass/glass_client.cpp','void __thiscall GlassesClient::ParseSnapshot(')+'\n'
code+=r'''
int main(){
    cm.constraints[1].attach_point_type2=0;
    msg_t ropes;ropes.longs={7,8,9};
    Rope_ReadGameState(&ropes,1);
    assert(ropes.longIndex==3&&ropeCreates==0);
    assert(cm.constraints[0].target_index1==-100&&cm.constraints[0].target_index2==-200);
    assert(cm.constraints[1].target_index1==-100&&cm.constraints[1].target_index2==-200);
    ropes.longIndex=0;Rope_ReadGameState(&ropes,0);
    assert(ropeCreates==1&&cm.constraints[0].target_index1==7&&cm.constraints[0].target_index2==8);
    assert(cm.constraints[1].target_index1==9&&cm.constraints[1].target_index2==-200);
    GlassesClient glasses;
    for(int client:{1,0}){
        msg_t full;full.bits={1,1,1,2};full.shorts={2,30154};full.floats={1,2,3,4,5,6};
        const int before=glassWrites;glasses.ParseSnapshot(client,&full,true);
        assert(!full.discarded&&full.bit==4&&full.shortIndex==2&&full.floatIndex==6);
        assert(glassWrites-before==(client==0?2:0));
        msg_t delta;delta.bits={1,0,1,2};delta.shorts={1,30154};delta.floats={1,2,3,4,5,6};
        const int afterFull=glassWrites;glasses.ParseSnapshot(client,&delta,false);
        assert(!delta.discarded&&delta.bit==4&&delta.shortIndex==2&&delta.floatIndex==6);
        assert(glassWrites-afterFull==(client==0?1:0));
        msg_t empty;empty.bits={0};empty.shorts={30154};
        glasses.ParseSnapshot(client,&empty,true);
        assert(!empty.discarded&&empty.bit==1&&empty.shortIndex==1);
    }
    puts("PASS: both connections consume identical rope/glass payloads; shared world and effects are applied once");
}
'''
cpp=out/'shared-world.cpp';cpp.write_text(code);exe=out/'shared-world.exe'
subprocess.run([args.compiler,'-std=c++20',str(cpp),'-o',str(exe)],check=True)
subprocess.run([str(exe)],check=True)
