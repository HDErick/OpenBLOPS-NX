#include "g_checkpoint_client_sp.h"
#ifdef KISAK_SP
#include "checkpoint_record_set.h"
#include "checkpoint_destructible_sp.h"
#include "checkpoint_string_bindings_sp.h"
#include <cgame/cg_spawn.h>
#include <clientscript/cscr_stringlist.h>
#include <physics/destructible.h>
#include <database/db_registry.h>
#include <gfx_d3d/r_material.h>
#include <client/client.h>
#include <cgame_mp/cg_local_mp.h>
#include <clientscript/cscr_save_write.h>
#include <clientscript/cscr_checkpoint_identity.h>
#include <clientscript/cscr_vm.h>
#include <clientscript/cscr_variable.h>
#include <universal/memfile.h>
#include <qcommon/threads.h>
#include <memory>
#include <clientscript/cscr_save.h>
#include <clientscript/cscr_main.h>
#include <cgame/cg_scr_main.h>
#include <cgame/cg_localents.h>
#include <cgame_mp/cg_main_mp.h>
#include <cgame_mp/cg_animtree_mp.h>
#include <cgame_mp/cg_ents_mp.h>
#include <qcommon/dobj_management.h>
#include <xanim/dobj_utils.h>
#include <xanim/xmodel.h>
#include <cgame_mp/cg_snapshot_mp.h>
#include <cgame_mp/cg_servercmds_mp.h>
#include <cgame_mp/cg_view_mp.h>
#include <cgame_mp/cg_draw_mp.h>
#include <bit>
#include <client_mp/cl_input_mp.h>
#include <game_mp/g_main_mp.h>
#include <universal/com_memory.h>
#include <universal/mem_userhunk.h>
#include <qcommon/common.h>
#include <win32/win_shared.h>
#include <EffectsCore/fx_system.h>
extern HunkUser* g_cgPieceArrayHunk;
extern unsigned int cg_numDestructibles[MAX_LOCAL_CLIENTS];
namespace {
#include "checkpoint_client_fields.inc"
#include "checkpoint_fake_fields.inc"
struct Presentation {
    int time=0,oldTime=0,physicsTime=0,stanceTime=0,weapon=0,offhand=0;
    int landTime = 0;
    float landChange = 0.0f;
    StanceState stance=CL_STANCE_STAND,stancePosition=CL_STANCE_STAND;
    float viewangles[3]={},viewOrg[3]={},movieOrigin[3]={},movieAngles[3]={},movieVelocity[3]={};
    Camera camera{};int triggerIndex=0,pendingCount=0;trigger_info_t triggers[256]{};int entityTriggerIndex[1536]{};
    visionSetVars_t vision[3][VISIONSETMODECOUNT]{};
    visionSetLerpData_t blend[VISIONSETMODECOUNT]{};
    char names[10][64]{};
    bool underwater=false,lowHealth=false;int damageVisionEnd=0,deathQuote=0;
    CG_ScreenFadeState_SP screenFade{};
    unsigned wallTime=0;int scaleStartTime=0,scaleEndTime=0;
    float timeScale=1,scaleStart=1,scaleEnd=1;bool missionFailed=false;
};
template<class V>void PresentationFields(V& v,Presentation& x){
    v.Field("trigger_index",x.triggerIndex);v.Field("entity_triggers",x.entityTriggerIndex);v.Field("pending_triggers",x.pendingCount);if(x.pendingCount<0||x.pendingCount>256)throw std::runtime_error("checkpoint client trigger count invalid");
    for(int i=0;i<x.pendingCount;++i){auto& t=x.triggers[i];v.Field("entity",t.entnum);v.Field("other",t.otherEntnum);v.Field("uses",t.useCount);v.Field("other_uses",t.otherUseCount);}
    v.Field("time",x.time);v.Field("old_time",x.oldTime);v.Field("physics_time",x.physicsTime);v.Field("stance",x.stance);v.Field("stance_position",x.stancePosition);v.Field("stance_time",x.stanceTime);
    v.Field("landing_time", x.landTime);
    v.Field("landing_offset", x.landChange);
    v.Field("weapon",x.weapon);v.Field("offhand",x.offhand);v.Field("view_angles",x.viewangles);v.Field("view_origin",x.viewOrg);
    v.Field("movie_origin",x.movieOrigin);v.Field("movie_angles",x.movieAngles);v.Field("movie_velocity",x.movieVelocity);ClientFields(v,x.camera);
    for(auto& states:x.vision)for(auto& state:states)ClientFields(v,state);for(auto& blend:x.blend)ClientFields(v,blend);
    v.Field("wall_time",x.wallTime);v.Field("screen_fade_target",x.screenFade.targetAlpha);v.Field("screen_fade_start",x.screenFade.startAlpha);v.Field("screen_fade_current",x.screenFade.currentAlpha);
    v.Field("screen_fade_time",x.screenFade.startTime);v.Field("screen_fade_duration",x.screenFade.duration);v.Field("screen_fade_hold",x.screenFade.holdStartUntilEnd);
    v.Field("time_scale",x.timeScale);v.Field("time_scale_start",x.scaleStart);v.Field("time_scale_end",x.scaleEnd);v.Field("time_scale_start_time",x.scaleStartTime);v.Field("time_scale_end_time",x.scaleEndTime);v.Field("mission_failed",x.missionFailed);
    v.Field("vision_names",x.names);v.Field("underwater",x.underwater);v.Field("low_health",x.lowHealth);v.Field("damage_vision_end",x.damageVisionEnd);v.Field("death_quote",x.deathQuote);
}
void Word(std::vector<std::uint8_t>& bytes,unsigned value){for(int i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
void Blob(std::vector<std::uint8_t>& bytes,const std::vector<std::uint8_t>& data){Word(bytes,static_cast<unsigned>(data.size()));bytes.insert(bytes.end(),data.begin(),data.end());}
}
bool G_CheckpointCaptureClient_SP(std::vector<std::uint8_t>& bytes,CheckpointClientStats_SP& stats,std::string& error){
    bytes.clear();stats={};
    try{
        if(!Sys_IsMainThread())throw std::runtime_error("client checkpoint capture requires main-thread ownership");
        auto& vm=gScrVmPub[SCRIPTINSTANCE_CLIENT];auto& pub=gScrVarPub[SCRIPTINSTANCE_CLIENT];
        if(vm.function_count||vm.inparamcount||vm.outparamcount||!pub.levelId||!pub.gameId)throw std::runtime_error("client VM is not idle/initialized");
        std::vector<std::uint8_t> script(8u*1024u*1024u);auto memory=std::make_unique<MemoryFile>();
        MemFile_InitForWriting(memory.get(),static_cast<int>(script.size()),script.data(),false,false);Scr_PrepareSave(SCRIPTINSTANCE_CLIENT);Scr_SaveVariables(SCRIPTINSTANCE_CLIENT,memory.get());MemFile_StartSegment(memory.get(),-1);
        if(memory->memoryOverflow)throw std::runtime_error("client script archive overflow");script.resize(memory->bytesUsed);stats.objects=pub.savecount;stats.identity=Scr_CheckpointIdentity_SP(SCRIPTINSTANCE_CLIENT);
        auto& cg=*CG_GetLocalClientGlobals(0);auto* cl=CL_GetLocalClientGlobals(0);if(!cl)throw std::runtime_error("local client is unavailable");
        auto saved=std::make_unique<Presentation>();auto staging=std::make_unique<Presentation>();
        if(cg_level.currentTriggerListSize)throw std::runtime_error("client trigger dispatcher is active");saved->triggerIndex=cg_level.triggerIndex;saved->pendingCount=cg_level.pendingTriggerListSize;std::memcpy(saved->triggers,cg_level.pendingTriggerList,sizeof(saved->triggers));std::memcpy(saved->entityTriggerIndex,cg_level.entTriggerIndex,sizeof(saved->entityTriggerIndex));
        saved->time=cg.time;saved->oldTime=cg.oldTime;saved->physicsTime=cg.physicsTime;saved->stance=cl->stance;saved->stancePosition=cl->stancePosition;saved->stanceTime=cl->stanceTime;
        saved->landTime = cg.landTime;
        saved->landChange = cg.landChange;
        saved->weapon=cl->cgameUserCmdWeapon;saved->offhand=cl->cgameUserCmdOffHandIndex;saved->camera=cg.cameraData;
        for(int i=0;i<3;++i){saved->viewangles[i]=cl->viewangles[i];saved->viewOrg[i]=cg.refdef.vieworg[i];saved->movieOrigin[i]=cg.movieCameraOrigin[i];saved->movieAngles[i]=cg.movieCameraAngles[i];saved->movieVelocity[i]=cg.movieCameraVelocity[i];}
        for(int i=0;i<VISIONSETMODECOUNT;++i){saved->vision[0][i]=cg.visionSetFrom[i];saved->vision[1][i]=cg.visionSetTo[i];saved->vision[2][i]=cg.visionSetCurrent[i];saved->blend[i]=cg.visionSetLerpData[i];}
        const char* names[]={cg.visionNameNaked,cg.visionNameNight,cg.visionNameFlare,cg.visionNameInfrared,cg.visionNameTvguided,cg.visionNameExtraCam,cg.visionNameLastStand,cg.visionNameDeath,cg.visionNameLowHealth,cg.visionNameUnderwater};
        for(int i=0;i<10;++i)std::memcpy(saved->names[i],names[i],64);
        saved->wallTime=static_cast<unsigned>(Sys_Milliseconds());saved->screenFade=CG_GetScreenFadeState_SP(0);
        saved->timeScale=Com_GetTimeScale();saved->scaleStart=cg.timeScaleStart;saved->scaleEnd=cg.timeScaleEnd;saved->scaleStartTime=cg.timeScaleTimeStart;saved->scaleEndTime=cg.timeScaleTimeEnd;saved->missionFailed=Dvar_GetBool("hud_missionFailed");
        saved->underwater=cg.underwaterVision;saved->lowHealth=cg.lowHealthVision;saved->damageVisionEnd=cg.damageVisionEndTime;saved->deathQuote=cg.deathQuoteTime;
        checkpoint::Resources refs;std::vector<std::uint8_t> fields,again;checkpoint::FieldWriter writer(fields,refs);PresentationFields(writer,*saved);
        checkpoint::FieldReader reader(fields,refs);PresentationFields(reader,*staging);reader.Finish();checkpoint::FieldWriter rewriter(again,refs);PresentationFields(rewriter,*staging);
        if(fields!=again)throw std::runtime_error("client presentation changed during round-trip");
        std::vector<std::uint8_t> fakes,destructibles;unsigned fakeCount=0,destructibleCount=0;
        checkpoint::Resources clientRefs;clientRefs.stringName=[](std::uint16_t id){return SL_ConvertToString(id,SCRIPTINSTANCE_CLIENT);};clientRefs.findString=[](const char* text){return static_cast<std::uint16_t>(SL_FindString(text,SCRIPTINSTANCE_CLIENT));};
        Word(fakes,1);Word(fakes,0);Word(fakes,cg_fakeEntitiesInuseCountFromMap);Word(fakes,cg_fakeEntitiesInuseCountFromLoadScript);
        auto fakeStage=std::make_unique<fake_centity_s>();
        for(unsigned i=0;i<512;++i)if(cg_fakeEntitiesInuseArray[i]){auto& row=cg_fakeEntitiesArray[i];std::vector<std::uint8_t> payload,roundtrip;
            checkpoint::FieldWriter fw(payload,clientRefs);FakeFakeEntity(fw,row);checkpoint::FieldReader fr(payload,clientRefs);FakeFakeEntity(fr,*fakeStage);fr.Finish();checkpoint::FieldWriter again(roundtrip,clientRefs);FakeFakeEntity(again,*fakeStage);
            if(payload!=roundtrip)throw std::runtime_error("client fake entity changed during round-trip");
            Word(fakes,i);Blob(fakes,payload);unsigned destructible=0;
            if(row.cent.destructible){auto delta=reinterpret_cast<std::uintptr_t>(row.cent.destructible)-reinterpret_cast<std::uintptr_t>(cg_destructibles[0]);
                if(delta>=sizeof(Destructible)*CG_MAX_DESTRUCTIBLES_SP||delta%sizeof(Destructible))throw std::runtime_error("fake entity destructible is outside client pool");destructible=static_cast<unsigned>(delta/sizeof(Destructible))+1;}
            Word(fakes,destructible);++fakeCount;
        }
        for(int i=0;i<4;++i)fakes[4+i]=static_cast<std::uint8_t>(fakeCount>>(8*i));
        Word(destructibles,1);Word(destructibles,0);
        for(unsigned i=0;i<CG_MAX_DESTRUCTIBLES_SP;++i){auto& row=cg_destructibles[0][i];if(row.entNum==1023)continue;
            if(!row.ddef||row.ddef->numPieces<0||row.ddef->numPieces>4096||(!row.pieceArray&&row.ddef->numPieces))throw std::runtime_error("invalid client destructible");
            checkpoint::SavedDestructible_SP saved,staging;saved.value=row;saved.definition=row.ddef->name;if(row.ddef->numPieces)saved.pieces.assign(row.pieceArray,row.pieceArray+row.ddef->numPieces);
            std::vector<std::uint8_t> payload,roundtrip;checkpoint::FieldWriter dw(payload,clientRefs);checkpoint::DestructibleFields(dw,saved);checkpoint::FieldReader dr(payload,clientRefs);checkpoint::DestructibleFields(dr,staging);dr.Finish();checkpoint::FieldWriter aw(roundtrip,clientRefs);checkpoint::DestructibleFields(aw,staging);
            if(payload!=roundtrip)throw std::runtime_error("client destructible changed during round-trip");Word(destructibles,i);Blob(destructibles,payload);++destructibleCount;
        }
        for(int i=0;i<4;++i)destructibles[4+i]=static_cast<std::uint8_t>(destructibleCount>>(8*i));
        std::vector<std::uint8_t> models;Word(models,1);Word(models,0);unsigned modelCount=0;
        for(unsigned i=4;i<1536;++i){auto* object=Com_GetClientDObj(i,0);if(!object||!DObjGetNumModels(object))continue;
            auto* cent=CG_GetEntity(0,i);if(cent->nextState.eType==ET_ACTOR||cent->nextState.eType==ET_ACTOR_CORPSE||cent->nextState.eType==ET_PLAYER_CORPSE)continue;
            fake_centity_s row{};row.cent=*cent;const char* model=DObjGetModel(object,0)->name;std::vector<std::uint8_t> payload;checkpoint::FieldWriter mw(payload,clientRefs);mw.Text("model",model);FakeFakeEntity(mw,row);Word(models,i);Blob(models,payload);++modelCount;
        }for(int i=0;i<4;++i)models[4+i]=static_cast<std::uint8_t>(modelCount>>(8*i));
        Word(bytes,5);Word(bytes,stats.identity);Blob(bytes,script);Blob(bytes,fields);Blob(bytes,fakes);Blob(bytes,destructibles);Blob(bytes,models);stats.bytes=static_cast<unsigned>(bytes.size());stats.fakes=fakeCount;stats.destructibles=destructibleCount;return true;
    }catch(const std::exception& e){error=e.what();bytes.clear();return false;}
}

namespace {
struct ClientStage {
    checkpoint::Resources refs;Presentation presentation;
    std::vector<std::uint8_t> script;
    struct Fake {unsigned slot,destructible;fake_centity_s value{};};
    struct DestructibleRow {unsigned slot;checkpoint::SavedDestructible_SP saved;};
    struct Model {unsigned slot;const char* name=nullptr;fake_centity_s row{};};std::vector<Model> models;
    std::vector<Fake> fakes;std::vector<DestructibleRow> destructibles;unsigned mapFakes=0,loadFakes=0;
};
std::unique_ptr<ClientStage> ReadClient(const std::vector<std::uint8_t>& bytes,bool owning){
    using namespace checkpoint;RecordInput input(bytes);if(input.Word()!=5||input.Word()!=Scr_CheckpointIdentity_SP(SCRIPTINSTANCE_CLIENT))throw std::runtime_error("client checkpoint code/schema mismatch");
    auto stage=std::make_unique<ClientStage>();stage->script=input.Blob();auto fields=input.Blob(),fakes=input.Blob(),destructibles=input.Blob(),models=input.Blob();if(!input.Empty())throw std::runtime_error("trailing client checkpoint data");
    if(owning)stage->refs.findString=[](const char* text){return static_cast<std::uint16_t>(SL_GetString_(SCRIPTINSTANCE_CLIENT,text,0,15));};else stage->refs.findString=CheckpointTemporaryStrings_SP(SCRIPTINSTANCE_CLIENT);
    FieldReader reader(fields,stage->refs);PresentationFields(reader,stage->presentation);reader.Finish();
    RecordInput di(destructibles);if(di.Word()!=1)throw std::runtime_error("client destructible schema mismatch");auto count=di.Word();if(count>1024)throw std::runtime_error("client destructible count invalid");std::set<unsigned> slots;
    for(unsigned i=0;i<count;++i){ClientStage::DestructibleRow row;row.slot=di.Word();if(row.slot>=1024||!slots.insert(row.slot).second)throw std::runtime_error("client destructible slot invalid");auto payload=di.Blob();FieldReader dr(payload,stage->refs);DestructibleFields(dr,row.saved);dr.Finish();auto* asset=DestructibleDefPrecache(row.saved.definition);if(!asset||asset->numPieces!=row.saved.pieces.size())throw std::runtime_error("client destructible asset mismatch");stage->destructibles.push_back(std::move(row));}if(!di.Empty())throw std::runtime_error("trailing client destructible data");
    RecordInput fi(fakes);if(fi.Word()!=1)throw std::runtime_error("client fake entity schema mismatch");count=fi.Word();stage->mapFakes=fi.Word();stage->loadFakes=fi.Word();if(count>512||stage->mapFakes>count||stage->loadFakes>count)throw std::runtime_error("client fake count invalid");slots.clear();
    for(unsigned i=0;i<count;++i){ClientStage::Fake row;row.slot=fi.Word();auto payload=fi.Blob();row.destructible=fi.Word();if(row.slot>=512||!slots.insert(row.slot).second||row.destructible>1024)throw std::runtime_error("client fake owner invalid");FieldReader fr(payload,stage->refs);FakeFakeEntity(fr,row.value);fr.Finish();if(row.value.cent.nextState.number!=static_cast<int>(1024+row.slot))throw std::runtime_error("client fake number mismatch");stage->fakes.push_back(std::move(row));}if(!fi.Empty())throw std::runtime_error("trailing fake entity data");
    // Model warm-up records borrow strings already owned by fake entities or
    // the network/script state. They must not acquire a second native reference.
    stage->refs.findString=CheckpointTemporaryStrings_SP(SCRIPTINSTANCE_CLIENT);
    RecordInput mi(models);if(mi.Word()!=1)throw std::runtime_error("client model schema mismatch");count=mi.Word();if(count>1536)throw std::runtime_error("client model count invalid");slots.clear();
    for(unsigned i=0;i<count;++i){ClientStage::Model row;row.slot=mi.Word();if(row.slot<4||row.slot>=1536||!slots.insert(row.slot).second)throw std::runtime_error("client model owner invalid");auto payload=mi.Blob();FieldReader mr(payload,stage->refs);mr.Text("model",row.name);FakeFakeEntity(mr,row.row);mr.Finish();if(!row.name||!DB_FindXAssetHeader(ASSET_TYPE_XMODEL,const_cast<char*>(row.name),false,-1).model)throw std::runtime_error("client model asset missing");stage->models.push_back(std::move(row));}if(!mi.Empty())throw std::runtime_error("trailing client model data");return stage;
}
}
bool G_CheckpointValidateClient_SP(const std::vector<std::uint8_t>& bytes,std::string& error){try{return ReadClient(bytes,false)!=nullptr;}catch(const std::exception& e){error=e.what();return false;}}
void G_CheckpointRetireClient_SP(){
    auto* cg=CG_GetLocalClientGlobals(0);CG_ShutdownEntities(0);CG_ShutdownEntity(0,&cg->predictedPlayerEntity,1);CG_ShutdownFakeEntities(0);CG_FreeDestructibles_SP(0);
    FX_KillAllEffects(0);CG_FreeClientEntityCaches(0);CG_FreeClientDObjInfo(0);CG_FreeEntityDObjInfo(0);
    Scr_ShutdownSystem(SCRIPTINSTANCE_CLIENT,1,1);for(unsigned i=0;i<CLASS_NUM_COUNT;++i)Scr_RemoveClassMap(SCRIPTINSTANCE_CLIENT,i);
    cg->snap=cg->nextSnap=nullptr;std::memset(cg->centInNextSnapshot,0,sizeof(cg->centInNextSnapshot));
    for(unsigned i=0;i<1024;++i){auto* cent=CG_GetEntity(0,i);cent->clientFlags&=~2u;cent->tree=nullptr;}
}
bool G_CheckpointApplyClient_SP(const std::vector<std::uint8_t>& bytes,std::string& error){
    try{
        auto stage=ReadClient(bytes,true);Com_Printf(15,"CHECKPOINT_RESTORE_PHASE client_vm\n");
        auto memory=std::make_unique<MemoryFile>();MemFile_InitForReading(memory.get(),static_cast<int>(stage->script.size()),stage->script.data(),false);Scr_LoadVariables(SCRIPTINSTANCE_CLIENT,memory.get());
        auto& pub=gScrVarPub[SCRIPTINSTANCE_CLIENT];auto* debug=gScrVarDebugPub[SCRIPTINSTANCE_CLIENT];if(debug){for(auto id:{pub.levelId,pub.animId,pub.timeArrayId,pub.pauseArrayId})if(id)++debug->extRefCount[id];for(unsigned i=0;i<CLASS_NUM_COUNT;++i){const auto& map=gScrClassMap[SCRIPTINSTANCE_CLIENT][i];if(map.id)++debug->extRefCount[map.id];if(map.entArrayId)++debug->extRefCount[map.entArrayId];}}
        Scr_PostLoadVariables(SCRIPTINSTANCE_CLIENT);
        {std::vector<std::uint8_t> again(8u*1024u*1024u);auto check=std::make_unique<MemoryFile>();MemFile_InitForWriting(check.get(),static_cast<int>(again.size()),again.data(),false,false);Scr_PrepareSave(SCRIPTINSTANCE_CLIENT);Scr_SaveVariables(SCRIPTINSTANCE_CLIENT,check.get());MemFile_StartSegment(check.get(),-1);again.resize(check->bytesUsed);if(again!=stage->script)throw std::runtime_error("client VM differs immediately after restore");}

        for(auto& row:stage->destructibles){auto& target=cg_destructibles[0][row.slot];target=row.saved.value;target.ddef=DestructibleDefPrecache(row.saved.definition);target.pieceCount=static_cast<int>(row.saved.pieces.size());target.destructiblePoseID=0;target.flags&=~1u;
            target.pieceArray=static_cast<DESTRUCTIBLE_PIECE_INFO*>(Hunk_UserAlloc(g_cgPieceArrayHunk,static_cast<int>(row.saved.pieces.size()*sizeof(DESTRUCTIBLE_PIECE_INFO)),4,"checkpoint client pieces"));
            for(unsigned i=0;i<row.saved.pieces.size();++i){target.pieceArray[i]=row.saved.pieces[i];target.pieceArray[i].xdollHandle=0;target.pieceArray[i].fx=0;target.pieceArray[i].burnData.fx=0;target.pieceArray[i].burnData.sndId=0;}
        }cg_numDestructibles[0]=static_cast<unsigned>(stage->destructibles.size());
        for(auto& row:stage->fakes){cg_fakeEntitiesArray[row.slot]=row.value;cg_fakeEntitiesInuseArray[row.slot]=true;auto& cent=cg_fakeEntitiesArray[row.slot].cent;cent.destructible=row.destructible?&cg_destructibles[0][row.destructible-1]:nullptr;cent.pose.physObjId=0;if(cent.nextState.eType==ET_FX||cent.nextState.eType==ET_LOOP_FX)cent.pose.fx.effect=0;}
        cg_fakeEntitiesInuseCount[0]=static_cast<int>(stage->fakes.size());cg_fakeEntitiesInuseCountFromMap=stage->mapFakes;cg_fakeEntitiesInuseCountFromLoadScript=stage->loadFakes;
        auto& cg=*CG_GetLocalClientGlobals(0);auto& cl=*CL_GetLocalClientGlobals(0);const auto& saved=stage->presentation;
        cg_level.triggerIndex=saved.triggerIndex;cg_level.pendingTriggerListSize=saved.pendingCount;cg_level.currentTriggerListSize=0;std::memcpy(cg_level.pendingTriggerList,saved.triggers,sizeof(saved.triggers));std::memcpy(cg_level.entTriggerIndex,saved.entityTriggerIndex,sizeof(saved.entityTriggerIndex));
        CG_InitClientEntityCaches(0);
        // Animation commands belong to the restored timeline. A future
        // watermark marks the checkpoint's full animation baseline as already
        // applied, leaving freshly rebuilt client actor trees in their bind pose.
        cg.lastAppliedAnimCommandTime=-1;
        cg.time=saved.time;cg.oldTime=saved.oldTime;cg.physicsTime=saved.physicsTime;cg.cameraData=saved.camera;cg.predictedPlayerState=level.clients[0].ps;cg.predictedErrorTime=0;std::memset(cg.predictedError,0,sizeof(cg.predictedError));
        cg.landTime = saved.landTime;
        cg.landChange = saved.landChange;
        cg.deathQuoteTime=saved.deathQuote;cg.damageVisionEndTime=saved.damageVisionEnd;cg.underwaterVision=saved.underwater;cg.lowHealthVision=saved.lowHealth;
        // These death effects outlive VM/world restoration. Restore their saved
        // values and rebase wall-clock deadlines rather than retaining death's
        // black overlay/slowdown or discarding an intentional mission fade.
        const unsigned wallDelta=static_cast<unsigned>(Sys_Milliseconds())-saved.wallTime;
        auto fade=saved.screenFade;fade.startTime=std::bit_cast<int>(static_cast<unsigned>(fade.startTime)+wallDelta);CG_SetScreenFadeState_SP(0,fade);
        cg.timeScaleStart=saved.scaleStart;cg.timeScaleEnd=saved.scaleEnd;
        cg.timeScaleTimeStart=saved.scaleStartTime?std::bit_cast<int>(static_cast<unsigned>(saved.scaleStartTime)+wallDelta):0;
        cg.timeScaleTimeEnd=saved.scaleStartTime?std::bit_cast<int>(static_cast<unsigned>(saved.scaleEndTime)+wallDelta):0;
        Com_SetTimeScale(saved.timeScale);Dvar_SetBoolByName("hud_missionFailed",saved.missionFailed);
        cg.viewFade=0;cg.v_dmg_time=0;std::memset(cg.viewDamage,0,sizeof(cg.viewDamage));CG_ClearGenericFilter(&cg);CG_ClearScriptedBlur_SP(0);CG_ResetViewModelHidden_SP(0);CG_InitLocalEntities(0);
        for(int i=0;i<VISIONSETMODECOUNT;++i){cg.visionSetFrom[i]=saved.vision[0][i];cg.visionSetTo[i]=saved.vision[1][i];cg.visionSetCurrent[i]=saved.vision[2][i];cg.visionSetLerpData[i]=saved.blend[i];}
        char* names[]={cg.visionNameNaked,cg.visionNameNight,cg.visionNameFlare,cg.visionNameInfrared,cg.visionNameTvguided,cg.visionNameExtraCam,cg.visionNameLastStand,cg.visionNameDeath,cg.visionNameLowHealth,cg.visionNameUnderwater};for(int i=0;i<10;++i)std::memcpy(names[i],saved.names[i],64);
        for(int i=0;i<3;++i){cl.viewangles[i]=saved.viewangles[i];cg.refdef.vieworg[i]=saved.viewOrg[i];cg.movieCameraOrigin[i]=saved.movieOrigin[i];cg.movieCameraAngles[i]=saved.movieAngles[i];cg.movieCameraVelocity[i]=saved.movieVelocity[i];}
        cl.stance=saved.stance;cl.stancePosition=saved.stancePosition;cl.stanceTime=saved.stanceTime;cl.cgameUserCmdWeapon=saved.weapon;cl.cgameUserCmdOffHandIndex=saved.offhand;cl.savedUserCmdValid=false;
        cl.serverTime=cl.oldServerTime=cl.oldFrameServerTime=cl.oldSnapServerTime=static_cast<int>(level.time);cl.serverTimeDelta=cl.serverTime-cls.realtime;cl.snap.serverTime=static_cast<int>(level.time);cl.snap.ps=level.clients[0].ps;cl.snap.valid=true;cl.extrapolatedSnapshot=cl.newSnapshots=0;for(auto& snap:cl.snapshots)snap.valid=false;
        CG_GetLocalClientStaticGlobals(0)->processedSnapshotNum=cl.snap.messageNum;cg.latestSnapshotNum=cl.snap.messageNum;cg.latestSnapshotTime=level.time;cg.mapRestart=1;
        bgs=&cg.bgs;
        for(auto& row:stage->models){auto* cent=CG_GetEntity(0,row.slot);if(row.slot<1024){*cent=row.row.cent;cent->clientFlags&=~2u;}
            auto* model=DB_FindXAssetHeader(ASSET_TYPE_XMODEL,const_cast<char*>(row.name),false,-1).model;CG_PreProcess_GetDObj(0,row.slot,cent->nextState.eType,model,cent->nextState.animTreeIndex,0);
        }bgs=nullptr;
        Scr_AddString("save_restore",SCRIPTINSTANCE_CLIENT);auto thread=Scr_ExecThread(SCRIPTINSTANCE_CLIENT,cg_scr_data.levelnotify,1);Scr_FreeThread(thread,SCRIPTINSTANCE_CLIENT);
        std::memset(cl.cmds,0,sizeof(cl.cmds));Com_Printf(15,"CHECKPOINT_RESTORE_PHASE client_ready time=%i\n",cg.time);return true;
    }catch(const std::exception& e){bgs=nullptr;error=e.what();return false;}
}
#endif
