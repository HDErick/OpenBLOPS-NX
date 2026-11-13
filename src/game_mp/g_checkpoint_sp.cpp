#include "g_checkpoint_sp.h"
#ifdef KISAK_SP
#include "checkpoint_archive.h"
#include "checkpoint_identity.h"
#include <clientscript/cscr_checkpoint_identity.h>
#include <qcommon/cm_load.h>
#include "g_checkpoint_records_sp.h"
#include "g_checkpoint_world_sp.h"
#include "g_checkpoint_stage_sp.h"
#include "g_checkpoint_client_sp.h"
#include "g_main_mp.h"
#include "g_cmds_mp.h"
#include <clientscript/cscr_save_write.h>
#include <clientscript/cscr_vm.h>
#include <clientscript/cscr_variable.h>
#include <qcommon/common.h>
#include <qcommon/cmd.h>
#include <qcommon/threads.h>
#include <universal/dvar.h>
#include <universal/memfile.h>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <exception>
#include <cmath>
#include <client/client.h>
#include <cgame_mp/cg_main_mp.h>
#include <cgame_mp/cg_draw_mp.h>
#include <cgame_mp/cg_view_mp.h>
#include <qcommon/dobj_management.h>
#include <xanim/xanim.h>
#include <server_mp/sv_main_mp.h>
#include <server/sv_game.h>
#include <qcommon/net_chan_mp.h>
#include <qcommon/msg.h>
#include <win32/win_shared.h>

namespace {
const dvar_s *probe,*automatic,*exitAfterProbe,*probeCount,*deathTest,*restoreTest,*enabled,*saveCycle,*restoreCount;
unsigned int started;
unsigned int captures;
bool gameplayReady;
bool captureQueued;unsigned mapGeneration;
bool restoreQueued;unsigned restoredAt;unsigned restoreGeneration;
cmd_function_s manualSaveCommand,manualLoadCommand;
cmd_function_s verifyCommand;bool verifyQueued;
cmd_function_s restoreCommand;
cmd_function_s captureCommand;bool commandsRegistered;
void CaptureCheckpointMain_SP();
void RestoreCheckpointMain_SP();
void VerifyCheckpointMain_SP();
void ManualSaveCheckpoint_SP();
void ManualLoadCheckpoint_SP();
// Persists across a full map reload, but never across application launches.
unsigned deathTestStage;
std::string deathTestMap;
std::vector<std::uint8_t> probeImage;
checkpoint::Store store;
checkpoint::Resources resources,preparedResources,committedResources;
std::vector<std::uint8_t> preparedImage,committedImage;
int nextSaveId=1,requestedId=0,preparedId=0,committedId=0;bool requestPending=false,commitRequested=false,lastSaveSuccessful=false;
std::string requestedName;unsigned loadedWallTime=0;unsigned cycleStage=0,cycleTime=0,completedRestores=0;int cycleId=0;float testOrigin[3]{},preparedOrigin[3]{},committedOrigin[3]{};
}
namespace {
void ReportCheckpointSaveFailure_SP(const char* error){
    lastSaveSuccessful=false;requestPending=false;
    Com_Printf(15,"CHECKPOINT_SAVE_ERROR %s\n",error);
    Com_Printf(15,"CHECKPOINT_SAVE_RETAINED {\"id\":%i,\"available\":%s}\n",
        committedId,committedImage.empty()?"false":"true");
}
}
void G_CheckpointInit_SP(){
    enabled=_Dvar_RegisterBool("campaign_checkpoints",true,0,"In-memory solo campaign checkpoints");
    requestPending=false;requestedId=preparedId=committedId=0;lastSaveSuccessful=true;loadedWallTime=0;preparedImage.clear();committedImage.clear();preparedResources={};committedResources={};
    ++mapGeneration;captureQueued=false;restoreQueued=false;restoredAt=0;verifyQueued=false;G_CheckpointClearLiveReferences_SP();
    if(!commandsRegistered){Cmd_AddCommandInternal("checkpoint_capture_main",CaptureCheckpointMain_SP,&captureCommand);Cmd_AddCommandInternal("checkpoint_restore_main",RestoreCheckpointMain_SP,&restoreCommand);Cmd_AddCommandInternal("checkpoint_verify_main",VerifyCheckpointMain_SP,&verifyCommand);Cmd_AddCommandInternal("checkpoint_save",ManualSaveCheckpoint_SP,&manualSaveCommand);Cmd_AddCommandInternal("checkpoint_load",ManualLoadCheckpoint_SP,&manualLoadCommand);commandsRegistered=true;}
    probe=_Dvar_RegisterBool("checkpoint_probe",false,0,"Capture a diagnostic script snapshot at a safe server-frame boundary");
    automatic=_Dvar_RegisterBool("checkpoint_probe_auto",false,0,"Capture diagnostic checkpoints at ten-second gameplay intervals");
    exitAfterProbe=_Dvar_RegisterBool("checkpoint_probe_exit",false,0,"Exit after the opt-in automated checkpoint probe");
    probeCount=_Dvar_RegisterInt("checkpoint_probe_count",1,1,10,0,"Number of automatic checkpoint probes before stopping");
    deathTest=_Dvar_RegisterBool("checkpoint_test_death",false,0,"Automation only: kill the campaign player after capture and verify map restart");
    restoreTest=_Dvar_RegisterBool("checkpoint_test_restore",false,0,"Automation only: restore captured checkpoint after campaign death");
    saveCycle=_Dvar_RegisterBool("checkpoint_test_save_cycle",false,0,"Automation only: verify prepare, commit, death and checkpoint recovery");cycleStage=cycleTime=completedRestores=0;cycleId=0;restoreCount=_Dvar_RegisterInt("checkpoint_test_restore_count",1,1,5,0,"Automation only: number of deaths against the same checkpoint");
    if(!deathTest->current.enabled){deathTestStage=0;deathTestMap.clear();}
    else if(deathTestStage==1){
        if(deathTestMap==Dvar_GetString("mapname")){
            deathTestStage=2;Com_Printf(15,"CHECKPOINT_DEATH {\"stage\":\"map_reinitialized\"}\n");
        }else{
            deathTestStage=3;Com_Printf(15,"CHECKPOINT_DEATH {\"stage\":\"wrong_map\"}\n");Cbuf_AddText(0,"quit\n");
        }
    }
    started=static_cast<unsigned int>(level.time);captures=0;gameplayReady=false;probeImage.clear();store.Reset();resources=checkpoint::Resources{};
}
int G_CheckpointRequest_SP(const char* name,bool commit){
    if(!enabled||!enabled->current.enabled||automatic->current.enabled||probe->current.enabled||requestPending||restoreQueued||zombiemode->current.enabled||Com_IsMenuLevel(Dvar_GetString("mapname"))||Dvar_GetInt("g_reloading")||!g_entities[0].client||g_entities[0].health<=0)return -1;
    if(saveCycle&&saveCycle->current.enabled&&std::strcmp(name?name:"","checkpoint_lifecycle_test"))return -1;
    requestedId=nextSaveId++;requestedName=name&&*name?name:"auto";if(requestedName.size()>128)requestedName.resize(128);requestPending=true;commitRequested=commit;lastSaveSuccessful=false;
    Com_Printf(15,"CHECKPOINT_SAVE {\"stage\":\"requested\",\"id\":%i,\"time\":%i}\n",requestedId,level.time);return requestedId;
}
bool G_CheckpointCommit_SP(int id){
    if(id==committedId&&id>0)return true;
    if(requestPending&&id==requestedId){commitRequested=true;return true;}
    if(id<=0||id!=preparedId)return false;
    std::string error;if(!store.Commit(error))return false;committedImage=preparedImage;committedResources=preparedResources;committedId=id;std::memcpy(committedOrigin,preparedOrigin,sizeof(committedOrigin));
    Com_Printf(15,"CHECKPOINT_SAVE {\"stage\":\"committed\",\"id\":%i,\"time\":%i}\n",id,level.time);
    if(requestedName=="manual")SV_GameSendServerCommand(-1,SV_CMD_RELIABLE,va("g \"%cCheckpoint saved\"",21));return true;
}
bool G_CheckpointSaveSuccessful_SP(){return enabled&&enabled->current.enabled&&lastSaveSuccessful&&!requestPending;}
bool G_CheckpointRecentlyLoaded_SP(){return loadedWallTime&&static_cast<unsigned>(Sys_Milliseconds())-loadedWallTime<5000u;}
bool G_CheckpointTryLoad_SP(){
    if(restoreQueued)return true;
    if(restoreTest&&restoreTest->current.enabled){if(probeImage.empty())return false;}
    else if(!enabled||!enabled->current.enabled||committedImage.empty()){
        Com_Printf(15,"CHECKPOINT_LOAD_UNAVAILABLE reason=%s committed_id=%i\n",
            !enabled||!enabled->current.enabled?"disabled":"no_committed_checkpoint",committedId);
        return false;
    }
    Com_Printf(15,"CHECKPOINT_LOAD_SELECTED id=%i\n",restoreTest&&restoreTest->current.enabled?0:committedId);
    restoreQueued=true;Cbuf_AddText(0,va("checkpoint_restore_main %u\n",mapGeneration));return true;
}
void G_CheckpointFrame_SP(){
    if(restoredAt){
        if(static_cast<unsigned>(level.time)-restoredAt>=5000u){
            if(!verifyQueued){verifyQueued=true;Cbuf_AddText(0,va("checkpoint_verify_main %u\n",mapGeneration));}
        }return;
    }
    if(!probe||(!requestPending&&!probe->current.enabled&&!automatic->current.enabled&&!saveCycle->current.enabled))return;
    if(zombiemode->current.enabled||Com_IsMenuLevel(Dvar_GetString("mapname")))return;
    gentity_s* readyPlayer=nullptr;
    for(int i=0;i<level.maxclients;++i){auto& entity=g_entities[i];
        if(entity.r.inuse&&entity.client&&entity.health>0&&entity.client->sess.connected==CON_CONNECTED&&entity.client->sess.sessionState==SESS_STATE_PLAYING){readyPlayer=&entity;break;}
    }
    // Server simulation can advance while the local client is still loading a
    // replacement map. Start the automation clock only after a real live spawn.
    if(!gameplayReady&&readyPlayer){
        gameplayReady=true;started=static_cast<unsigned>(level.time);
        if(automatic->current.enabled)Com_Printf(15,"CHECKPOINT_READY {\"health\":%i,\"time\":%i}\n",readyPlayer->health,level.time);
    }
    if(saveCycle->current.enabled&&readyPlayer&&gameplayReady){
        if(cycleStage==0&&static_cast<unsigned>(level.time)-started>=10000u){cycleId=G_CheckpointRequest_SP("checkpoint_lifecycle_test",false);if(cycleId>0)cycleStage=1;}
        else if(cycleStage==1&&preparedId==cycleId&&G_CheckpointSaveSuccessful_SP()){cycleStage=2;cycleTime=level.time;}
        else if(cycleStage==2&&static_cast<unsigned>(level.time)-cycleTime>=1000u){if(G_CheckpointCommit_SP(cycleId)){cycleStage=3;cycleTime=level.time;}else{Com_Printf(15,"CHECKPOINT_SAVE_ERROR lifecycle commit failed\n");Cbuf_AddText(0,"quit\n");}}
        else if(cycleStage==3&&static_cast<unsigned>(level.time)-cycleTime>=1000u){cycleStage=4;readyPlayer->client->ps.origin[0]+=128.0f;readyPlayer->r.currentOrigin[0]+=128.0f;Cmd_Kill_f(readyPlayer);Com_Printf(15,"CHECKPOINT_DEATH {\"stage\":\"killed\",\"health\":%i}\n",readyPlayer->health);}
    }
    const bool autoReady=automatic->current.enabled&&readyPlayer&&gameplayReady&&captures<static_cast<unsigned>(probeCount->current.integer)&&static_cast<unsigned int>(level.time)-started>=10000u;
    if(!probe->current.enabled&&!autoReady&&!(requestPending&&readyPlayer))return;
    if(!captureQueued){captureQueued=true;Cbuf_AddText(0,va("checkpoint_capture_main %u\n",mapGeneration));}
}
namespace {
void ManualSaveCheckpoint_SP(){
    Com_SyncThreads();if(!com_sv_running->current.enabled||!Dvar_GetBool("sv_cheats"))return;
    if(G_CheckpointRequest_SP("manual",true)<0)Com_Printf(15,"Checkpoint save unavailable in the current state.\n");
}
void ManualLoadCheckpoint_SP(){
    Com_SyncThreads();if(!com_sv_running->current.enabled||!Dvar_GetBool("sv_cheats"))return;
    if(!G_CheckpointTryLoad_SP())Com_Printf(15,"No committed checkpoint is available.\n");
}
bool HasAnimatedPose_SP(const DObj* object){
    if(!object||!object->tree)return false;
    struct Node {unsigned short index;float weight;};std::vector<Node> pending;
    if(object->tree->children)pending.push_back({object->tree->children,1.0f});
    unsigned visited=0;
    while(!pending.empty()&&++visited<4096){const auto node=pending.back();pending.pop_back();const auto& info=*GetAnimInfo(node.index);
        if(info.next)pending.push_back({info.next,node.weight});
        const float weight=node.weight*info.state.weight;
        if(weight>0.0001f){if(!info.children&&info.animToModel)return true;if(info.children)pending.push_back({info.children,weight});}
    }return false;
}
void VerifyCheckpointMain_SP(){
    if(static_cast<unsigned>(std::strtoul(Cmd_Argv(1),nullptr,10))!=mapGeneration)return;
    Com_SyncThreads();auto& cg=*CG_GetLocalClientGlobals(0);
    const int clientHealth=cg.snap?cg.snap->ps.stats[0]:0;
    unsigned animatedActors=0,unposedActors=0;
    for(int i=0;i<level.num_entities;++i){const auto& ent=g_entities[i];if(!ent.r.inuse||ent.s.eType!=ET_ACTOR)continue;const auto* object=Com_GetClientDObj(i,0);if(!object)continue;
        if(HasAnimatedPose_SP(object))++animatedActors;else if(HasAnimatedPose_SP(Com_GetServerDObj(i)))++unposedActors;
    }
    const bool visible=animatedActors>0&&!unposedActors&&CG_GetScreenFadeState_SP(0).currentAlpha<0.99f&&!Dvar_GetBool("hud_missionFailed")&&Com_GetTimeScale()>0.5f;
    const bool healthy=visible&&g_entities[0].r.inuse&&g_entities[0].client&&g_entities[0].health>0&&Dvar_GetInt("g_reloading")==0&&cg.snap&&clientHealth>0&&cg.time>=static_cast<int>(restoredAt)+3000&&mapGeneration==restoreGeneration;
    Com_Printf(15,"CHECKPOINT_RESTORE {\"stage\":\"continued\",\"ok\":%s,\"time\":%i,\"health\":%i,\"generation\":%u,\"client_time\":%i,\"client_health\":%i,\"fade_alpha\":%.4f,\"time_scale\":%.4f,\"animated_actors\":%u,\"unposed_actors\":%u,\"animation_time\":%i}\n",healthy?"true":"false",level.time,g_entities[0].health,mapGeneration,cg.time,clientHealth,CG_GetScreenFadeState_SP(0).currentAlpha,Com_GetTimeScale(),animatedActors,unposedActors,cg.lastAppliedAnimCommandTime);
    restoredAt=0;verifyQueued=false;++completedRestores;
    if(healthy&&completedRestores<static_cast<unsigned>(restoreCount->current.integer)){
        g_entities[0].client->ps.origin[0]+=128.0f;g_entities[0].r.currentOrigin[0]+=128.0f;Cmd_Kill_f(&g_entities[0]);Com_Printf(15,"CHECKPOINT_DEATH {\"stage\":\"killed\",\"health\":%i}\n",g_entities[0].health);
    }else Cbuf_AddText(0,"quit\n");
}
void RestoreCheckpointMain_SP(){
    if(static_cast<unsigned>(std::strtoul(Cmd_Argv(1),nullptr,10))!=mapGeneration)return;
    restoreQueued=false;Com_SyncThreads();std::string error;checkpoint::Image image;
    checkpoint::Identity expected{checkpoint::MapIdentity(Dvar_GetString("mapname"),cm.checksum),Scr_CheckpointIdentity_SP(SCRIPTINSTANCE_SERVER),2};
    const checkpoint::Section* client=nullptr;unsigned relocated=0;
    const bool snapshotTest=restoreTest->current.enabled;const bool test=snapshotTest||saveCycle->current.enabled;const auto& savedBytes=snapshotTest?probeImage:committedImage;const auto& savedRefs=snapshotTest?resources:committedResources;
    bool valid=checkpoint::Decode(savedBytes,expected,image,error)&&G_CheckpointValidateStaging_SP(image,savedRefs,relocated,error);
    if(valid){for(const auto& section:image.sections)if(section.id==checkpoint::SectionId::Client)client=&section;valid=client&&G_CheckpointCanApplyServer_SP(image,savedRefs,error)&&G_CheckpointValidateClient_SP(client->bytes,error);}
    if(!valid){Com_Printf(15,"CHECKPOINT_RESTORE_ERROR preflight: %s\n",error.c_str());Cbuf_AddText(0,test?"quit\n":"fast_restart\n");return;}
    restoreGeneration=mapGeneration;Com_Printf(15,"CHECKPOINT_RESTORE_PHASE client_retire\n");
    // Discard packets and reliable commands from the abandoned timeline while
    // preserving monotonically increasing transport sequence numbers.
    std::vector<unsigned char> packet(65536);msg_t message{};message.data=packet.data();message.maxsize=static_cast<int>(packet.size());netadr_t from{};
    while(NET_GetLoopPacket(NS_SERVER,&from,&message)){}while(NET_GetLoopPacket(NS_CLIENT1,&from,&message)){}
    auto* connection=CL_GetLocalClientConnection(0);auto& serverClient=svs.clients[0];
    connection->serverCommandSequence=connection->lastExecutedServerCommand=serverClient.reliableSequence;CG_GetLocalClientStaticGlobals(0)->serverCommandSequence=serverClient.reliableSequence;
    serverClient.reliableAcknowledge=serverClient.reliableSent=serverClient.reliableSequence;serverClient.lastClientCommand=connection->reliableSequence;connection->reliableAcknowledge=connection->reliableSequence;
    if(test){
        // Reproduce the retained native death presentation independently of
        // mission timing; healthy snapshots alone cannot catch a black screen.
        CG_FadeScreen_SP(0,0,0,0,255,static_cast<int>(Sys_Milliseconds()),0,false);
        CG_StartTimeScale_SP(0,0,0.1f,0.1f);Com_SetTimeScale(0.1f);Dvar_SetBoolByName("hud_missionFailed",true);
    }
    G_CheckpointRetireClient_SP();
    const bool applied=G_CheckpointApplyServer_SP(image,savedRefs,error)&&G_CheckpointApplyClient_SP(client->bytes,error);
    float originError=0;for(int i=0;i<3;++i)originError+=std::fabs(level.clients[0].ps.origin[i]-(snapshotTest?testOrigin[i]:committedOrigin[i]));
    Com_Printf(15,"CHECKPOINT_RESTORE {\"stage\":\"applied\",\"ok\":%s,\"time\":%i,\"health\":%i,\"generation\":%u,\"origin_error\":%.4f,\"checkpoint_id\":%i}\n",applied?"true":"false",level.time,g_entities[0].health,mapGeneration,originError,snapshotTest?0:committedId);
    if(!applied){Com_Printf(15,"CHECKPOINT_RESTORE_ERROR apply: %s\n",error.c_str());Cbuf_AddText(0,"quit\n");return;}loadedWallTime=static_cast<unsigned>(Sys_Milliseconds());if(test)restoredAt=static_cast<unsigned>(level.time);
}
void CaptureCheckpointMain_SP(){
    if(static_cast<unsigned>(std::strtoul(Cmd_Argv(1),nullptr,10))!=mapGeneration)return;
    captureQueued=false;
    Com_SyncThreads();iassert(Sys_IsMainThread());
    if(!com_sv_running->current.enabled||zombiemode->current.enabled||Com_IsMenuLevel(Dvar_GetString("mapname")))return;
    gentity_s* readyPlayer=nullptr;
    for(int i=0;i<level.maxclients;++i){auto& entity=g_entities[i];if(entity.r.inuse&&entity.client&&entity.health>0&&entity.client->sess.connected==CON_CONNECTED&&entity.client->sess.sessionState==SESS_STATE_PLAYING){readyPlayer=&entity;break;}}
    auto &pub=gScrVarPub[SCRIPTINSTANCE_SERVER];auto &vm=gScrVmPub[SCRIPTINSTANCE_SERVER];
    // Never serialize the VM while a native callback/script invocation is active.
    if(level.initializing||vm.function_count||vm.inparamcount||vm.outparamcount||!pub.levelId||!pub.gameId)return;
    Dvar_SetBool(const_cast<dvar_s*>(probe),false);++captures;started=static_cast<unsigned>(level.time);
    probeImage.clear();if(readyPlayer)for(int i=0;i<3;++i)testOrigin[i]=readyPlayer->client->ps.origin[i];
    bool success=false;
    try{
    constexpr std::size_t capacity=8u*1024u*1024u;
    std::vector<std::uint8_t> bytes(capacity);auto memory=std::make_unique<MemoryFile>();
    MemFile_InitForWriting(memory.get(),static_cast<int>(bytes.size()),bytes.data(),false,false);
    resources=checkpoint::Resources{};
    Scr_PrepareSave(SCRIPTINSTANCE_SERVER);G_CheckpointPrepareRecordRoots_SP(resources);Scr_SaveVariables(SCRIPTINSTANCE_SERVER,memory.get());
    MemFile_StartSegment(memory.get(),-1);
    if(memory->memoryOverflow){
        Com_Printf(15,"CHECKPOINT_PROBE {\"ok\":false,\"reason\":\"script archive overflow\"}\n");
    }else{
        bytes.resize(memory->bytesUsed);checkpoint::Image image;
        const char* map=Dvar_GetString("mapname");
        image.identity.map=checkpoint::MapIdentity(map,cm.checksum);
        image.identity.script=Scr_CheckpointIdentity_SP(SCRIPTINSTANCE_SERVER);image.identity.schema=2;
        if(!Scr_CheckpointSourceCount_SP(SCRIPTINSTANCE_SERVER))throw std::runtime_error("checkpoint source identity is empty");
        image.time=static_cast<unsigned int>(level.time);
        image.sections.push_back({checkpoint::SectionId::Script,std::move(bytes)});
        std::string error;checkpoint::Image decoded;
        CheckpointCaptureStats_SP stats;std::vector<std::uint8_t> entityBytes,playerBytes,actorBytes;
        const bool recordsOk=G_CheckpointCaptureRecords_SP(resources,entityBytes,playerBytes,stats,error);
        if(recordsOk){image.sections.push_back({checkpoint::SectionId::Entities,std::move(entityBytes)});image.sections.push_back({checkpoint::SectionId::Players,std::move(playerBytes)});}
        const bool actorsOk=recordsOk&&G_CheckpointCaptureActors_SP(resources,actorBytes,stats,error);
        if(actorsOk)image.sections.push_back({checkpoint::SectionId::Actors,std::move(actorBytes)});
        CheckpointWorldStats_SP worldStats;std::vector<std::uint8_t> worldBytes;
        const bool worldOk=actorsOk&&G_CheckpointCaptureWorld_SP(resources,worldBytes,worldStats,error);
        if(worldOk)image.sections.push_back({checkpoint::SectionId::World,std::move(worldBytes)});
        CheckpointClientStats_SP clientStats;std::vector<std::uint8_t> clientBytes;
        const bool clientOk=worldOk&&G_CheckpointCaptureClient_SP(clientBytes,clientStats,error);
        if(clientOk)image.sections.push_back({checkpoint::SectionId::Client,std::move(clientBytes)});
        const bool encoded=clientOk&&checkpoint::Encode(image,probeImage,error)&&checkpoint::Decode(probeImage,image.identity,decoded,error);
        unsigned relocated=0;const bool ok=encoded&&G_CheckpointValidateStaging_SP(decoded,resources,relocated,error);
        success=ok;
        bool committed=false;
        if(requestPending){
            const bool prepared=ok&&G_CheckpointCanApplyServer_SP(image,resources,error)&&G_CheckpointValidateClient_SP(image.sections.back().bytes,error)&&store.Prepare(image,error);
            // A failed new candidate never touches the last committed image/resources.
            if(prepared){preparedId=requestedId;preparedImage=probeImage;preparedResources=resources;std::memcpy(preparedOrigin,testOrigin,sizeof(preparedOrigin));lastSaveSuccessful=true;}else lastSaveSuccessful=false;
            requestPending=false;if(prepared&&commitRequested)committed=G_CheckpointCommit_SP(requestedId);
            Com_Printf(15,"CHECKPOINT_SAVE {\"stage\":\"prepared\",\"ok\":%s,\"id\":%i,\"time\":%u}\n",prepared?"true":"false",requestedId,image.time);
            if(!prepared)ReportCheckpointSaveFailure_SP(error.c_str());
        }
        Com_Printf(15,"CHECKPOINT_PROBE {\"ok\":%s,\"restorable\":%s,\"objects\":%u,\"bytes\":%u,\"time\":%u,\"section_mask\":%u,\"entities\":%u,\"players\":%u,\"resources\":%u,\"actors\":%u,\"sentients\":%u,\"tags\":%u,\"scripted_animations\":%u,\"dobjs\":%u,\"animation_nodes\":%u,\"map_identity\":%u,\"script_identity\":%u,\"script_sources\":%u,\"turrets\":%u,\"vehicles\":%u,\"destructibles\":%u,\"path_nodes\":%u,\"physics_bodies\":%u,\"hud_elements\":%u,\"relocated_resources\":%u,\"client_objects\":%u,\"client_bytes\":%u,\"client_identity\":%u}\n",
            ok?"true":"false",committed?"true":"false",pub.savecount,
            static_cast<unsigned int>(probeImage.size()),image.time,checkpoint::Mask(image),stats.entities,stats.players,static_cast<unsigned int>(resources.entries.size()),stats.actors,stats.sentients,stats.tags,stats.scripted,stats.dobjs,stats.animationNodes,image.identity.map,image.identity.script,Scr_CheckpointSourceCount_SP(SCRIPTINSTANCE_SERVER),worldStats.turrets,worldStats.vehicles,worldStats.destructibles,worldStats.nodes,worldStats.bodies,worldStats.hud,relocated,clientStats.objects,clientStats.bytes,clientStats.identity);
        if(!ok)Com_Printf(15,"CHECKPOINT_RECORD_ERROR %s\n",error.c_str());
    }
    }catch(const std::exception& error){
        Com_Printf(15,"CHECKPOINT_PROBE {\"ok\":false,\"reason\":\"native capture exception\"}\n");
        Com_Printf(15,"CHECKPOINT_RECORD_ERROR %s\n",error.what());
    }
    if(requestPending)ReportCheckpointSaveFailure_SP("capture did not produce a prepared checkpoint");
    if((deathTest->current.enabled||restoreTest->current.enabled)&&success){
        if(deathTestStage==0){
            gentity_s* player=readyPlayer;
            if(player&&Dvar_GetBool("sv_cheats")){
                deathTestMap=Dvar_GetString("mapname");if(restoreTest->current.enabled){player->client->ps.origin[0]+=128.0f;player->r.currentOrigin[0]+=128.0f;}Cmd_Kill_f(player);
                if(player->health<=0){
                    deathTestStage=1;Com_Printf(15,"CHECKPOINT_DEATH {\"stage\":\"killed\",\"health\":%i}\n",player->health);return;
                }
            }
            Com_Printf(15,"CHECKPOINT_DEATH {\"stage\":\"kill_failed\"}\n");Cbuf_AddText(0,"quit\n");return;
        }
        if(deathTestStage==2){
            deathTestStage=3;
            const int health=g_entities[0].r.inuse&&g_entities[0].client?g_entities[0].health:0;
            Com_Printf(15,"CHECKPOINT_DEATH {\"stage\":\"recovered\",\"health\":%i,\"reloading\":%i}\n",health,Dvar_GetInt("g_reloading"));
        }
    }
    if((deathTest->current.enabled||restoreTest->current.enabled)&&deathTestStage==1)return;
    if(exitAfterProbe->current.enabled&&(!automatic->current.enabled||captures>=static_cast<unsigned>(probeCount->current.integer)))Cbuf_AddText(0,"quit\n");
}
}
#endif
