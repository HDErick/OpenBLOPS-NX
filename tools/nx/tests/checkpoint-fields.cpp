#include <game_mp/checkpoint_fields.h>
#include <game_mp/checkpoint_physics.h>
#include <game_mp/checkpoint_animation.h>
#include <game_mp/checkpoint_identity.h>
#include <game_mp/checkpoint_record_set.h>
#include <game_mp/checkpoint_resource_catalog.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <limits>
using namespace checkpoint;
enum class State : std::int32_t{Dead=-1,Alive=7};
struct Handle {std::uint16_t number=0,infoIndex=0;};
struct Fixture {int health=-71;unsigned ammo=999;float origin[3]={1.5f,-3.25f,0};double time=-12.5;bool dead=true;char name[4]={'A','B',0,0};State state=State::Dead;int* owner=nullptr;int* alias=nullptr;int* actor=nullptr;Handle handle{37,876};const char* text=nullptr;const char* empty="";const char* absent=nullptr;std::intptr_t physics=-1;unsigned char scalar[8]={1,2,3,4,5,6,7,8};};
template<class IO>void visit(IO& io,Fixture& x){io.Field("health",x.health);io.Field("ammo",x.ammo);io.Field("origin",x.origin);io.Field("time",x.time);io.Field("dead",x.dead);io.Field("name",x.name);io.Field("state",x.state);io.Ref("owner",x.owner,ResourceKind::Entity);io.Ref("alias",x.alias,ResourceKind::Entity);io.Ref("actor",x.actor,ResourceKind::Actor);io.Handle("handle",x.handle);io.Text("objective",x.text);io.Text("empty",x.empty);io.Text("absent",x.absent);io.Opaque("physics",x.physics);io.ScalarUnion("scalar",x.scalar,sizeof(x.scalar));}
struct TestBodyState {
 float position[3]{},rotation[3][3]{},velocity[3]{},angVelocity[3]{},centerOfMassOffset[3]{},buoyancyBoxMin[3]{},buoyancyBoxMax[3]{};
 float mass=1,friction=0,bounce=0;int timeLastAsleep=0,id=0,buoyancy=0,underwater=0;
};
int main(int argc,char** argv){
 assert(argc==2);Fixture source;int target=3;source.owner=source.alias=source.actor=&target;std::string title="Follow the checkpoint objective";source.text=title.c_str();Resources refs;
 std::vector<std::uint8_t> bytes;FieldWriter writer(bytes,refs);visit(writer,source);assert(refs.entries.size()==2);
 std::ofstream file(argv[1],std::ios::binary);file.write((const char*)bytes.data(),bytes.size());file.close();
 title.assign(title.size(),'X');Fixture restored{};FieldReader reader(bytes,refs);visit(reader,restored);reader.Finish();
 assert(restored.health==-71&&restored.ammo==999&&restored.origin[1]==-3.25f&&restored.time==-12.5&&restored.dead&&restored.state==State::Dead);
 assert(restored.owner==&target&&restored.alias==restored.owner&&restored.actor==restored.owner);
 assert(restored.handle.number==37&&restored.handle.infoIndex==0);
 assert(std::string(restored.text)=="Follow the checkpoint objective"&&restored.text!=source.text);
 assert(restored.empty&&restored.empty[0]==0&&restored.absent==nullptr&&restored.physics==-1);
 assert(!memcmp(restored.scalar,source.scalar,8));
 std::vector<std::uint8_t> again;FieldWriter rewriter(again,refs);visit(rewriter,restored);assert(again==bytes);
 for(std::size_t n=0;n<bytes.size();++n){auto truncated=bytes;truncated.resize(n);bool rejected=false;Fixture staging;try{FieldReader r(truncated,refs);visit(r,staging);r.Finish();}catch(const std::runtime_error&){rejected=true;}assert(rejected);}
 bool rejected=false;auto bad=bytes;bad[0]^=1;try{FieldReader r(bad,refs);visit(r,restored);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 // A valid resource ordinal with a different resource type must not alias.
 std::vector<std::uint8_t> onlyRef;FieldWriter rw(onlyRef,refs);rw.Ref("resource",source.owner,ResourceKind::Entity);
 onlyRef[9]=2;rejected=false;try{FieldReader r(onlyRef,refs);int* p=nullptr;r.Ref("resource",p,ResourceKind::Entity);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 std::string longText(4097,'a');rejected=false;try{FieldWriter w(again,refs);w.Text("long",longText.c_str());}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 // Native physics pointer values are represented by resource IDs, not addresses.
 std::vector<std::uint8_t> opaque;FieldWriter ow(opaque,refs);std::intptr_t physics=reinterpret_cast<std::intptr_t>(&target);ow.Opaque("physics",physics);
 FieldReader op(opaque,refs);std::intptr_t loaded=0;op.Opaque("physics",loaded);op.Finish();assert(loaded==physics);
 // Rehydration binds symbolic strings/VM references to different runtime IDs.
 Resources before,after;before.stringName=[](std::uint16_t id)->const char*{assert(id==79);return "tag_weapon";};
 before.BindScript(123,1);after.findString=[](const char* name)->std::uint16_t{assert(std::string(name)=="tag_weapon");return 502;};after.BindScript(309,1);
 std::vector<std::uint8_t> symbolic;FieldWriter sw(symbolic,before);sw.StringId("tag",79);sw.StringId("null_tag",0);sw.ScriptObject("thread",123u);sw.ScriptObject("null_thread",0u);
 FieldReader sr(symbolic,after);std::uint16_t tag=0,nullTag=7;unsigned thread=0,nullThread=7;sr.StringId("tag",tag);sr.StringId("null_tag",nullTag);sr.ScriptObject("thread",thread);sr.ScriptObject("null_thread",nullThread);sr.Finish();
 assert(tag==502&&nullTag==0&&thread==309&&nullThread==0);
 after.stringName=[](std::uint16_t id)->const char*{assert(id==502);return "tag_weapon";};
 std::vector<std::uint8_t> relocated;FieldWriter aw(relocated,after);aw.StringId("tag",tag);aw.StringId("null_tag",nullTag);aw.ScriptObject("thread",thread);aw.ScriptObject("null_thread",nullThread);assert(relocated==symbolic);
 rejected=false;try{sw.ScriptObject("unprepared",77u);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 rejected=false;try{sw.ScriptObject("invalid",0x10000u);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 rejected=false;try{after.BindScript(777,1);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 Resources empty;rejected=false;try{FieldReader missing(symbolic,empty);missing.StringId("tag",tag);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 // Include symbolic fields in the cross-ABI comparison artifact.
 std::ofstream append(argv[1],std::ios::binary|std::ios::app);append.write((const char*)symbolic.data(),symbolic.size());append.close();
 AnimationTree tree;tree.present=true;tree.asset="human";tree.assetSize=17;tree.hiddenParts[2]=0x87654321u;
 tree.nodes.resize(3);for(unsigned i=0;i<3;++i)tree.nodes[i].savedInfo=static_cast<unsigned short>(100+i);tree.nodes[0].animation=0;tree.nodes[1].animation=4;tree.nodes[1].parent=1;tree.nodes[1].notifyName="anim_done";tree.nodes[1].rate=-0.5f;tree.nodes[1].cycle=-3;tree.nodes[1].instant=true;tree.nodes[2].animation=9;tree.nodes[2].parent=1;
 std::vector<std::uint8_t> animBytes;FieldWriter animWriter(animBytes,refs);AnimationFields(animWriter,tree);
 AnimationTree loadedTree;FieldReader animReader(animBytes,refs);AnimationFields(animReader,loadedTree);animReader.Finish();
 assert(loadedTree.nodes.size()==3&&loadedTree.hiddenParts[2]==0x87654321u&&loadedTree.nodes[1].cycle==-3&&std::string(loadedTree.nodes[1].notifyName)=="anim_done");
 for(unsigned variant=0;variant<5;++variant){auto badTree=tree;if(variant==0)badTree.nodes[1].parent=2;if(variant==1)badTree.nodes[2].savedInfo=badTree.nodes[1].savedInfo;if(variant==2)badTree.nodes[1].animation=17;if(variant==3)badTree.present=false;if(variant==4)badTree.nodes.resize(4096);
  rejected=false;try{ValidateAnimationTree(badTree);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 }
 for(std::size_t n=0;n<animBytes.size();++n){auto truncated=animBytes;truncated.resize(n);rejected=false;try{AnimationTree badTree;FieldReader r(truncated,refs);AnimationFields(r,badTree);r.Finish();}catch(const std::runtime_error&){rejected=true;}assert(rejected);}
 std::ofstream animFile(argv[1],std::ios::binary|std::ios::app);animFile.write((const char*)animBytes.data(),animBytes.size());animFile.close();
 // Restarted clips have duplicate asset indices under distinct live parents.
 // This formerly rejected later mission saves and would merge their subtrees.
 AnimationTree restarted=tree;restarted.nodes.resize(7);
 const unsigned animations[]={0,4,9,4,9,0,4},parents[]={0,1,2,1,4,0,6};
 for(unsigned i=0;i<7;++i){auto& node=restarted.nodes[i];node={};node.savedInfo=static_cast<unsigned short>(200+i);node.animation=static_cast<unsigned short>(animations[i]);node.parent=parents[i];node.time=i*0.1f;node.weight=1.0f-i*0.1f;}
 auto parentOf=[](unsigned animation){return animation==9?4u:0u;};auto leaf=[](unsigned animation){return animation==9;};
 ValidateAnimationAssetTopology(restarted,parentOf,leaf);
 std::vector<std::uint8_t> restartBytes;FieldWriter restartWriter(restartBytes,refs);AnimationFields(restartWriter,restarted);
 AnimationTree restartLoaded;FieldReader restartReader(restartBytes,refs);AnimationFields(restartReader,restartLoaded);restartReader.Finish();
 std::vector<unsigned> allocatedParents;std::vector<float> restoredTimes;
 const auto instances=RebuildAnimationInstances(restartLoaded,[&](const AnimationNode& node,unsigned parent){allocatedParents.push_back(parent);restoredTimes.push_back(node.time);return 1000u+static_cast<unsigned>(allocatedParents.size());});
 assert(instances.size()==8&&allocatedParents==std::vector<unsigned>({0,1001,1002,1001,1004,0,1006}));
 assert(instances[2]!=instances[4]&&instances[3]!=instances[5]&&restoredTimes[2]!=restoredTimes[4]);
 for(unsigned v=0;v<7;++v){auto bad=restarted;if(v==0)bad.nodes[4].parent=2;if(v==1)bad.nodes[4].parent=1;if(v==2)bad.nodes[4].parent=3;if(v==3)bad.nodes[4].savedInfo=0;if(v==4)bad.nodes[4].savedInfo=4096;if(v==5)bad.nodes[5].animation=4;if(v==6)bad.nodes[2].animation=0;
  rejected=false;try{ValidateAnimationAssetTopology(bad,parentOf,leaf);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 }
 rejected=false;try{RebuildAnimationInstances(restarted,[](const AnimationNode&,unsigned){return 7u;});}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 std::ofstream restartFile(argv[1],std::ios::binary|std::ios::app);restartFile.write((const char*)restartBytes.data(),restartBytes.size());restartFile.close();
 puts("PASS: repeated animation clips, cloned subtrees/roots, distinct playback, relocated parents and malformed instance rejection");
 // Moving props retain motion/properties without native body addresses.
 EntityPhysicsState<TestBodyState> physicsState,loadedPhysics;
 physicsState.body.id=731;physicsState.body.mass=14.5f;physicsState.body.position[0]=189.25f;physicsState.body.velocity[2]=-18.0f;physicsState.body.angVelocity[1]=1.25f;
 for(int i=0;i<3;++i)physicsState.body.rotation[i][i]=1;
 physicsState.body.centerOfMassOffset[1]=2;physicsState.gravity[2]=-800;physicsState.flags=0x21;physicsState.trajectoryDuration=135;physicsState.stableTime=13.25f;
 std::vector<std::uint8_t> physicsBytes;FieldWriter pw(physicsBytes,refs);EntityPhysicsFields(pw,physicsState);
 FieldReader pr(physicsBytes,refs);EntityPhysicsFields(pr,loadedPhysics);pr.Finish();
 assert(loadedPhysics.body.id==731&&loadedPhysics.body.mass==14.5f&&loadedPhysics.body.velocity[2]==-18&&loadedPhysics.gravity[2]==-800&&loadedPhysics.body.angVelocity[1]==1.25f&&loadedPhysics.trajectoryDuration==135&&loadedPhysics.flags==0x21);
 std::vector<std::uint8_t> physicsAgain;FieldWriter pa(physicsAgain,refs);EntityPhysicsFields(pa,loadedPhysics);assert(physicsAgain==physicsBytes);
 for(std::size_t n=0;n<physicsBytes.size();++n){auto truncated=physicsBytes;truncated.resize(n);rejected=false;try{EntityPhysicsState<TestBodyState> value;FieldReader reader(truncated,refs);EntityPhysicsFields(reader,value);reader.Finish();}catch(const std::runtime_error&){rejected=true;}assert(rejected);}
 for(unsigned v=0;v<8;++v){auto bad=physicsState;if(v==0)bad.body.mass=0;if(v==1)bad.body.velocity[0]=std::numeric_limits<float>::infinity();if(v==2)bad.body.angVelocity[1]=1000;if(v==3)bad.body.id=1024;if(v==4)bad.references=2;if(v==5)bad.constraints=1;if(v==6)bad.vehicle=true;if(v==7)bad.gravity[0]=std::numeric_limits<float>::quiet_NaN();
  rejected=false;try{ValidateEntityPhysics(bad);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 }
 Resources physicsRefs;int token=0,newBody=0,wrongToken=0;physicsRefs.Declare({ResourceKind::Physics,ResourceKind::Entity});physicsRefs.Bind(1,ResourceKind::Physics,&token);physicsRefs.Bind(2,ResourceKind::Entity,&target);
 std::vector<std::uint8_t> bodyReference;FieldWriter bodyWriter(bodyReference,physicsRefs);bodyWriter.Opaque("body",reinterpret_cast<std::intptr_t>(&token));
 physicsRefs.BindRebuiltPhysics(1,&token,&newBody);std::intptr_t bodyHandle=0;FieldReader bodyReader(bodyReference,physicsRefs);bodyReader.Opaque("body",bodyHandle);bodyReader.Finish();assert(bodyHandle==reinterpret_cast<std::intptr_t>(&newBody));
 std::vector<std::uint8_t> rebound;FieldWriter reboundWriter(rebound,physicsRefs);reboundWriter.Opaque("body",bodyHandle);assert(rebound==bodyReference);
 for(unsigned v=0;v<4;++v){rejected=false;try{physicsRefs.BindRebuiltPhysics(v==0?0:v==1?2:1,v==2?&wrongToken:&newBody,v==3?nullptr:&token);}catch(const std::runtime_error&){rejected=true;}assert(rejected);}
 std::ofstream physicsFile(argv[1],std::ios::binary|std::ios::app);physicsFile.write((const char*)physicsBytes.data(),physicsBytes.size());physicsFile.close();
 SourceIdentity first,second,changed;std::string textA="damage = 10;",textB=textA;
 first.Record("maps/test.gsc",textA.data(),static_cast<int>(textA.size()));second.Record("maps/test.gsc",textB.data(),static_cast<int>(textB.size()));
 assert(first.Finish(19,512,4)==second.Finish(19,512,4));
 textB="damage = 20;";changed.Record("maps/test.gsc",textB.data(),static_cast<int>(textB.size()));assert(first.Finish(19,512,4)!=changed.Finish(19,512,4));
 assert(first.Finish(19,512,4)!=first.Finish(19,512,8));assert(first.Finish(19,512,4)!=first.Finish(19,513,4));
 assert(MapIdentity("vorkuta",12)!=MapIdentity("vorkuta",13));assert(MapIdentity("vorkuta",12)!=MapIdentity("rebirth",12));
 second.Record("optional.gsc",nullptr,-1);assert(first.Count()==second.Count());second.Reset();assert(second.Count()==0);
 // Persist owner descriptors, then bind the same IDs to a different address space.
 std::vector<ResourceDescriptor> catalog={{ResourceKind::Entity,ResourceOwner::Entity,3,0},{ResourceKind::AnimationScript,ResourceOwner::GlobalAnimation,24,1}};
 std::vector<std::uint8_t> catalogBytes;FieldWriter cw(catalogBytes,refs);ResourceCatalogFields(cw,catalog);
 std::vector<ResourceDescriptor> parsedCatalog;FieldReader cr(catalogBytes,refs);ResourceCatalogFields(cr,parsedCatalog);cr.Finish();assert(parsedCatalog[1].auxiliary==1&&parsedCatalog[0].slot==3);
 Resources relocatedRefs;relocatedRefs.Declare({ResourceKind::Entity,ResourceKind::AnimationScript});int otherEntity=19,otherScript=27;
 relocatedRefs.Bind(1,ResourceKind::Entity,&otherEntity);relocatedRefs.Bind(2,ResourceKind::AnimationScript,&otherScript);
 Resources sourceRefs;sourceRefs.Intern(ResourceKind::Entity,&target);std::vector<std::uint8_t> sourceReference;FieldWriter refWriter(sourceReference,sourceRefs);refWriter.Ref("owner",&target,ResourceKind::Entity);
 FieldReader relocatedReader(sourceReference,relocatedRefs);int* relocatedOwner=nullptr;relocatedReader.Ref("owner",relocatedOwner,ResourceKind::Entity);assert(relocatedOwner==&otherEntity&&relocatedOwner!=&target);
 rejected=false;try{relocatedRefs.Bind(1,ResourceKind::Entity,&otherScript);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 Resources unbound;unbound.Declare({ResourceKind::Entity});rejected=false;try{unbound.Resolve(ResourceKind::Entity,1);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 for(unsigned variant=0;variant<3;++variant){auto bad=catalog[0];if(variant==0)bad.slot=1024;if(variant==1)bad.kind=ResourceKind::Actor;if(variant==2)bad.owner=static_cast<ResourceOwner>(999);
  rejected=false;try{ValidateResourceDescriptor(bad);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 }
 auto words=[](std::initializer_list<unsigned> values){std::vector<std::uint8_t> out;for(auto word:values)for(int i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(word>>(8*i)));return out;};
 auto pool=words({3,1,0,0,0,1,3,8,0x11223344,0x55667788});auto parsedPool=ParseNativeRecords(SectionId::Entities,pool);assert(parsedPool.size()==1&&parsedPool[0].resource==1&&parsedPool[0].slot==3&&parsedPool[0].bytes.size()==8);
 for(std::size_t n=0;n<pool.size();++n){auto truncated=pool;truncated.resize(n);rejected=false;try{ParseNativeRecords(SectionId::Entities,truncated);}catch(const std::runtime_error&){rejected=true;}assert(rejected);}
 auto badPool=words({3,2,0,0,0,1,3,0,2,3,0});rejected=false;try{ParseNativeRecords(SectionId::Entities,badPool);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 auto world=words({1,0,0,0,0,8,0,0,0});assert(ParseNativeRecords(SectionId::World,world).size()==2);
 world.resize(20);rejected=false;try{ParseNativeRecords(SectionId::World,world);}catch(const std::runtime_error&){rejected=true;}assert(rejected);
 std::ofstream catalogFile(argv[1],std::ios::binary|std::ios::app);catalogFile.write((const char*)catalogBytes.data(),catalogBytes.size());catalogFile.close();
 printf("PASS: %u-bit typed records, owned text, resource aliases/kinds, inert handles, truncation and schema (%zu bytes)\n",unsigned(sizeof(void*)*8),bytes.size());
}
