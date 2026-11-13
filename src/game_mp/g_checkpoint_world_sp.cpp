#include "g_checkpoint_world_sp.h"
#ifdef KISAK_SP
#include "g_main_mp.h"
#include "g_utils_mp.h"
#include "g_anim_commands_sp.h"
#include "checkpoint_resource_catalog.h"
#include "checkpoint_destructible_sp.h"
#include "checkpoint_physics.h"
#include <physics/phys_colgeom.h>
#include <qcommon/cm_load.h>
#include <clientscript/scr_const.h>
#include "checkpoint_handles_sp.h"
#include <universal/com_memory.h>
#include <universal/mem_userhunk.h>
#include <database/db_registry.h>
#include <clientscript/cscr_stringlist.h>
#include "checkpoint_record_set.h"
#include <qcommon/dobj_management.h>
#include <xanim/dobj.h>
#include <game/actor.h>
#include <game/sentient.h>
#include <game/g_scr_vehicle.h>
#include <game/turret.h>
#include <game/g_hudelem.h>
#include <game/pathnode.h>
#include <cgame/cg_hudelem.h>
#include <physics/phys_main.h>
#include <physics/destructible.h>
#include <memory>
#include <vehicle/nitrous_vehicle.h>
#include <universal/com_math.h>
#include <algorithm>
#include <game/actor_badplace.h>
#include <game/actor_threat.h>
#include <game/actor_spawner.h>
#include <server_mp/sv_main_mp.h>
#include <client_mp/sv_client_mp.h>
#include <server/sv_game.h>
#include <clientscript/cscr_stringlist.h>
#include <qcommon/threads.h>
extern Destructible s_destructibles[G_MAX_DESTRUCTIBLES_SP];
extern unsigned s_numDestructibles;
extern HunkUser* g_pieceArrayHunk;
extern unsigned holdrand;
extern TIMED_RADIUS_DAMAGE g_timed_radius_damage[512];
extern vehicle_custom_path_t gCustomPaths[10];
namespace {
#include "checkpoint_world_fields.inc"
#include "checkpoint_nitrous_fields.inc"
template<class V>void WorldFields(V& v,objective_t& x){
    v.Field("objective.state",x.state);v.Field("objective.origin",x.origin);v.Field("objective.size",x.size);
    v.Field("objective.entity",x.entNum);v.Field("objective.team",x.teamNum);v.Field("objective.icon",x.icon);
    v.Field("objective.use3d",x.use3D);v.Field("objective.color",x.color);v.Text("objective.text",x.alt_3D_text);
}
template<class V>void WorldFields(V& v,trigger_info_t& x){v.Field("entity",x.entnum);v.Field("other",x.otherEntnum);v.Field("uses",x.useCount);v.Field("other_uses",x.otherUseCount);}
template<class V>void WorldFields(V& v,game_hudelem_s& x){WorldFields(v,x.elem);v.Field("client",x.clientNum);v.Field("team",x.team);v.Field("archived",x.archived);}
template<class V>void LevelFields(V& v,level_locals_t& x){
    v.Field("entities",x.num_entities);v.Field("actors",x.num_actors);v.Field("actor_corpses",x.actorCorpseCount);v.Field("max_clients",x.maxclients);
    v.Field("frame",x.framenum);v.Field("time",x.time);v.Field("previous_time",x.previousTime);v.Field("frame_time",x.frametime);v.Field("start_time",x.startTime);
    v.Field("save_persist",x.savepersist);v.Field("fog_distance",x.fFogOpaqueDist);v.Field("fog_distance_squared",x.fFogOpaqueDistSqrd);
    v.Field("ignore_radius_damage",x.bPlayerIgnoreRadiusDamage);v.Field("ignore_radius_damage_latched",x.bPlayerIgnoreRadiusDamageLatched);
    v.Field("compass_origin",x.compassMapUpperLeft);v.Field("compass_size",x.compassMapWorldSize);v.Field("compass_north",x.compassNorth);
    v.Field("actor_clone",x.currentActorClone);v.Field("player_clone",x.currentPlayerClone);v.Field("search_frame",x.iSearchFrame);
    v.Field("grenade_hints",x.numGrenadeHints);v.Field("sun",x.mapSunDirection);v.Field("mission_success",x.missionSuccess);v.Field("mission_cinematic",x.missionSuccessCinematic);
    v.Field("current_animation_entity",x.currentAnimEntity);v.Field("animation_pending",x.bAnimScriptPending);v.Field("force_level_end",x.forceLevelEnd_SP);
    for(auto& objective:x.objectives)WorldFields(v,objective);v.Field("objective_masks",x.objectivesClientMask);v.Field("model_map",x.modelMap);
    v.Field("pending_triggers",x.pendingTriggerListSize);v.Field("current_triggers",x.currentTriggerListSize);
    if(x.pendingTriggerListSize<0||x.pendingTriggerListSize>256||x.currentTriggerListSize<0||x.currentTriggerListSize>256)throw std::runtime_error("invalid checkpoint trigger count");
    for(int i=0;i<x.pendingTriggerListSize;++i)WorldFields(v,x.pendingTriggerList[i]);
    for(int i=0;i<x.currentTriggerListSize;++i)WorldFields(v,x.currentTriggerList[i]);
    for(auto& handle:x.droppedWeaponCue)v.Handle("dropped_weapon",handle);
    v.Ref("first_free_entity",x.firstFreeEnt,checkpoint::ResourceKind::Entity);v.Ref("last_free_entity",x.lastFreeEnt,checkpoint::ResourceKind::Entity);
    v.Ref("first_free_actor",x.firstFreeActor,checkpoint::ResourceKind::Entity);v.Ref("last_free_actor",x.lastFreeActor,checkpoint::ResourceKind::Entity);
}
using SavedDestructible=checkpoint::SavedDestructible_SP;
using checkpoint::DestructibleFields;
struct SavedNode {pathnode_t value{};std::vector<pathlink_s> links;};
template<class V>void NodeFields(V& v,SavedNode& x){auto& c=x.value.constant;auto& d=x.value.dynamic;
    v.Field("type",c.type);v.Field("spawn_flags",c.spawnflags);v.StringId("target_name",c.targetname);v.StringId("script_link",c.script_linkName);
    v.StringId("noteworthy",c.script_noteworthy);v.StringId("target",c.target);v.StringId("animation",c.animscript);v.Field("animation_function",c.animscriptfunc);
    v.Field("origin",c.vOrigin);v.Field("angle",c.fAngle);v.Field("forward",c.forward);v.Field("radius",c.fRadius);v.Field("min_distance",c.minUseDistSq);
    v.Field("overlap",c.wOverlapNode);v.Field("chain",c.wChainId);v.Field("depth",c.wChainDepth);v.Field("chain_parent",c.wChainParent);
    v.Handle("owner",d.pOwner);v.Field("free_time",d.iFreeTime);v.Field("valid_time",d.iValidTime);v.Field("player_los_time",d.inPlayerLOSTime);
    v.Field("link_count",d.wLinkCount);v.Field("overlap_count",d.wOverlapCount);v.Field("turret",d.turretEntNumber);v.Field("users",d.userCount);
    unsigned count=static_cast<unsigned>(x.links.size());v.Field("links",count);if(count>65535)throw std::runtime_error("checkpoint node has too many links");x.links.resize(count);
    for(auto& link:x.links){v.Field("distance",link.fDist);v.Field("node",link.nodeNum);v.Field("disconnect_count",link.disconnectCount);v.Field("negotiation",link.negotiationLink);v.Field("bad_places",link.ubBadPlaceCount);}
}
void ReadBodyState(const PhysObjUserData& source,BodyState& state){
    if(!source.body)throw std::runtime_error("checkpoint body has no rigid body");
    for(int i=0;i<3;++i){state.position[i]=source.savedPos[i];state.centerOfMassOffset[i]=source.centerOfMassOffset[i];state.buoyancyBoxMin[i]=source.buoyancyBoxMin[i];state.buoyancyBoxMax[i]=source.buoyancyBoxMax[i];
        for(int j=0;j<3;++j)state.rotation[i][j]=source.savedRot[i][j];}
    state.velocity[0]=source.body->m_t_vel.x;state.velocity[1]=source.body->m_t_vel.y;state.velocity[2]=source.body->m_t_vel.z;
    state.angVelocity[0]=source.body->m_a_vel.x;state.angVelocity[1]=source.body->m_a_vel.y;state.angVelocity[2]=source.body->m_a_vel.z;
    state.mass=source.mass;state.friction=source.friction;state.bounce=source.bounce;state.timeLastAsleep=physGlob.timeLastUpdate;
    state.id=source.id;state.buoyancy=source.buoyancy;state.underwater=source.underwater;
}
using SavedEntityBody=checkpoint::EntityPhysicsState<BodyState>;
SavedEntityBody ReadEntityBody(const PhysObjUserData& source){
    SavedEntityBody saved;ReadBodyState(source,saved.body);auto& body=*source.body;
    saved.gravity[0]=body.m_gravity_acc_vec.x;saved.gravity[1]=body.m_gravity_acc_vec.y;saved.gravity[2]=body.m_gravity_acc_vec.z;
    saved.linearDrag=body.m_t_drag_coef;saved.angularDrag=body.m_a_drag_coef;saved.maxAngularVelocity=body.m_max_avel;saved.stableTime=body.m_stable_energy_time;saved.stable=(body.m_flags&4)!=0;
    saved.eventTime=source.m_time_since_last_event;saved.reevalTime=source.m_time_since_last_reeval;saved.flags=source.m_flags;saved.buoyantTime=source.timeBuoyant;saved.rippleTime=source.timeRipple;saved.trajectoryDuration=source.trDuration;
    saved.references=source.refcount;saved.constraints=body.m_constraint_count;saved.vehicle=source.vehicle!=nullptr;return saved;
}
struct SavedNitrous {NitrousVehicle value;bool active=false,path=false;BodyState body{};};
template<class V>void NitroStateFields(V& v,SavedNitrous& x){NitroFields(v,x.value);v.Field("active_body",x.active);v.Field("path_constraint",x.path);if(x.active)WorldFields(v,x.body);}
template<class T>unsigned PoolSlot(const void* p,const T* base,unsigned count){
    const auto value=reinterpret_cast<std::uintptr_t>(p),start=reinterpret_cast<std::uintptr_t>(base);
    if(!base||value<start||value-start>=sizeof(T)*count||(value-start)%sizeof(T))throw std::runtime_error("checkpoint resource is outside its owner pool");
    return static_cast<unsigned>((value-start)/sizeof(T));
}
std::vector<checkpoint::ResourceDescriptor> BuildCatalog(const checkpoint::Resources& refs){
    using namespace checkpoint;std::vector<ResourceDescriptor> result;result.reserve(refs.entries.size());
    for(const auto& entry:refs.entries){ResourceDescriptor desc;desc.kind=entry.kind;const auto* pointer=entry.pointer;
        switch(entry.kind){
        case ResourceKind::Entity:desc.owner=ResourceOwner::Entity;desc.slot=PoolSlot(pointer,g_entities,MAX_GENTITIES);break;
        case ResourceKind::Client:desc.owner=ResourceOwner::Client;desc.slot=PoolSlot(pointer,level.clients,level.maxclients);break;
        case ResourceKind::Actor:desc.owner=ResourceOwner::Actor;desc.slot=PoolSlot(pointer,level.actors,MAX_ACTORS);break;
        case ResourceKind::Sentient:desc.owner=ResourceOwner::Sentient;desc.slot=PoolSlot(pointer,level.sentients,MAX_SENTIENTS);break;
        case ResourceKind::Turret:desc.owner=ResourceOwner::Turret;desc.slot=PoolSlot(pointer,level.turrets,MAX_TURRETS);break;
        case ResourceKind::Destructible:desc.owner=ResourceOwner::Destructible;desc.slot=PoolSlot(pointer,s_destructibles,G_MAX_DESTRUCTIBLES_SP);break;
        case ResourceKind::Vehicle:desc.owner=ResourceOwner::Vehicle;desc.slot=PoolSlot(pointer,level.vehicles,MAX_VEHICLES);break;
        case ResourceKind::PathNode:desc.owner=ResourceOwner::PathNode;desc.slot=PoolSlot(pointer,g_path.actualNodeCount?Path_ConvertIndexToNode(0):nullptr,g_path.actualNodeCount);break;
        case ResourceKind::CustomVehiclePath:desc.owner=ResourceOwner::CustomVehiclePath;desc.slot=PoolSlot(pointer,gCustomPaths,10);break;
        case ResourceKind::AnimationScript:{
            bool found=false;desc.owner=ResourceOwner::SpecificAnimation;
            for(unsigned i=0;i<MAX_ACTORS;++i)if(pointer==&level.actors[i].AnimScriptSpecific){desc.slot=i;found=true;break;}
            if(!found){desc.owner=ResourceOwner::GlobalAnimation;const AnimScriptList* lists[]={&g_scr_data.dogAnim,&g_scr_data.humanAnim,&g_scr_data.zombieAnim,&g_scr_data.zombieDogAnim};
                static_assert(sizeof(AnimScriptList)%sizeof(scr_animscript_t)==0);
                for(unsigned i=0;i<4;++i){auto value=reinterpret_cast<std::uintptr_t>(pointer),start=reinterpret_cast<std::uintptr_t>(lists[i]);
                    if(value>=start&&value-start<sizeof(AnimScriptList)&&(value-start)%sizeof(scr_animscript_t)==0){desc.slot=static_cast<unsigned>((value-start)/sizeof(scr_animscript_t));desc.auxiliary=i;found=true;break;}}
            }
            if(!found)throw std::runtime_error("checkpoint animation descriptor has no owner");break;
        }
        default:{bool found=false;
            if(entry.kind==ResourceKind::NitrousVehicle){desc.owner=ResourceOwner::NitrousVehicle;for(unsigned i=0;i<MAX_VEHICLES;++i)if(pointer==level.vehicles[i].nitrousVehicle){desc.slot=i;found=true;break;}}
            else for(unsigned i=0;i<static_cast<unsigned>(level.num_entities);++i){const auto& entity=g_entities[i];
                bool match=false;
                if(entry.kind==ResourceKind::AnimationTree){desc.owner=ResourceOwner::AnimationTree;DObj* object=entity.r.inuse?Com_GetServerDObj(i):nullptr;match=object&&object->tree==pointer;
                    if(match&&entity.s.eType==ET_ACTOR_CORPSE)for(unsigned c=0;c<MAX_ACTOR_CORPSES_SP;++c)if(g_scr_data.actorCorpseInfo[c].tree==pointer){desc.auxiliary=c+1;break;}}
                if(entry.kind==ResourceKind::TagInfo){desc.owner=ResourceOwner::TagInfo;match=entity.tagInfo==pointer;}
                if(entry.kind==ResourceKind::ScriptedAnimation){desc.owner=ResourceOwner::ScriptedAnimation;match=entity.scriptedAnim_SP==pointer;}
                if(entry.kind==ResourceKind::Physics){desc.owner=ResourceOwner::PhysicsEntity;match=entity.physObjId==reinterpret_cast<std::intptr_t>(pointer);}
                if(match){desc.slot=i;found=true;break;}
            }
            if(!found)throw std::runtime_error("checkpoint native resource has no captured owner (kind "+std::to_string(static_cast<unsigned>(entry.kind))+")");break;
        }}
        ValidateResourceDescriptor(desc);result.push_back(desc);
    }
    return result;
}
template<class V>void CorpseFields(V& v,corpseInfo_t& x){
    v.Field("entity",x.entnum);v.Field("time",x.time);v.Field("falling",x.falling);
    auto& p=x.ProneInfo;v.Field("orientation",p.bCorpseOrientation);v.Field("pitch_enabled",p.orientPitch);v.Field("prone",p.prone);v.Field("prone_time",p.iProneTime);v.Field("transition",p.iProneTrans);v.Field("height",p.fBodyHeight);v.Field("body_pitch",p.fBodyPitch);v.Field("body_roll",p.fBodyRoll);
}
template<class V>void SoundNotifyFields(V& v,SPSoundNotifyState& x){v.StringId("notify",x.notifyString);v.Field("alias",x.alias);v.Field("replace",x.notifyReplaceAllowed);v.Field("start",x.startTime);v.Field("length",x.lengthMs);}
struct SavedGlobals {
    unsigned random=0;int aiLimit=0,physicsSnapshot=0,physicsUpdate=0;threat_bias_t threat{};PathLinkInfo blockers[2048]{};int blockersInitialized=0;std::vector<const char*> configstrings=std::vector<const char*>(MAX_CONFIGSTRINGS,nullptr);
};
template<class V>void GlobalFields(V& v,SavedGlobals& x){
    v.Field("blockers_initialized",x.blockersInitialized);for(auto& link:x.blockers){v.Field("from",link.from);v.Field("to",link.to);v.Field("previous",link.prev);v.Field("next",link.next);if(link.prev>=2048||link.next>=2048)throw std::runtime_error("checkpoint navigation blocker list invalid");}
    v.Field("threat_groups",x.threat.threatGroupCount);if(x.threat.threatGroupCount<1||x.threat.threatGroupCount>16)throw std::runtime_error("checkpoint threat group count invalid");
    for(auto& name:x.threat.groupName)v.StringId("threat_group_name",name);v.Field("threat_bias",x.threat.threatTable);
    v.Field("physics_snapshot",x.physicsSnapshot);v.Field("physics_update",x.physicsUpdate);v.Field("random",x.random);v.Field("ai_limit",x.aiLimit);unsigned count=static_cast<unsigned>(x.configstrings.size());v.Field("configstring_count",count);
    if(count!=MAX_CONFIGSTRINGS)throw std::runtime_error("checkpoint configstring schema mismatch");
    for(auto& text:x.configstrings)v.Text("configstring",text);
}
void Word(std::vector<std::uint8_t>& out,unsigned value){for(int i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
void Record(std::vector<std::uint8_t>& out,unsigned type,unsigned slot,unsigned id,const std::vector<std::uint8_t>& bytes){
    if(bytes.size()>checkpoint::kMaxBytes-16||out.size()>checkpoint::kMaxBytes-16-bytes.size())throw std::runtime_error("checkpoint world section too large");
    Word(out,type);Word(out,slot);Word(out,id);Word(out,static_cast<unsigned>(bytes.size()));out.insert(out.end(),bytes.begin(),bytes.end());
}
}
struct CheckpointWorldStage_SP::Impl {
    std::unique_ptr<level_locals_t> level=std::make_unique<level_locals_t>();
    std::unique_ptr<TurretInfo[]> turrets=std::make_unique<TurretInfo[]>(MAX_TURRETS);
    std::unique_ptr<scr_vehicle_s[]> vehicles=std::make_unique<scr_vehicle_s[]>(MAX_VEHICLES);
    std::unique_ptr<game_hudelem_s[]> hud=std::make_unique<game_hudelem_s[]>(1024);
    std::unique_ptr<Destructible[]> destructibles=std::make_unique<Destructible[]>(G_MAX_DESTRUCTIBLES_SP);
    std::vector<SavedDestructible> pieces=std::vector<SavedDestructible>(G_MAX_DESTRUCTIBLES_SP);
    std::vector<pathnode_t> nodes;std::vector<SavedNode> links;
    vehicle_custom_path_t paths[10]{};
    std::vector<std::pair<unsigned,SavedEntityBody>> bodies;
    SPSoundNotifyState sounds[1024]{};
    corpseInfo_t corpses[MAX_ACTOR_CORPSES_SP]{};
    SavedGlobals globals;
    explicit Impl(unsigned count):nodes(count),links(count){}
};
CheckpointWorldStage_SP::CheckpointWorldStage_SP(unsigned nodes):impl_(std::make_unique<Impl>(nodes)){}
CheckpointWorldStage_SP::~CheckpointWorldStage_SP()=default;
level_locals_t& CheckpointWorldStage_SP::Level(){return *impl_->level;}
void* CheckpointWorldStage_SP::Pool(checkpoint::ResourceOwner owner,unsigned slot){
    using checkpoint::ResourceOwner;
    switch(owner){
    case ResourceOwner::Turret:if(slot<MAX_TURRETS)return &impl_->turrets[slot];break;
    case ResourceOwner::Vehicle:if(slot<MAX_VEHICLES)return &impl_->vehicles[slot];break;
    case ResourceOwner::Destructible:if(slot<G_MAX_DESTRUCTIBLES_SP)return &impl_->destructibles[slot];break;
    case ResourceOwner::PathNode:if(slot<impl_->nodes.size())return &impl_->nodes[slot];break;
    case ResourceOwner::CustomVehiclePath:if(slot<10)return &impl_->paths[slot];break;
    default:break;
    }
    throw std::runtime_error("checkpoint staged world pool slot out of range");
}
void CheckpointWorldStage_SP::Read(const std::vector<checkpoint::NativeRecord>& records,checkpoint::Resources& refs){
    for(const auto& record:records){checkpoint::FieldReader reader(record.bytes,refs);
        switch(record.group){
        case 0:LevelFields(reader,*impl_->level);break;
        case 1:WorldFields(reader,impl_->turrets[record.slot]);break;
        case 2:WorldFields(reader,impl_->vehicles[record.slot]);break;
        case 3:WorldFields(reader,impl_->hud[record.slot]);break;
        case 4:{auto& saved=impl_->pieces[record.slot];DestructibleFields(reader,saved);saved.value.pieceArray=saved.pieces.data();saved.value.pieceCount=static_cast<int>(saved.pieces.size());impl_->destructibles[record.slot]=saved.value;break;}
        case 5:{auto& node=impl_->links.at(record.slot);NodeFields(reader,node);node.value.constant.Links=node.links.data();node.value.constant.totalLinkCount=static_cast<unsigned short>(node.links.size());impl_->nodes.at(record.slot)=node.value;break;}
        case 6:WorldFields(reader,impl_->paths[record.slot]);break;
        case 7:{SavedEntityBody state;checkpoint::EntityPhysicsFields(reader,state);impl_->bodies.push_back({record.resource,state});break;}
        case 8:{std::vector<checkpoint::ResourceDescriptor> catalog;checkpoint::ResourceCatalogFields(reader,catalog);break;}
        case 9:GlobalFields(reader,impl_->globals);break;
        case 13:SoundNotifyFields(reader,impl_->sounds[record.slot]);break;
        case 12:CorpseFields(reader,impl_->corpses[record.slot]);break;
        case 11:G_CheckpointApplyAnimCommands_SP(record.bytes,{},false);continue;
        case 10:{SavedNitrous saved;NitroStateFields(reader,saved);break;}
        default:throw std::runtime_error("unsupported checkpoint staged world record");
        }
        reader.Finish();
    }
    const auto& level=*impl_->level;
    if(level.num_entities<1||level.num_entities>MAX_GENTITIES||level.maxclients<1||level.maxclients>32||level.num_actors<0||level.num_actors>MAX_ACTORS)throw std::runtime_error("invalid staged world pool counts");
}
void G_CheckpointApplyWorld_SP(const std::vector<checkpoint::NativeRecord>& records,checkpoint::Resources& refs){
    for(const auto& record:records){checkpoint::FieldReader reader(record.bytes,refs);
        switch(record.group){
        case 0:LevelFields(reader,level);break;
        case 1:WorldFields(reader,level.turrets[record.slot]);break;
        case 2:WorldFields(reader,level.vehicles[record.slot]);break;
        case 3:WorldFields(reader,g_hudelems[record.slot]);break;
        case 4:{SavedDestructible saved;DestructibleFields(reader,saved);auto& target=s_destructibles[record.slot];target=saved.value;
            target.ddef=DestructibleDefPrecache(saved.definition);target.pieceCount=static_cast<int>(saved.pieces.size());
            if(!target.ddef||target.ddef->numPieces!=target.pieceCount)throw std::runtime_error("checkpoint destructible asset changed");
            target.pieceArray=static_cast<DESTRUCTIBLE_PIECE_INFO*>(Hunk_UserAlloc(g_pieceArrayHunk,static_cast<unsigned>(saved.pieces.size()*sizeof(DESTRUCTIBLE_PIECE_INFO)),4,"checkpoint pieces"));
            for(unsigned i=0;i<saved.pieces.size();++i){target.pieceArray[i]=saved.pieces[i];target.pieceArray[i].xdollHandle=0;target.pieceArray[i].fx=0;target.pieceArray[i].burnData.fx=0;target.pieceArray[i].burnData.sndId=0;}
            target.destructiblePoseID=0;++s_numDestructibles;break;}
        case 5:{SavedNode saved;checkpoint::Resources borrowed;borrowed.findString=[](const char* name){return static_cast<std::uint16_t>(SL_FindString(name,SCRIPTINSTANCE_SERVER));};checkpoint::FieldReader nodeReader(record.bytes,borrowed);NodeFields(nodeReader,saved);nodeReader.Finish();auto* target=Path_ConvertIndexToNode(record.slot);auto* links=target->constant.Links;
            if(saved.links.size()>target->constant.totalLinkCount)throw std::runtime_error("checkpoint path links exceed current map allocation");
            *target=saved.value;target->constant.Links=links;target->constant.totalLinkCount=static_cast<unsigned short>(saved.links.size());
            for(unsigned i=0;i<saved.links.size();++i)links[i]=saved.links[i];continue;}
        case 6:WorldFields(reader,gCustomPaths[record.slot]);break;
        case 7:continue; // Rebuilt after entity models, before simulation resumes.
        case 13:SoundNotifyFields(reader,*G_SPGetEntitySoundNotifyState(&g_entities[record.slot]));break;
        case 12:CorpseFields(reader,g_scr_data.actorCorpseInfo[record.slot]);break;
        case 11:continue; // References bind after XAnimInfo relocation.
        case 10:continue; // Vehicle models must exist before physics owners are initialized.
        case 8:{std::vector<checkpoint::ResourceDescriptor> catalog;checkpoint::ResourceCatalogFields(reader,catalog);break;}
        case 9:{SavedGlobals globals;GlobalFields(reader,globals);std::memcpy(g_path.pathLinkInfoArray,globals.blockers,sizeof(globals.blockers));g_path.pathLinkInfoArrayInited=globals.blockersInitialized;g_threatBias=globals.threat;holdrand=globals.random;physGlob.timeLastSnapshot=globals.physicsSnapshot;physGlob.timeLastUpdate=globals.physicsUpdate;G_SetAILimit_SP(globals.aiLimit);
            for(unsigned i=0;i<MAX_CONFIGSTRINGS;++i){const char* text=globals.configstrings[i]?globals.configstrings[i]:"";
                const char* existing=sv.configstrings[i]?SL_ConvertToString(sv.configstrings[i],SCRIPTINSTANCE_SERVER):"";
                if(std::strcmp(existing?existing:"",text))SV_SetConfigstring(i,const_cast<char*>(text));}
            break;}
        default:throw std::runtime_error("unsupported live world record");
        }
        reader.Finish();
    }
}
void G_CheckpointValidatePhysics_SP(const std::vector<checkpoint::NativeRecord>& records,const std::vector<checkpoint::ResourceDescriptor>& catalog,const gentity_s* entities,const checkpoint::Resources& refs){
    std::vector<bool> seen(catalog.size(),false);
    for(const auto& record:records)if(record.group==7){
        if(!record.resource||record.resource>catalog.size()||seen[record.resource-1]||record.slot!=record.resource-1)throw std::runtime_error("checkpoint physics record/catalog mismatch");
        seen[record.resource-1]=true;const auto& owner=catalog[record.resource-1];
        if(owner.owner!=checkpoint::ResourceOwner::PhysicsEntity)throw std::runtime_error("checkpoint physics owner has wrong kind");
        SavedEntityBody saved;checkpoint::Resources metadata;checkpoint::FieldReader reader(record.bytes,metadata);checkpoint::EntityPhysicsFields(reader,saved);reader.Finish();
        const auto& entity=entities[owner.slot];
        if(!entity.r.inuse||saved.body.id!=static_cast<int>(owner.slot)||entity.physObjId!=reinterpret_cast<std::intptr_t>(refs.Resolve(checkpoint::ResourceKind::Physics,record.resource)))throw std::runtime_error("checkpoint physics entity ownership mismatch");
        if(entity.classname!=scr_const.script_brushmodel&&(!entity.model||entity.model>=512||!G_GetModel(entity.model)))throw std::runtime_error("checkpoint physics owner has no loaded collision model");
        if(entity.classname==scr_const.script_brushmodel&&static_cast<unsigned>(entity.s.index.brushmodel)>=cm.numSubModels)throw std::runtime_error("checkpoint physics brush model is outside the map");
    }
    for(unsigned i=0;i<catalog.size();++i)if(catalog[i].owner==checkpoint::ResourceOwner::PhysicsEntity&&!seen[i])throw std::runtime_error("checkpoint physics owner has no body state");
}
void G_CheckpointRebuildPhysics_SP(const std::vector<checkpoint::NativeRecord>& records,const std::vector<checkpoint::ResourceDescriptor>& catalog,checkpoint::Resources& refs){
    struct Lock {Lock(){Sys_EnterCriticalSection(CRITSECT_PHYSICS_UPDATE);Sys_EnterCriticalSection(CRITSECT_PHYSICS);}~Lock(){Sys_LeaveCriticalSection(CRITSECT_PHYSICS);Sys_LeaveCriticalSection(CRITSECT_PHYSICS_UPDATE);}} lock;
    for(const auto& record:records)if(record.group==7){
        const auto& owner=catalog.at(record.resource-1);auto& entity=g_entities[owner.slot];SavedEntityBody saved;
        checkpoint::FieldReader reader(record.bytes,refs);checkpoint::EntityPhysicsFields(reader,saved);reader.Finish();
        if(entity.physObjId)throw std::runtime_error("checkpoint physics body was initialized before model reconstruction");
        gjk_geom_list_t geometry{};create_gjk_geom_collision_visitor visitor;visitor.gjk_geom_list=&geometry;
        create_gjk_geom(&entity,&visitor,false,0x280EC93,true);
        if(!geometry.m_geom_count){destroy_gjk_geom(&geometry);throw std::runtime_error("checkpoint physics collision shape is empty");}
        // This constructor consumes the shapes and restores momentum directly.
        // G_CreatePhysicsObject would repeat launch impulses and map constraints.
        auto* data=Phys_CreateBodyFromState(0,&saved.body,&geometry,false);
        if(!data||!data->body)throw std::runtime_error("checkpoint physics allocation failed");
        if(data->refcount!=1||data->id!=saved.body.id||data->vehicle){Phys_ObjDestroy(0,reinterpret_cast<std::intptr_t>(data));throw std::runtime_error("checkpoint physics body ID collides with another owner");}
        const auto* token=refs.Resolve(checkpoint::ResourceKind::Physics,record.resource);
        entity.physObjId=reinterpret_cast<std::intptr_t>(data);refs.BindRebuiltPhysics(record.resource,token,data);
        data->trDuration=saved.trajectoryDuration;data->m_flags=saved.flags;data->timeBuoyant=saved.buoyantTime;data->timeRipple=saved.rippleTime;data->m_time_since_last_event=saved.eventTime;data->m_time_since_last_reeval=saved.reevalTime;
        auto& body=*data->body;body.m_gravity_acc_vec.x=saved.gravity[0];body.m_gravity_acc_vec.y=saved.gravity[1];body.m_gravity_acc_vec.z=saved.gravity[2];
        body.m_t_drag_coef=saved.linearDrag;body.m_a_drag_coef=saved.angularDrag;body.m_max_avel=saved.maxAngularVelocity;body.m_stable_energy_time=saved.stableTime;body.m_flags=(body.m_flags&~4u)|(saved.stable?4u:0u);
        Com_Printf(15,"CHECKPOINT_PHYSICS_RESTORE entity=%u mass=%.3f\n",owner.slot,saved.body.mass);
    }
}
void G_CheckpointRebuildVehicles_SP(const std::vector<checkpoint::NativeRecord>& records,checkpoint::Resources& refs){
    for(const auto& record:records)if(record.group==10){auto& vehicle=level.vehicles[record.slot];auto* native=vehicle.nitrousVehicle;
        const auto* info=BG_GetVehicleInfo(vehicle.infoIdx);native->init(&g_entities[vehicle.entNum],&info->nitrousVehParams);
        SavedNitrous saved;checkpoint::FieldReader reader(record.bytes,refs);NitroStateFields(reader,saved);reader.Finish();
        if(saved.active)native->unpause_physics();
        std::vector<std::uint8_t> scalar;checkpoint::FieldWriter writer(scalar,refs);NitroFields(writer,saved.value);checkpoint::FieldReader nr(scalar,refs);NitroFields(nr,*native);nr.Finish();
        if(saved.active){float quat[4];AxisToQuat(saved.body.rotation,quat);auto id=reinterpret_cast<std::intptr_t>(native->m_phys_user_data);Phys_ObjSetOrientation(id,saved.body.position,quat);Phys_ObjSetVelocity(id,saved.body.velocity);Phys_ObjSetAngularVelocityRaw(id,saved.body.angVelocity);}
        if(saved.path)native->start_path((saved.value.m_flags&0x100)?1:0);
    }
}
void G_CheckpointBindWorldHandles_SP(){
    CheckpointHandleBinder_SP binder;LevelFields(binder,level);
    for(unsigned i=0;i<MAX_TURRETS;++i)if(level.turrets[i].inuse)WorldFields(binder,level.turrets[i]);
    for(unsigned i=0;i<MAX_VEHICLES;++i)if(level.vehicles[i].entNum!=1023)WorldFields(binder,level.vehicles[i]);
    for(unsigned i=0;i<g_path.actualNodeCount;++i)binder.Handle("node owner",Path_ConvertIndexToNode(i)->dynamic.pOwner);
}
void* G_CheckpointLiveWorldPool_SP(checkpoint::ResourceOwner owner,unsigned slot){
    using checkpoint::ResourceOwner;
    switch(owner){
    case ResourceOwner::Turret:if(slot<MAX_TURRETS)return &level.turrets[slot];break;
    case ResourceOwner::Vehicle:if(slot<MAX_VEHICLES)return &level.vehicles[slot];break;
    case ResourceOwner::Destructible:if(slot<G_MAX_DESTRUCTIBLES_SP)return &s_destructibles[slot];break;
    case ResourceOwner::PathNode:if(slot<g_path.actualNodeCount)return Path_ConvertIndexToNode(slot);break;
    case ResourceOwner::CustomVehiclePath:if(slot<10)return &gCustomPaths[slot];break;
    default:break;
    }
    throw std::runtime_error("checkpoint live world pool slot out of range");
}
bool G_CheckpointCaptureWorld_SP(checkpoint::Resources& refs,std::vector<std::uint8_t>& bytes,CheckpointWorldStats_SP& stats,std::string& error){
    stats={};bytes.clear();std::string context;
    try{
        Word(bytes,1);std::vector<std::uint8_t> record,again;
        auto capture=[&](auto& source,auto& staging,auto visitor,unsigned type,unsigned slot,unsigned id){
            record.clear();checkpoint::FieldWriter writer(record,refs);visitor(writer,source);
            checkpoint::FieldReader reader(record,refs);visitor(reader,staging);reader.Finish();
            again.clear();checkpoint::FieldWriter rewriter(again,refs);visitor(rewriter,staging);
            if(record!=again)throw std::runtime_error("world record changed during round-trip");Record(bytes,type,slot,id,record);
        };
        context="level";auto levelStage=std::make_unique<level_locals_t>();capture(level,*levelStage,[](auto& io,auto& x){LevelFields(io,x);},0,0,0);
        TurretInfo turret{};if(level.turrets)for(unsigned i=0;i<MAX_TURRETS;++i)if(level.turrets[i].inuse){
            context="turret "+std::to_string(i);capture(level.turrets[i],turret,[](auto& io,auto& x){WorldFields(io,x);},1,i,refs.Intern(checkpoint::ResourceKind::Turret,&level.turrets[i]));++stats.turrets;
        }
        auto vehicle=std::make_unique<scr_vehicle_s>();if(level.vehicles)for(unsigned i=0;i<MAX_VEHICLES;++i)if(level.vehicles[i].entNum!=1023){
            context="vehicle "+std::to_string(i);capture(level.vehicles[i],*vehicle,[](auto& io,auto& x){WorldFields(io,x);},2,i,refs.Intern(checkpoint::ResourceKind::Vehicle,&level.vehicles[i]));++stats.vehicles;
        }
        game_hudelem_s hud{};for(unsigned i=0;i<1024;++i)if(g_hudelems[i].elem.type){
            context="HUD "+std::to_string(i);capture(g_hudelems[i],hud,[](auto& io,auto& x){WorldFields(io,x);},3,i,0);++stats.hud;
        }
        for(unsigned i=0;i<G_MAX_DESTRUCTIBLES_SP;++i){auto& source=s_destructibles[i];if(source.entNum==1023)continue;
            context="destructible "+std::to_string(i);if(!source.ddef||source.ddef->numPieces<0||source.ddef->numPieces>4096||(!source.pieceArray&&source.ddef->numPieces))throw std::runtime_error("invalid live destructible definition/pieces");
            SavedDestructible saved,staging;saved.value=source;saved.definition=source.ddef->name;
            if(source.ddef->numPieces)saved.pieces.assign(source.pieceArray,source.pieceArray+source.ddef->numPieces);
            capture(saved,staging,[](auto& io,auto& x){DestructibleFields(io,x);},4,i,refs.Intern(checkpoint::ResourceKind::Destructible,&source));++stats.destructibles;
        }
        for(unsigned i=0;i<g_path.actualNodeCount;++i){context="path node "+std::to_string(i);auto* node=Path_ConvertIndexToNode(i);SavedNode saved,staging;saved.value=*node;
            if(node->constant.totalLinkCount){if(!node->constant.Links)throw std::runtime_error("path node has null links");saved.links.assign(node->constant.Links,node->constant.Links+node->constant.totalLinkCount);}
            capture(saved,staging,[](auto& io,auto& x){NodeFields(io,x);},5,i,refs.Intern(checkpoint::ResourceKind::PathNode,node));++stats.nodes;
        }
        // Additional resource references can be discovered while encoding world records.
        for(unsigned i=0;i<refs.entries.size();++i){const auto entry=refs.entries[i];
            if(entry.kind==checkpoint::ResourceKind::CustomVehiclePath){context="custom vehicle path "+std::to_string(i+1);auto& source=*const_cast<vehicle_custom_path_t*>(static_cast<const vehicle_custom_path_t*>(entry.pointer));vehicle_custom_path_t staging{};
                capture(source,staging,[](auto& io,auto& x){WorldFields(io,x);},6,PoolSlot(entry.pointer,gCustomPaths,10),i+1);}
            if(entry.kind==checkpoint::ResourceKind::Physics){context="physics body "+std::to_string(i+1);auto saved=ReadEntityBody(*static_cast<const PhysObjUserData*>(entry.pointer));SavedEntityBody staging;
                capture(saved,staging,[](auto& io,auto& x){checkpoint::EntityPhysicsFields(io,x);},7,i,i+1);++stats.bodies;
                Com_Printf(15,"CHECKPOINT_PHYSICS_CAPTURE entity=%i mass=%.3f constraints=%i\n",saved.body.id,saved.body.mass,saved.constraints);}
        }
        for(unsigned i=0;i<MAX_VEHICLES;++i)if(level.vehicles[i].entNum!=1023&&level.vehicles[i].nitrousVehicle){
            auto& source=*level.vehicles[i].nitrousVehicle;SavedNitrous saved,staging;saved.value=source;saved.active=source.m_phys_user_data!=nullptr;saved.path=source.m_vpc!=nullptr;
            if(saved.active)ReadBodyState(*source.m_phys_user_data,saved.body);
            context="vehicle physics "+std::to_string(i);capture(saved,staging,[](auto& io,auto& x){NitroStateFields(io,x);},10,i,refs.Intern(checkpoint::ResourceKind::NitrousVehicle,&source));
        }
        for(unsigned i=0;i<1024;++i){auto& source=*G_SPGetEntitySoundNotifyState(&g_entities[i]);if(source.notifyString){SPSoundNotifyState staging{};capture(source,staging,[](auto& io,auto& x){SoundNotifyFields(io,x);},13,i,0);}}
        for(unsigned i=0;i<MAX_ACTOR_CORPSES_SP;++i){corpseInfo_t staging{};capture(g_scr_data.actorCorpseInfo[i],staging,[](auto& io,auto& x){CorpseFields(io,x);},12,i,0);}
        context="animation commands";record.clear();G_CheckpointCaptureAnimCommands_SP(record);G_CheckpointApplyAnimCommands_SP(record,{},false);Record(bytes,11,0,0,record);
        context="globals";SavedGlobals globals,globalsStage;globals.random=holdrand;std::memcpy(globals.blockers,g_path.pathLinkInfoArray,sizeof(globals.blockers));globals.blockersInitialized=g_path.pathLinkInfoArrayInited;globals.threat=g_threatBias;globals.physicsSnapshot=physGlob.timeLastSnapshot;globals.physicsUpdate=physGlob.timeLastUpdate;globals.aiLimit=G_GetAILimit_SP();
        for(unsigned i=0;i<MAX_CONFIGSTRINGS;++i)globals.configstrings[i]=sv.configstrings[i]?SL_ConvertToString(sv.configstrings[i],SCRIPTINSTANCE_SERVER):nullptr;
        capture(globals,globalsStage,[](auto& io,auto& x){GlobalFields(io,x);},9,0,0);
        context="resource catalog";auto catalog=BuildCatalog(refs);std::vector<checkpoint::ResourceDescriptor> restored;
        capture(catalog,restored,[](auto& io,auto& x){checkpoint::ResourceCatalogFields(io,x);},8,0,0);
        checkpoint::ParseNativeRecords(checkpoint::SectionId::World,bytes);
        return true;
    }catch(const std::exception& e){error=context+": "+e.what();bytes.clear();return false;}
}
#endif
