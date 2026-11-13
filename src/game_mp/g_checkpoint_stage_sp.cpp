#include "g_checkpoint_stage_sp.h"
#ifdef KISAK_SP
#include "g_checkpoint_records_sp.h"
#include "g_checkpoint_world_sp.h"
#include "checkpoint_animation.h"
#include "checkpoint_string_bindings_sp.h"
#include "g_main_mp.h"
#include <game/actor.h>
#include <game/sentient.h>
#include <xanim/xanim.h>
#include <memory>
#include <algorithm>
#include <vehicle/nitrous_vehicle.h>
#include "g_utils_mp.h"
#include "g_anim_commands_sp.h"
#include <game_mp/actor_mp.h>
#include <game/actor_badplace.h>
#include <game/g_hudelem.h>
#include <game/g_scr_vehicle.h>
#include <game/turret.h>
#include <game/pathnode.h>
#include <game/g_targets.h>
#include <clientscript/cscr_save.h>
#include <clientscript/cscr_save_write.h>
#include <clientscript/cscr_stringlist.h>
#include <clientscript/cscr_memorytree.h>
#include <clientscript/cscr_animtree.h>
#include <clientscript/cscr_vm.h>
#include <clientscript/cscr_checkpoint_identity.h>
#include <qcommon/common.h>
#include <qcommon/cm_load.h>
#include <qcommon/cm_world.h>
#include <server/sv_world.h>
#include <server/sv_game.h>
#include <server_mp/sv_main_mp.h>
#include <client_mp/sv_client_mp.h>
#include <xanim/dobj.h>
#include <xanim/dobj_utils.h>
#include <qcommon/dobj_management.h>
#include <universal/memfile.h>
#include <universal/dvar.h>
namespace {
const checkpoint::Section& FindSection(const checkpoint::Image& image,checkpoint::SectionId id){for(const auto& section:image.sections)if(section.id==id)return section;throw std::runtime_error("checkpoint native section missing");}
struct Stage {
    checkpoint::Resources refs;
    std::unique_ptr<gentity_s[]> entities=std::make_unique<gentity_s[]>(MAX_GENTITIES);
    std::unique_ptr<gclient_s[]> clients=std::make_unique<gclient_s[]>(32);
    std::unique_ptr<actor_s[]> actors=std::make_unique<actor_s[]>(MAX_ACTORS);
    std::unique_ptr<sentient_s[]> sentients=std::make_unique<sentient_s[]>(MAX_SENTIENTS);
    std::unique_ptr<AnimScriptList[]> animationScripts=std::make_unique<AnimScriptList[]>(4);
    std::vector<std::unique_ptr<tagInfo_s>> tags;
    std::vector<std::unique_ptr<ScriptedAnimRecord_SP>> scripted;
    std::vector<std::unique_ptr<XAnimTree_s>> trees;
    std::vector<std::unique_ptr<unsigned>> opaque;
    std::unique_ptr<CheckpointWorldStage_SP> world;
    std::vector<checkpoint::AnimationTree> animationStates;
};
}
namespace {
void ValidateAnimationAsset_SP(const checkpoint::AnimationTree& tree){
    if(!tree.present)return;
    if(!tree.published||tree.published>=MAX_XANIMTREE_NUM||tree.publishedUser>1)throw std::runtime_error("checkpoint animation asset is not published");
    const auto* asset=gScrAnimPub[SCRIPTINSTANCE_SERVER].xanim_lookup[tree.publishedUser][tree.published].anims;
    if(!asset||asset->size!=tree.assetSize)throw std::runtime_error("checkpoint animation asset changed");
    checkpoint::ValidateAnimationAssetTopology(tree,[&](unsigned index){return asset->entries[index].parent;},[&](unsigned index){return IsLeafNode(&asset->entries[index]);});
}
std::unique_ptr<Stage> BuildStage(const checkpoint::Image& image,const checkpoint::Resources& source,unsigned& relocated){
    relocated=0;
        using namespace checkpoint;
        auto worldRecords=ParseNativeRecords(SectionId::World,FindSection(image,SectionId::World).bytes);
        std::vector<ResourceDescriptor> catalog;Resources metadata;
        for(const auto& record:worldRecords)if(record.group==8){FieldReader reader(record.bytes,metadata);ResourceCatalogFields(reader,catalog);reader.Finish();}
        if(catalog.size()!=source.entries.size())throw std::runtime_error("checkpoint resource catalog/source count mismatch");
        unsigned nodes=0;for(const auto& desc:catalog)if(desc.owner==ResourceOwner::PathNode)nodes=(std::max)(nodes,desc.slot+1);
        auto stage=std::make_unique<Stage>();stage->world=std::make_unique<CheckpointWorldStage_SP>(nodes);
        std::vector<ResourceKind> kinds;for(const auto& desc:catalog)kinds.push_back(desc.kind);stage->refs.Declare(kinds);
        stage->refs.stringName=source.stringName;stage->refs.findString=CheckpointTemporaryStrings_SP(SCRIPTINSTANCE_SERVER);stage->refs.scriptReferences=source.scriptReferences;
        for(unsigned i=0;i<catalog.size();++i){const auto& desc=catalog[i];void* pointer=nullptr;
            switch(desc.owner){
            case ResourceOwner::Entity:pointer=&stage->entities[desc.slot];break;
            case ResourceOwner::Client:pointer=&stage->clients[desc.slot];break;
            case ResourceOwner::Actor:pointer=&stage->actors[desc.slot];break;
            case ResourceOwner::Sentient:pointer=&stage->sentients[desc.slot];break;
            case ResourceOwner::SpecificAnimation:pointer=&stage->actors[desc.slot].AnimScriptSpecific;break;
            case ResourceOwner::GlobalAnimation:pointer=reinterpret_cast<unsigned char*>(&stage->animationScripts[desc.auxiliary])+desc.slot*sizeof(scr_animscript_t);break;
            case ResourceOwner::TagInfo:stage->tags.push_back(std::make_unique<tagInfo_s>());pointer=stage->tags.back().get();break;
            case ResourceOwner::ScriptedAnimation:stage->scripted.push_back(std::make_unique<ScriptedAnimRecord_SP>());pointer=stage->scripted.back().get();break;
            case ResourceOwner::AnimationTree:stage->trees.push_back(std::make_unique<XAnimTree_s>());pointer=stage->trees.back().get();break;
            case ResourceOwner::PhysicsEntity:case ResourceOwner::NitrousVehicle:stage->opaque.push_back(std::make_unique<unsigned>());pointer=stage->opaque.back().get();break;
            default:pointer=stage->world->Pool(desc.owner,desc.slot);break;
            }
            if(pointer==source.entries[i].pointer)throw std::runtime_error("checkpoint staging reused a live owner address");stage->refs.Bind(i+1,desc.kind,pointer);++relocated;
        }
        auto& nativeLevel=stage->world->Level();nativeLevel.gentities=stage->entities.get();nativeLevel.clients=stage->clients.get();nativeLevel.actors=stage->actors.get();nativeLevel.sentients=stage->sentients.get();
        stage->world->Read(worldRecords,stage->refs);
        std::array<unsigned,4096> animationOwners{};
        for(const auto& record:ParseNativeRecords(SectionId::Entities,FindSection(image,SectionId::Entities).bytes)){
            if(record.group==0){if(stage->refs.Resolve(ResourceKind::Entity,record.resource)!=&stage->entities[record.slot])throw std::runtime_error("entity record/catalog owner mismatch");G_CheckpointReadEntity_SP(stage->entities[record.slot],stage->refs,record.bytes);}
            if(record.group==1)G_CheckpointReadTag_SP(*static_cast<tagInfo_s*>(const_cast<void*>(stage->refs.Resolve(ResourceKind::TagInfo,record.resource))),stage->refs,record.bytes);
            if(record.group==2)G_CheckpointReadScriptedAnimation_SP(*static_cast<ScriptedAnimRecord_SP*>(const_cast<void*>(stage->refs.Resolve(ResourceKind::ScriptedAnimation,record.resource))),stage->refs,record.bytes);
            if(record.group==3){AnimationTree tree;FieldReader reader(record.bytes,stage->refs);AnimationFields(reader,tree);reader.Finish();ValidateAnimationAsset_SP(tree);
                for(const auto& node:tree.nodes){auto& owner=animationOwners[node.savedInfo];if(owner&&owner!=record.resource)throw std::runtime_error("checkpoint animation instance belongs to multiple trees");owner=record.resource;}
                stage->animationStates.push_back(std::move(tree));}
        }
        for(const auto& record:ParseNativeRecords(SectionId::Players,FindSection(image,SectionId::Players).bytes)){
            if(stage->refs.Resolve(ResourceKind::Client,record.resource)!=&stage->clients[record.slot])throw std::runtime_error("player record/catalog owner mismatch");G_CheckpointReadPlayer_SP(stage->clients[record.slot],stage->refs,record.bytes);
        }
        for(const auto& record:ParseNativeRecords(SectionId::Actors,FindSection(image,SectionId::Actors).bytes)){
            if(record.group==2){if(stage->refs.Resolve(ResourceKind::Actor,record.resource)!=&stage->actors[record.slot])throw std::runtime_error("actor record/catalog owner mismatch");G_CheckpointReadActor_SP(stage->actors[record.slot],stage->refs,record.bytes);}
            else{if(stage->refs.Resolve(ResourceKind::Sentient,record.resource)!=&stage->sentients[record.slot])throw std::runtime_error("sentient record/catalog owner mismatch");G_CheckpointReadSentient_SP(stage->sentients[record.slot],stage->refs,record.bytes);}
        }
        for(unsigned i=0;i<static_cast<unsigned>(nativeLevel.num_entities);++i){const auto& entity=stage->entities[i];if(!entity.r.inuse)continue;
            if(entity.s.number!=static_cast<int>(i))throw std::runtime_error("staged entity number/pool mismatch");
            if(entity.actor&&entity.actor->ent!=&entity)throw std::runtime_error("staged actor/entity ownership mismatch");
            if(entity.sentient&&entity.sentient->ent!=&entity)throw std::runtime_error("staged sentient/entity ownership mismatch");
        }
        return stage;
}
std::unique_ptr<checkpoint::Resources> liveReferences;
void RestoreDebugRootCounts(){
    auto* debug=gScrVarDebugPub[SCRIPTINSTANCE_SERVER];if(!debug)return;const auto& pub=gScrVarPub[SCRIPTINSTANCE_SERVER];
    for(auto id:{pub.levelId,pub.animId,pub.timeArrayId,pub.pauseArrayId})if(id)++debug->extRefCount[id];
    for(unsigned i=0;i<CLASS_NUM_COUNT;++i){const auto& map=gScrClassMap[SCRIPTINSTANCE_SERVER][i];if(map.id)++debug->extRefCount[map.id];if(map.entArrayId)++debug->extRefCount[map.entArrayId];}
}
}
bool G_CheckpointValidateStaging_SP(const checkpoint::Image& image,const checkpoint::Resources& source,unsigned& relocated,std::string& error){
    try{return BuildStage(image,source,relocated)!=nullptr;}catch(const std::exception& e){error=e.what();return false;}
}
bool G_CheckpointCanApplyServer_SP(const checkpoint::Image& image,const checkpoint::Resources& source,std::string& error){
    try{
        unsigned relocated=0;auto stage=BuildStage(image,source,relocated);
        if(level.maxclients!=1)throw std::runtime_error("checkpoint restore requires solo campaign");
        std::vector<checkpoint::ResourceDescriptor> catalog;checkpoint::Resources metadata;
        for(const auto& record:checkpoint::ParseNativeRecords(checkpoint::SectionId::World,FindSection(image,checkpoint::SectionId::World).bytes))if(record.group==8){checkpoint::FieldReader reader(record.bytes,metadata);checkpoint::ResourceCatalogFields(reader,catalog);reader.Finish();}
        const auto world=checkpoint::ParseNativeRecords(checkpoint::SectionId::World,FindSection(image,checkpoint::SectionId::World).bytes);
        G_CheckpointValidatePhysics_SP(world,catalog,stage->entities.get(),stage->refs);
        const auto records=checkpoint::ParseNativeRecords(checkpoint::SectionId::Entities,FindSection(image,checkpoint::SectionId::Entities).bytes);
        for(const auto& desc:catalog)if(desc.owner==checkpoint::ResourceOwner::AnimationTree){const auto& entity=stage->entities[desc.slot];
            if(entity.pAnimTree){bool found=false;for(const auto& record:records)if(record.group==3&&record.slot==desc.slot){checkpoint::AnimationTree state;checkpoint::FieldReader reader(record.bytes,metadata);checkpoint::AnimationFields(reader,state);reader.Finish();
                if(!state.published||state.published>=MAX_XANIMTREE_NUM||state.publishedUser>1)throw std::runtime_error("animation asset is not published");auto* asset=gScrAnimPub[SCRIPTINSTANCE_SERVER].xanim_lookup[state.publishedUser][state.published].anims;if(!asset||asset->size!=state.assetSize)throw std::runtime_error("animation asset changed");found=true;break;}if(!found)throw std::runtime_error("owned animation state missing");}
            else if(!entity.actor&&!entity.client&&!(entity.s.eType==ET_ACTOR_CORPSE&&desc.auxiliary&&desc.auxiliary<=MAX_ACTOR_CORPSES_SP))throw std::runtime_error("unsupported animation tree owner");
        }
        return true;
    }catch(const std::exception& e){error=e.what();return false;}
}
void G_CheckpointClearLiveReferences_SP(){liveReferences.reset();}
bool G_CheckpointApplyServer_SP(const checkpoint::Image& image,const checkpoint::Resources& source,std::string& error){
    try{
        using namespace checkpoint;unsigned relocated=0;auto stage=BuildStage(image,source,relocated);
        if(level.maxclients!=1)throw std::runtime_error("checkpoint restore currently requires a solo campaign");
        const auto worldRecords=ParseNativeRecords(SectionId::World,FindSection(image,SectionId::World).bytes);
        const auto entities=ParseNativeRecords(SectionId::Entities,FindSection(image,SectionId::Entities).bytes);
        const auto players=ParseNativeRecords(SectionId::Players,FindSection(image,SectionId::Players).bytes);
        const auto ai=ParseNativeRecords(SectionId::Actors,FindSection(image,SectionId::Actors).bytes);
        std::vector<ResourceDescriptor> catalog;Resources metadata;
        for(const auto& record:worldRecords)if(record.group==8){FieldReader reader(record.bytes,metadata);ResourceCatalogFields(reader,catalog);reader.Finish();}
        G_CheckpointValidatePhysics_SP(worldRecords,catalog,stage->entities.get(),stage->refs);
        std::vector<AnimationTree> treeStates(entities.size());std::vector<const NativeRecord*> treeRecords;
        for(unsigned i=0;i<entities.size();++i)if(entities[i].group==3){FieldReader reader(entities[i].bytes,metadata);AnimationFields(reader,treeStates[i]);reader.Finish();treeRecords.push_back(&entities[i]);}
        Com_Printf(15,"CHECKPOINT_RESTORE_PHASE retiring_native\n");
        level.initializing=1;bgs=&level_bgs;G_FreeEntities(true);G_SPShutdownSoundNotifyState();HudElem_DestroyAll();Path_ShutdownBadPlaces();for(int i=0;i<level.maxclients;++i)if(svs.clients[i].scriptId)SV_FreeClientScriptId(&svs.clients[i]);
        for(unsigned i=0;i<g_path.actualNodeCount;++i)Scr_FreeEntityNum(i,2,SCRIPTINSTANCE_SERVER);
        Scr_ShutdownSystem(SCRIPTINSTANCE_SERVER,1,1);
        for(unsigned i=0;i<CLASS_NUM_COUNT;++i)Scr_RemoveClassMap(SCRIPTINSTANCE_SERVER,i);
        EntHandle::Init();SentientHandle::Init();Actor_ShutdownThreatBiasGroups();G_InitActors();G_InitSentients();G_InitTurrets();G_InitDestructibles();G_InitAnimCommands_SP();
        Com_Printf(15,"CHECKPOINT_RESTORE_PHASE loading_vm\n");
        auto script=FindSection(image,SectionId::Script).bytes;auto memory=std::make_unique<MemoryFile>();MemFile_InitForReading(memory.get(),static_cast<int>(script.size()),script.data(),false);
        Scr_LoadVariables(SCRIPTINSTANCE_SERVER,memory.get());RestoreDebugRootCounts();
        auto refs=std::make_unique<Resources>();std::vector<ResourceKind> kinds;for(const auto& desc:catalog)kinds.push_back(desc.kind);refs->Declare(kinds);
        refs->stringName=[](std::uint16_t id){return SL_ConvertToString(id,SCRIPTINSTANCE_SERVER);};
        refs->findString=[](const char* name){return static_cast<std::uint16_t>(SL_GetString_(SCRIPTINSTANCE_SERVER,name,0,15));};
        refs->scriptBinder=[](const char* name,std::uint16_t id){auto native=Scr_ReadSaveId(id,SCRIPTINSTANCE_SERVER);
            if(native&&!std::strcmp(name,"animation_thread")){++gScrVarPub[SCRIPTINSTANCE_SERVER].ext_threadcount;if(gScrVarDebugPub[SCRIPTINSTANCE_SERVER])++gScrVarDebugPub[SCRIPTINSTANCE_SERVER]->extRefCount[native];}return native;};
        for(const auto& ref:source.scriptReferences){const auto native=gScrVarPub[SCRIPTINSTANCE_SERVER].saveIdMapRev[ref.archiveId];if(native)refs->BindScript(native,ref.archiveId);}
        const AnimScriptList* lists[]={&g_scr_data.dogAnim,&g_scr_data.humanAnim,&g_scr_data.zombieAnim,&g_scr_data.zombieDogAnim};
        for(unsigned i=0;i<catalog.size();++i){const auto& desc=catalog[i];void* pointer=nullptr;
            switch(desc.owner){
            // Physics bindings remain inert tokens while entity/model records
            // are decoded. No staging token is exposed as a live physics handle.
            case ResourceOwner::PhysicsEntity:pointer=const_cast<void*>(stage->refs.Resolve(ResourceKind::Physics,i+1));break;
            case ResourceOwner::Entity:pointer=&g_entities[desc.slot];break;
            case ResourceOwner::Client:pointer=&level.clients[desc.slot];break;
            case ResourceOwner::Actor:pointer=&level.actors[desc.slot];break;
            case ResourceOwner::Sentient:pointer=&level.sentients[desc.slot];break;
            case ResourceOwner::NitrousVehicle:pointer=NitrousVehicle::add_vehicle(static_cast<scr_vehicle_s*>(stage->world->Pool(ResourceOwner::Vehicle,desc.slot))->entNum);break;
            case ResourceOwner::SpecificAnimation:pointer=&level.actors[desc.slot].AnimScriptSpecific;break;
            case ResourceOwner::GlobalAnimation:pointer=const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(lists[desc.auxiliary]))+desc.slot*sizeof(scr_animscript_t);break;
            case ResourceOwner::TagInfo:pointer=MT_Alloc(sizeof(tagInfo_s),17,SCRIPTINSTANCE_SERVER);std::memset(pointer,0,sizeof(tagInfo_s));break;
            case ResourceOwner::ScriptedAnimation:pointer=MT_Alloc(sizeof(ScriptedAnimRecord_SP),18,SCRIPTINSTANCE_SERVER);std::memset(pointer,0,sizeof(ScriptedAnimRecord_SP));break;
            case ResourceOwner::AnimationTree:{const auto& entity=stage->entities[desc.slot];
                if(entity.pAnimTree){const AnimationTree* state=nullptr;for(unsigned j=0;j<entities.size();++j)if(entities[j].group==3&&entities[j].resource==i+1){state=&treeStates[j];break;}
                    if(!state||!state->published||state->published>=MAX_XANIMTREE_NUM||state->publishedUser>1)throw std::runtime_error("checkpoint animation asset missing for entity "+std::to_string(desc.slot));
                    auto* asset=gScrAnimPub[SCRIPTINSTANCE_SERVER].xanim_lookup[state->publishedUser][state->published].anims;if(!asset||asset->size!=state->assetSize)throw std::runtime_error("checkpoint animation asset unavailable");pointer=Com_XAnimCreateSmallTree(asset);
                }else if(entity.actor)pointer=g_scr_data.actorXAnimTrees[static_cast<unsigned>(entity.actor-stage->actors.get())];
                else if(entity.client)pointer=level_bgs.clientinfo[desc.slot].pXAnimTree;
                else if(entity.s.eType==ET_ACTOR_CORPSE&&desc.auxiliary&&desc.auxiliary<=MAX_ACTOR_CORPSES_SP)pointer=g_scr_data.actorCorpseInfo[desc.auxiliary-1].tree;
                else throw std::runtime_error("checkpoint animation owner is not reconstructible for entity "+std::to_string(desc.slot));
                if(!pointer)throw std::runtime_error("checkpoint animation tree owner unavailable");break;}
            default:pointer=G_CheckpointLiveWorldPool_SP(desc.owner,desc.slot);break;
            }
            refs->Bind(i+1,desc.kind,pointer);
        }
        Com_Printf(15,"CHECKPOINT_RESTORE_PHASE loading_native\n");
        for(const auto& record:entities){
            if(record.group==0)G_CheckpointReadEntity_SP(g_entities[record.slot],*refs,record.bytes);
            if(record.group==1)G_CheckpointReadTag_SP(*static_cast<tagInfo_s*>(const_cast<void*>(refs->Resolve(ResourceKind::TagInfo,record.resource))),*refs,record.bytes);
            if(record.group==2)G_CheckpointReadScriptedAnimation_SP(*static_cast<ScriptedAnimRecord_SP*>(const_cast<void*>(refs->Resolve(ResourceKind::ScriptedAnimation,record.resource))),*refs,record.bytes);
        }
        for(const auto& desc:catalog)if(desc.owner==ResourceOwner::PhysicsEntity)g_entities[desc.slot].physObjId=0;
        for(const auto& record:players)G_CheckpointReadPlayer_SP(level.clients[record.slot],*refs,record.bytes);
        for(const auto& record:ai)if(record.group==2){G_CheckpointReadActor_SP(level.actors[record.slot],*refs,record.bytes);level.actors[record.slot].Physics.proximity_data.reset();level.actors[record.slot].Physics.m_gjkcc_input=nullptr;}else G_CheckpointReadSentient_SP(level.sentients[record.slot],*refs,record.bytes);
        G_CheckpointApplyWorld_SP(worldRecords,*refs);
        for(const auto& record:entities)if(record.group==0&&g_entities[record.slot].r.inuse){G_CheckpointBindEntityHandles_SP(g_entities[record.slot]);if(g_entities[record.slot].client)G_CheckpointBindPlayerHandles_SP(*g_entities[record.slot].client);}
        for(const auto& record:ai)if(record.group==2)G_CheckpointBindActorHandles_SP(level.actors[record.slot]);else G_CheckpointBindSentientHandles_SP(level.sentients[record.slot]);
        G_CheckpointBindWorldHandles_SP();
        for(unsigned i=0;i<static_cast<unsigned>(level.maxclients);++i){svs.clients[i].scriptId=level.clients[i].sess.scriptPersId;}
        Scr_PostLoadVariables(SCRIPTINSTANCE_SERVER);
        Com_Printf(15,"CHECKPOINT_RESTORE_PHASE rebuilding_models\n");
        for(const auto& record:entities)if(record.group==0&&g_entities[record.slot].r.inuse)G_DObjUpdate(&g_entities[record.slot]);
        std::vector<unsigned short> infoRemap(4096);std::vector<XAnimTree_s*> rebuiltTrees(catalog.size()+1,nullptr);
        for(unsigned i=0;i<entities.size();++i)if(entities[i].group==3){const auto& record=entities[i];const auto& tree=treeStates[i];DObj* object=Com_GetServerDObj(record.slot);if(!object)continue;
            if(tree.present){if(!object->tree||object->tree->anims->size!=tree.assetSize)throw std::runtime_error("restored DObj animation asset mismatch");
                if(!record.resource||record.resource>=rebuiltTrees.size())throw std::runtime_error("restored animation tree resource missing");
                if(rebuiltTrees[record.resource]){if(rebuiltTrees[record.resource]!=object->tree)throw std::runtime_error("restored animation tree alias mismatch");}
                else{
                    XAnimClearTree(object->tree);object->tree->inst=tree.instance;
                    XModelNameMap modelMap[512];XAnimInitModelMap(object->localModels,object->numModels,modelMap);
                    unsigned short firstRoot=0,previousRoot=0;
                    RebuildAnimationInstances(tree,[&](const AnimationNode& node,unsigned parent){
                        const auto& entry=object->tree->anims->entries[node.animation];
                        const auto modelMapId=IsLeafNode(&entry)?static_cast<unsigned short>(XAnimGetAnimMap(entry.parts,modelMap)):0;
                        const auto infoIndex=XAnimAllocInfoWithParent(object->tree,modelMapId,node.animation,parent,1);auto& info=*GetAnimInfo(infoIndex);XAnimInitInfo(&info);
                        if(!node.parent){
                            // The native allocator assumes one root. Link restarted
                            // roots locally, keeping the global pool sentinel intact.
                            info.prev=previousRoot;info.next=0;
                            if(previousRoot)GetAnimInfo(previousRoot)->next=static_cast<unsigned short>(infoIndex);else firstRoot=static_cast<unsigned short>(infoIndex);
                            object->tree->children=firstRoot;previousRoot=static_cast<unsigned short>(infoIndex);
                        }
                        if(infoRemap[node.savedInfo])throw std::runtime_error("saved animation instance was rebuilt twice");infoRemap[node.savedInfo]=static_cast<unsigned short>(infoIndex);
                        info.notifyChild=node.notifyChild;info.notifyType=node.notifyType;info.notifyIndex=node.notifyIndex;
                        info.notifyName=node.notifyName?static_cast<unsigned short>(SL_GetString_(static_cast<scriptInstance_t>(tree.instance),node.notifyName,0,15)):0;
                        info.state.currentAnimTime=node.time;info.state.oldTime=node.oldTime;info.state.cycleCount=node.cycle;info.state.oldCycleCount=node.oldCycle;info.state.goalTime=node.goalTime;info.state.goalWeight=node.goalWeight;info.state.weight=node.weight;info.state.rate=node.rate;info.state.instantWeightChange=node.instant;
                        return infoIndex;
                    });
                    rebuiltTrees[record.resource]=object->tree;
                }
            }
            DObjSetHidePartBits(object,tree.hiddenParts);
        }
        for(const auto& record:worldRecords)if(record.group==11)G_CheckpointApplyAnimCommands_SP(record.bytes,infoRemap,true);
        G_CheckpointRebuildPhysics_SP(worldRecords,catalog,*refs);
        G_CheckpointRebuildVehicles_SP(worldRecords,*refs);
        CM_LinkWorld();for(const auto& record:entities)if(record.group==0){auto& entity=g_entities[record.slot];if(entity.r.inuse&&entity.r.linked){entity.r.linked=false;SV_LinkEntity(&entity);}}
        SV_LocateGameData(level.gentities,level.num_entities,sizeof(gentity_s),&level.clients[0].ps,sizeof(gclient_s),level.actors,sizeof(actor_s),G_GetAnimCommandsBase_SP(),sizeof(AnimCommand_SP));
        GScr_ResetChangeLevel_SP();level.initializing=0;level.loading=0;bgs=nullptr;liveReferences=std::move(refs);
        svs.time=level.time;svs.snapFlagServerBit^=4;svs.nextCachedSnapshotEntities=svs.nextCachedSnapshotClients=svs.nextCachedSnapshotFrames=0;SV_InitSnapshot();
        for(int i=0;i<level.maxclients;++i){auto& client=svs.clients[i];client.header.deltaMessage=-1;client.nextSnapshotTime=svs.time;client.lastSnapshotTime=-1;client.acknowledgedAnimCommandTime=-1;client.lastUsercmd.serverTime=0;}
        Com_Printf(15,"CHECKPOINT_RESTORE_PHASE server_ready time=%i health=%i\n",level.time,g_entities[0].health);return true;
    }catch(const std::exception& e){bgs=nullptr;error=e.what();return false;}
}
#endif
