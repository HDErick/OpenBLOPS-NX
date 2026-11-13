"""Check shipped in-match class-name layers and real DVAR-string/select readers."""
from pathlib import Path
import argparse
import json
import os
import re
import struct
import subprocess

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe')
parser.add_argument('--baseline',help='Source revision expected to reproduce blank offline names')
args=parser.parse_args()
root=Path(__file__).resolve().parents[3]
out=root/'build-nx/class-name-tests'
out.mkdir(parents=True,exist_ok=True)
path='src/universal/com_expressions_eval.cpp'
source=subprocess.check_output(['git','show',f'{args.baseline}:{path}'],cwd=root).decode() if args.baseline else (root/path).read_text()
def function(name):
 a=source.index(name);b=source.index('{',a)+1;depth=1
 while depth:depth+=(source[b]=='{')-(source[b]=='}');b+=1
 return source[a:b]

# Use actual menu relocations and expressions; verify normal and highlighted layers.
asset=Path('F:/pluto_t5_full_game/kbz/common_mp.kbz')
assert asset.exists(), 'Supply the local common_mp.kbz to verify the shipped labels'
b=asset.read_bytes();nb=struct.unpack_from('<I',b,8)[0];sizes=struct.unpack_from('<'+'I'*nb,b,12);nr,na=struct.unpack_from('<II',b,12+4*nb);pos=20+4*nb;blocks=[]
for i,n in enumerate(sizes):blocks.append(b[pos:pos+n] if i!=1 else b'');pos+=n if i!=1 else 0
rel={}
for _ in range(nr):sb,so,tb,to=struct.unpack_from('<BIBI',b,pos);pos+=10;rel[(sb,so)]=(tb,to)
def ptr(p,o=0):return rel.get((p[0],p[1]+o)) if p else None
def num(p,o):return struct.unpack_from('<i',blocks[p[0]],p[1]+o)[0]
def string(p):
 if not p:return ''
 data=blocks[p[0]];return data[p[1]:data.find(b'\0',p[1])].decode('utf8','replace')
labels=[]
for _ in range(na):
 t,ab,ao=struct.unpack_from('<IBI',b,pos);pos+=9;m=(ab,ao)
 if t!=22 or string(ptr(m))!='changeclass':continue
 table=ptr(m,448)
 for j in range(num(m,192)):
  item=ptr(table,j*8)
  if num(item,176) not in (1,3):continue
  exp=ptr(ptr(item,224),64);rp=ptr(exp,16)
  if not exp or not rp:continue
  keys=[string(ptr(rp,k*24+16)) for k in range(num(exp,12)) if num(rp,k*24)==0 and num(rp,k*24+8)==2]
  aliases=[key for key in keys if key.startswith('offline_customclass')]
  if aliases:
   assert len(aliases)==1 and aliases[0][8:] in keys
   labels.append(aliases[0])
assert len(labels)==15 and all(labels.count(f'offline_customclass{i}')==3 for i in range(1,6)),labels
print('PASS: all 15 shipped custom-class label layers use the offline-name branch',flush=True)
header=(root/'src/client_mp/cl_main_mp.h').read_text()
class_table=re.search(r'static const char \*customClassDvars\[11\] =\s*\{.*?\};',header,re.S).group()
code=r'''
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <map>
#include <vector>
#include <deque>
#include <stdexcept>
#include <strings.h>
#ifndef __cdecl
#define __cdecl
#endif
constexpr int VAL_INT=0,VAL_STRING=2,DVAR_TYPE_STRING=7;
struct itemDef_s{};
union Value {int intVal;const char *string;Value():intVal(0){}};
struct Operand {int dataType=VAL_INT;Value internals;};
struct OperandList {int operandCount=0;Operand operands[3];};
struct OperandStack {std::vector<Operand> values;int popped=0,added=0;};
struct dvar_s {int type=DVAR_TYPE_STRING;const char *name="";struct {const char *string="";int integer=0;} current;};
std::map<std::string,dvar_s> variables;
dvar_s debug;dvar_s *uiscript_debug=&debug;
const dvar_s *Dvar_FindVar(const char *name){auto it=variables.find(name);return it==variables.end()?nullptr:&it->second;}
const dvar_s *Dvar_FindVar(int){return nullptr;}
int I_strnicmp(const char *a,const char *b,int n){return strncasecmp(a,b,n);}
int I_stricmp(const char *a,const char *b){return strcasecmp(a,b);}
void GetOperand(OperandStack *s,Operand *out){assert(!s->values.empty());*out=s->values.back();s->values.pop_back();++s->popped;}
void GetOperandList(OperandStack *s,OperandList *out){out->operandCount=s->values.size();assert(out->operandCount<=3);for(int i=0;i<out->operandCount;++i)out->operands[i]=s->values[i];s->values.clear();}
Value GetOperandValueInt(Operand *op){return op->internals;}
void AddOperandToStack(OperandStack *s,Operand *op){s->values.push_back(*op);++s->added;}
const char *GetNameForValueType(int){return "other";}
void Expression_Error(const char *,...){throw std::runtime_error("expression error");}
void Expression_TraceInternal(const char *,...){}
const char *Dvar_DisplayableValue(const dvar_s *d){return d->current.string;}
// Match the expression engine's temporary string lifetime across select operands.
char *CopyTempString(const char *text){static std::deque<std::string> values;values.emplace_back(text);return values.back().data();}
'''+class_table+function('void __cdecl GetDvarStringValue(')+function('void __cdecl Select(')+r'''
Operand text(const char *value){Operand op;op.dataType=VAL_STRING;op.internals.string=value;return op;}
std::string read(const char *name){OperandStack stack;stack.values.push_back(text(name));GetDvarStringValue(0,nullptr,&stack);assert(stack.popped==1 && stack.added==1 && stack.values.size()==1);assert(stack.values[0].dataType==VAL_STRING);return stack.values[0].internals.string;}
std::string label(const char *offline,bool split,bool systemlink,bool training){
 const bool condition=(!split && !systemlink)||training;
 std::string normal=read(offline+8),local=read(offline);
 Operand flag;flag.internals.intVal=condition;OperandStack stack;stack.values={flag,text(normal.c_str()),text(local.c_str())};Select(0,nullptr,&stack);
 assert(stack.values.size()==1 && stack.values[0].dataType==VAL_STRING);return stack.values[0].internals.string;
}
int main(){
 for(int i=0;i<10;++i){dvar_s value;value.name=customClassDvars[i];value.current.string=i==0?"Renamed Rifle":"My loadout";variables[customClassDvars[i]]=value;}
 const char *labels[]={LABELS};
 for(const char *name:labels){
  const std::string expected=read(name+8);
#if defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH)
  assert(read(name)==expected);
  for(bool split:{false,true})for(bool link:{false,true})for(bool training:{false,true})assert(label(name,split,link,training)==expected);
#else
  assert(read(name).empty());assert(label(name,false,true,false).empty());assert(label(name,false,false,false)==expected);
#endif
 }
#if defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH)
 variables["customclass1"].current.string="Changed again";assert(read("offline_customclass1")=="Changed again");
 variables["customclass1"].current.string="";assert(read("offline_customclass1").empty());
 for(int i=5;i<10;++i)assert(read((std::string("offline_")+customClassDvars[i]).c_str())==read(customClassDvars[i]));
 assert(read("OFFLINE_CUSTOMCLASS2")=="My loadout");
#endif
 dvar_s unrelated;unrelated.current.string="Keep this value";
 for(const char *name:{"offline_customclass10","offline_customclass1_suffix","offline_unrelated"}){variables[name]=unrelated;assert(read(name)=="Keep this value");}
 assert(read("offline_missing").empty());
 puts("PASS: in-match class labels, highlights, renamed classes, mode branches, exact aliases and legacy/SP isolation");
}
'''
code=code.replace('LABELS',','.join(json.dumps(x) for x in labels))
env=dict(os.environ);env['PATH']=str(Path(args.compiler).parent)+os.pathsep+env.get('PATH','')
configs={'pc-direct-mp':['KISAK_MP','OPENBLOPS_NO_STEAM_AUTH'],'nx-direct-mp':['KISAK_MP','KISAK_NX','OPENBLOPS_NO_STEAM_AUTH'],'legacy-mp':['KISAK_MP'],'direct-sp':['KISAK_SP','OPENBLOPS_NO_STEAM_AUTH']}
if args.baseline:configs={'pc-direct-mp':configs['pc-direct-mp']}
for name,defines in configs.items():
 cpp=out/(name+'.cpp');exe=out/(name+'.exe');cpp.write_text(code)
 subprocess.run([args.compiler,'-std=c++17','-O0',*[f'-D{d}' for d in defines],str(cpp),'-o',str(exe)],check=True,env=env)
 print(name,flush=True);subprocess.run([str(exe)],check=True,env=env)
