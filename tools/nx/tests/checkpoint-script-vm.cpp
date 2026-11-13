// Execute the actual checkpoint VM reader/writer against a controlled object graph.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <deque>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#define iassert(x) assert(x)
constexpr int ERR_DROP=1, VAR_MASK=31, VAR_NAME_BITS=8, CLASS_NUM_COUNT=5;
constexpr unsigned VARIABLELIST_PARENT_BEGIN=1;
enum scriptInstance_t{SCRIPTINSTANCE_SERVER};
enum { VAR_UNDEFINED=0,VAR_POINTER=1,VAR_STRING=2,VAR_ISTRING=3,VAR_VECTOR=4,
 VAR_FLOAT=5,VAR_INTEGER=6,VAR_CODEPOS=7,VAR_PRECODEPOS=8,VAR_FUNCTION=9,
 VAR_STACK=10,VAR_ANIMATION=11,VAR_THREAD=13,VAR_NOTIFY_THREAD=14,
 VAR_TIME_THREAD=15,VAR_CHILD_THREAD=16,VAR_OBJECT=17,VAR_DEAD_ENTITY=18,
 VAR_ENTITY=19,VAR_ARRAY=20 };
struct VariableStackBuffer { const char* pos; uint16_t size,bufLen,localId; uint8_t time;char buf[1]; };
union VariableUnion { int intValue; float floatValue; unsigned stringValue,pointerValue;
 const float* vectorValue;const char* codePosValue;VariableStackBuffer* stackValue; };
constexpr int SCR_STACKBUF_HEADER=offsetof(VariableStackBuffer,buf);
constexpr int SCR_STACKBUF_ENTRY=1+sizeof(VariableUnion);
struct VariableValue { VariableUnion u{};int type=0; };
struct ObjectInfo { unsigned short refCount=0;union { unsigned self,entnum,size; } u{}; };
struct VariableValueInternal {
 struct { unsigned id=0;union { unsigned prev,prevSibling; }u{}; }hash;
 union U { unsigned next;VariableUnion u;ObjectInfo o;U():next(0){} }u;
 union W { unsigned type,status,name,classnum,notifyName,waitTime,parentLocalId;W():type(0){} }w;
};
struct Pub { unsigned time=0,gameId=0,levelId=0,animId=0,timeArrayId=0,pauseArrayId=0,freeEntList=0;
 uint16_t savecount=0;uint16_t *saveIdMap,*saveIdMapRev;
 const char *varUsagePos=nullptr;
 const char *programBuffer,*endScriptBuffer;unsigned numScriptThreads=0; }gScrVarPub[1];
using scrVarPub_t=Pub;
struct { VariableValueInternal* variableList; }gScrVarGlob[1];
struct { unsigned id=0,entArrayId=0; }gScrClassMap[1][5];
std::vector<VariableValueInternal> pool(0x48000);
uint16_t ids[0x7FFE]{},reverseIds[0x7FFE]{};
std::map<unsigned,std::vector<unsigned>> children;
std::map<std::pair<unsigned,unsigned>,unsigned> fields;
unsigned nextObject=1,nextChild=1;
std::deque<std::string> strings{""};
std::vector<std::unique_ptr<char[]>> allocations;
char program[4096]{};
struct MemoryFile { std::vector<unsigned char> data;size_t cursor=0; };
[[noreturn]] void Com_Error(int,const char* fmt,...) { char b[512];va_list a;va_start(a,fmt);vsnprintf(b,sizeof(b),fmt,a);va_end(a);throw std::runtime_error(b); }
void MemFile_WriteData(MemoryFile* f,unsigned n,unsigned char* p){f->data.insert(f->data.end(),p,p+n);}
void MemFile_WriteInt(MemoryFile* f,int v){MemFile_WriteData(f,4,(unsigned char*)&v);}
void MemFile_WriteFloat(MemoryFile* f,float v){MemFile_WriteData(f,4,(unsigned char*)&v);}
void MemFile_WriteCString(MemoryFile* f,char* s){MemFile_WriteData(f,(unsigned)strlen(s)+1,(unsigned char*)s);}
void MemFile_ReadData(MemoryFile* f,int n,unsigned char* p){if(n<0||(size_t)n>f->data.size()-f->cursor)throw std::runtime_error("truncated VM archive");memcpy(p,f->data.data()+f->cursor,n);f->cursor+=n;}
char* MemFile_ReadCString(MemoryFile* f){size_t a=f->cursor;while(f->cursor<f->data.size())if(!f->data[f->cursor++])return (char*)f->data.data()+a;throw std::runtime_error("truncated string");}
const char* SL_ConvertToString(unsigned id,scriptInstance_t){return strings.at(id).c_str();}
unsigned SL_GetStringOfSize(scriptInstance_t,const char* s,int,unsigned,int){for(unsigned i=0;i<strings.size();++i)if(strings[i]==s)return i;strings.emplace_back(s);return (unsigned)strings.size()-1;}
void* MT_Alloc(int n,int,scriptInstance_t){auto p=std::make_unique<char[]>(n);void* out=p.get();allocations.push_back(std::move(p));return out;}
const float* Scr_AllocVector(scriptInstance_t,const float* v){auto p=(float*)MT_Alloc(12,0,SCRIPTINSTANCE_SERVER);memcpy(p,v,12);return p;}
void AddRefToObject(scriptInstance_t,unsigned){}
void RemoveRefToObject(scriptInstance_t,unsigned){}
void RemoveRefToValue(scriptInstance_t,unsigned,VariableUnion){}
unsigned AllocObject(scriptInstance_t){unsigned id=nextObject++;pool[id+1]={};return id;}
unsigned AllocValue(scriptInstance_t){unsigned id=nextChild++;pool[0x8000+id]={};return id;}
unsigned FindFirstSibling(scriptInstance_t,unsigned id){return children[id].empty()?0:children[id].front();}
unsigned FindLastSibling(scriptInstance_t,unsigned id){return children[id].empty()?0:children[id].back();}
unsigned FindNextSibling(scriptInstance_t,unsigned child){for(auto& [id,entries]:children)for(size_t i=0;i<entries.size();++i)if(entries[i]==child)return i+1<entries.size()?entries[i+1]:0;return 0;}
unsigned GetVariable(scriptInstance_t,unsigned parent,unsigned name){auto key=std::make_pair(parent,name);auto it=fields.find(key);if(it!=fields.end())return it->second;
 unsigned id=AllocValue(SCRIPTINSTANCE_SERVER);auto& p=pool[0x8000+id];p.hash.id=id;p.hash.u.prevSibling=FindLastSibling(SCRIPTINSTANCE_SERVER,parent);p.w.name=name<<8;children[parent].push_back(id);fields[key]=id;return id;}
void SetNewVariableValue(scriptInstance_t,unsigned id,VariableValue* v){auto& p=pool[0x8000+id];p.u.u=v->u;p.w.type=(p.w.type&~31)|v->type;}
VariableValue Scr_GetArrayIndexValue(scriptInstance_t,unsigned){return {};}
#include "checkpoint-vm-functions.inc"
VariableUnion integer(int n){VariableUnion v{};v.intValue=n;return v;}
VariableUnion pointer(unsigned n){VariableUnion v{};v.pointerValue=n;return v;}
void field(unsigned parent,unsigned name,int type,VariableUnion v){VariableValue value{v,type};SetNewVariableValue(SCRIPTINSTANCE_SERVER,GetVariable(SCRIPTINSTANCE_SERVER,parent,name),&value);}
VariableValueInternal& at(unsigned parent,unsigned name){return pool[0x8000+fields.at({parent,name})];}
unsigned object(int type){unsigned id=AllocObject(SCRIPTINSTANCE_SERVER);pool[id+1].w.type=type;return id;}
void reset(unsigned objectBase){children.clear();fields.clear();pool.assign(pool.size(),{});nextObject=objectBase;nextChild=1;memset(ids,0,sizeof(ids));memset(reverseIds,0,sizeof(reverseIds));gScrVarPub[0]={};
 auto& p=gScrVarPub[0];p.saveIdMap=ids;p.saveIdMapRev=reverseIds;p.programBuffer=program;p.endScriptBuffer=program+sizeof(program);gScrVarGlob[0].variableList=pool.data();memset(gScrClassMap,0,sizeof(gScrClassMap));}
int main(int argc,char** argv){
 assert(argc==2);reset(1);auto& p=gScrVarPub[0];
 unsigned level=object(VAR_OBJECT),array=object(VAR_ARRAY),entity=object(VAR_ENTITY);
 pool[entity+1].u.o.u.entnum=37;pool[entity+1].w.classnum|=2<<8;
 unsigned wait=object(VAR_TIME_THREAD),notify=object(VAR_NOTIFY_THREAD),child=object(VAR_CHILD_THREAD);
 pool[wait+1].u.o.u.self=entity;pool[wait+1].w.waitTime|=1234<<8;
 unsigned text=SL_GetStringOfSize(SCRIPTINSTANCE_SERVER,"revive_wait",0,0,0);
 pool[notify+1].u.o.u.self=entity;pool[notify+1].w.notifyName|=text<<8;
 pool[child+1].u.o.u.self=entity;pool[child+1].w.parentLocalId|=wait<<8;
 p.levelId=level;p.time=70123;p.pauseArrayId=array;p.timeArrayId=wait;
 gScrClassMap[0][0].id=entity;
 field(level,1,VAR_POINTER,pointer(array));field(level,2,VAR_POINTER,pointer(entity));
 field(level,3,VAR_POINTER,pointer(notify));field(level,4,VAR_POINTER,pointer(child));
 // Object aliases, a cycle, string keys, object keys and signed integer keys.
 field(array,text,VAR_POINTER,pointer(level));field(array,0x10000+entity,VAR_POINTER,pointer(entity));
 for(int n:{-2097151,-8192,-33,-32,-1,0,15,16,31,32,8191,8192,2097151})
  field(array,(unsigned)(n+0x800000),VAR_INTEGER,integer(n*2));
 float vector[3]={1.25f,-2.5f,8};VariableUnion v{};v.vectorValue=vector;field(level,5,VAR_VECTOR,v);
 v.floatValue=3.125f;field(level,6,VAR_FLOAT,v);v.stringValue=text;field(level,7,VAR_STRING,v);
 v.codePosValue=program+101;field(level,8,VAR_FUNCTION,v);v.codePosValue=nullptr;field(level,9,VAR_CODEPOS,v);
 // Packed archived VM entries have different native widths on PC and Switch.
 const int count=5;auto stack=(VariableStackBuffer*)MT_Alloc(SCR_STACKBUF_HEADER+count*SCR_STACKBUF_ENTRY,0,SCRIPTINSTANCE_SERVER);
 stack->size=count;stack->bufLen=SCR_STACKBUF_HEADER+count*SCR_STACKBUF_ENTRY;stack->localId=child;stack->time=7;stack->pos=program+317;
 VariableValue values[5]={{integer(-44),VAR_INTEGER},{pointer(entity),VAR_POINTER},{ {},VAR_VECTOR},{ {},VAR_STRING},{ {},VAR_FUNCTION}};
 values[2].u.vectorValue=vector;values[3].u.stringValue=text;values[4].u.codePosValue=program+199;
 for(int i=0;i<count;++i){char* e=stack->buf+i*SCR_STACKBUF_ENTRY;*e=(char)values[i].type;memcpy(e+1,&values[i].u,sizeof(VariableUnion));}
 v.stackValue=stack;field(level,10,VAR_STACK,v);
 p.gameId=AllocValue(SCRIPTINSTANCE_SERVER);VariableValue game{pointer(array),VAR_POINTER};SetNewVariableValue(SCRIPTINSTANCE_SERVER,p.gameId,&game);
 Scr_PrepareSave(SCRIPTINSTANCE_SERVER);MemoryFile file;Scr_SaveVariables(SCRIPTINSTANCE_SERVER,&file);
 FILE* disk=fopen(argv[1],"wb");assert(disk);fwrite(file.data.data(),1,file.data.size(),disk);fclose(disk);
 // Force relocation of every object and string ID before loading.
 reset(100);strings.clear();strings.emplace_back("");strings.emplace_back("different");
 Scr_LoadVariables(SCRIPTINSTANCE_SERVER,&file);assert(file.cursor==file.data.size());
 unsigned restored=p.levelId,ra=at(restored,1).u.u.pointerValue,re=at(restored,2).u.u.pointerValue;
 assert(restored>=100 && restored!=level && p.time==70123);
 assert(pool[re+1].u.o.u.entnum==37 && (pool[re+1].w.classnum>>8)==2);
 unsigned rt=at(restored,3).u.u.pointerValue,rc=at(restored,4).u.u.pointerValue;
 assert(pool[rc+1].u.o.u.self==re && (pool[rc+1].w.parentLocalId>>8)==p.timeArrayId);
 assert((pool[p.timeArrayId+1].w.waitTime>>8)==1234);
 assert(!strcmp(SL_ConvertToString(pool[rt+1].w.notifyName>>8,SCRIPTINSTANCE_SERVER),"revive_wait"));
 unsigned newText=at(restored,7).u.u.stringValue;
 assert(at(ra,newText).u.u.pointerValue==restored && at(ra,0x10000+re).u.u.pointerValue==re);
 for(int n:{-2097151,-8192,-33,-32,-1,0,15,16,31,32,8191,8192,2097151})assert(at(ra,(unsigned)(n+0x800000)).u.u.intValue==n*2);
 assert(at(restored,6).u.u.floatValue==3.125f && at(restored,5).u.u.vectorValue[1]==-2.5f);
 assert(at(restored,8).u.u.codePosValue==program+101 && at(restored,9).u.u.codePosValue==nullptr);
 auto st=at(restored,10).u.u.stackValue;assert(st->size==count && st->localId==rc && st->pos==program+317 && st->time==7);
 VariableUnion loaded[5]{};for(int i=0;i<count;++i)memcpy(&loaded[i],st->buf+i*SCR_STACKBUF_ENTRY+1,sizeof(VariableUnion));
 assert(loaded[0].intValue==-44 && loaded[1].pointerValue==re && loaded[2].vectorValue[0]==1.25f && loaded[3].stringValue==newText && loaded[4].codePosValue==program+199);
 assert(pool[0x8000+p.gameId].u.u.pointerValue==ra);
 Scr_PostLoadVariables(SCRIPTINSTANCE_SERVER);
 // Invalid references/code positions and oversized object counts fail closed.
 bool rejected=false;MemoryFile bad;Scr_WriteSaveShort(&bad,0x7FFF);
 try{Scr_ReadObjectReference(SCRIPTINSTANCE_SERVER,&bad,0);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 rejected=false;bad={};Scr_WriteSaveInt(&bad,4096);
 try{Scr_ReadSaveCodePos(SCRIPTINSTANCE_SERVER,&bad);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 rejected=false;bad={};Scr_WriteSaveInt(&bad,1);Scr_WriteSaveShort(&bad,0x7FFE);
 try{Scr_LoadVariables(SCRIPTINSTANCE_SERVER,&bad);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 printf("PASS: %u-bit VM graph, relocated IDs, signed array keys, waits/notifies, packed stack and bounds (%zu bytes)\n",unsigned(sizeof(void*)*8),file.data.size());
}
