#include "g_checkpoint_records_sp.h"
#ifdef KISAK_SP
#include "g_main_mp.h"
#include <memory>
#include <exception>
#include <algorithm>
#include "g_scr_main_mp.h"
#include "checkpoint_animation.h"
#include "checkpoint_handles_sp.h"
#include <qcommon/dobj_management.h>
#include <xanim/dobj.h>
#include <xanim/dobj_utils.h>
#include <xanim/xanim.h>
#include <array>
#include <game/actor.h>
#include <game/sentient.h>
#include <clientscript/cscr_save_write.h>
#include <clientscript/cscr_stringlist.h>
namespace {
template<class V>void EntityFields(V& v,gentity_s& ent){
#include "checkpoint_entity_fields.inc"
    // All alternatives of this gameplay union contain only scalar values.
    // Its audited size is identical on x86 and AArch64; no owner pointers live here.
    static_assert(sizeof(ent.missile)==108,"audit changed entity union before checkpointing");
    v.ScalarUnion("scalar_union",&ent.missile,sizeof(ent.missile));
    v.Ref("client",ent.client,checkpoint::ResourceKind::Client);
    v.Ref("actor",ent.actor,checkpoint::ResourceKind::Actor);
    v.Ref("sentient",ent.sentient,checkpoint::ResourceKind::Sentient);
    v.Ref("turret",ent.pTurretInfo,checkpoint::ResourceKind::Turret);
    v.Ref("destructible",ent.destructible,checkpoint::ResourceKind::Destructible);
    v.Ref("vehicle",ent.scr_vehicle,checkpoint::ResourceKind::Vehicle);
    v.Handle("parent",ent.parent);v.Handle("owner",ent.r.ownerNum);v.Handle("missile_target",ent.missileTargetEnt);
    v.Ref("chain",ent.chain,checkpoint::ResourceKind::Entity);
    v.Ref("activator",ent.activator,checkpoint::ResourceKind::Entity);
    v.Ref("children",ent.tagChildren,checkpoint::ResourceKind::Entity);
    v.Ref("next_free",ent.nextFree,checkpoint::ResourceKind::Entity);
    v.Ref("animation",ent.pAnimTree,checkpoint::ResourceKind::AnimationTree);
    v.Ref("tag_info",ent.tagInfo,checkpoint::ResourceKind::TagInfo);
    v.Ref("scripted_animation",ent.scriptedAnim_SP,checkpoint::ResourceKind::ScriptedAnimation);
    v.Opaque("physics",ent.physObjId);
    v.Field("model",ent.model);v.StringId("classname",ent.classname);v.StringId("target",ent.target);
    v.StringId("targetname",ent.targetname);v.StringId("script_noteworthy",ent.script_noteworthy);v.StringId("script_linkname",ent.script_linkname);
    v.Field("attach_models",ent.attachModelNames);for(auto& tag:ent.attachTagNames)v.StringId("attach_tag",tag);
    v.Field("no_baseline",ent.noBaseline_SP);
    for(auto& damage:ent.flame_timed_damage){
        v.Ref("flame_attacker",damage.attacker,checkpoint::ResourceKind::Entity);
        v.Field("flame_damage",damage.damage);v.Field("flame_duration",damage.damageDuration);v.Field("flame_interval",damage.damageInterval);
        v.Field("flame_start",damage.start_timestamp);v.Field("flame_end",damage.end_timestamp);v.Field("flame_update",damage.lastupdate_timestamp);
    }
}
template<class V>void ActorFields(V& v,actor_s& actor){
#include "checkpoint_actor_fields.inc"
    v.Ref("ent",actor.ent,checkpoint::ResourceKind::Entity);
    v.Ref("sentient",actor.sentient,checkpoint::ResourceKind::Sentient);
    v.ScriptObject("animation_thread",actor.AnimScriptHandle);
    v.Ref("anim_script",actor.pAnimScriptFunc,checkpoint::ResourceKind::AnimationScript);
    v.Ref("grenade_cover_animation",actor.grenadeCoverAnim,checkpoint::ResourceKind::AnimationScript);
    v.StringId("specific_animation_name",actor.AnimScriptSpecific.name);
    v.Field("engage_min",actor.engageMinDist);v.Field("engage_min_falloff",actor.engageMinFalloffDist);
    v.Field("engage_max",actor.engageMaxDist);v.Field("engage_max_falloff",actor.engageMaxFalloffDist);
    v.Field("model",actor.state.modelindex);v.Field("attach_models",actor.state.attachModelIndex);
    for(auto& node:actor.pPotentialReacquireNode)v.Ref("reacquire_node",node,checkpoint::ResourceKind::PathNode);
    for(auto& info:actor.sentientInfo)v.Ref("last_known_node",info.pLastKnownNode,checkpoint::ResourceKind::PathNode);
    for(auto& suppression:actor.suppression)v.Ref("suppressor",suppression.suppressor,checkpoint::ResourceKind::Sentient);
    v.StringId("damage_hit_location",actor.damageHitLoc);v.StringId("damage_weapon",actor.damageWeapon);v.StringId("damage_mod",actor.damageMod);
    v.StringId("weapon",actor.weaponName);v.StringId("primary_weapon",actor.primaryWeaponName);
    v.StringId("secondary_weapon",actor.secondaryWeaponName);v.StringId("sidearm",actor.sidearmName);
    v.StringId("grenade_toss_method",actor.GrenadeTossMethod);v.StringId("script_state",actor.scriptState);
    v.StringId("last_script_state",actor.lastScriptState);v.StringId("state_change_reason",actor.stateChangeReason);v.StringId("pose",actor.animPose);
    v.Handle("favorite_enemy",actor.pFavoriteEnemy);v.Handle("grenade",actor.pGrenade);v.Handle("close_entity",actor.pCloseEnt);
    v.Handle("goal_entity",actor.scriptGoalEnt);v.Handle("safe_volume",actor.fixedNodeSafeVolume);v.Handle("grenade_notice",actor.grenadeFleeNotice);
    v.Ref("turret",actor.turretEnt,checkpoint::ResourceKind::Entity);v.Ref("pileup_entity",actor.pPileUpEnt,checkpoint::ResourceKind::Entity);
    v.Ref("pileup_actor",actor.pPileUpActor,checkpoint::ResourceKind::Actor);
    v.Ref("code_goal_volume",actor.codeGoal.volume,checkpoint::ResourceKind::Entity);v.Ref("script_goal_volume",actor.scriptGoal.volume,checkpoint::ResourceKind::Entity);
    v.Ref("code_goal_node",actor.codeGoal.node,checkpoint::ResourceKind::PathNode);v.Ref("script_goal_node",actor.scriptGoal.node,checkpoint::ResourceKind::PathNode);
    v.Ref("chain_node",actor.chainNode,checkpoint::ResourceKind::PathNode);v.Ref("enemy_path_node",actor.faceLikelyEnemyPathNode,checkpoint::ResourceKind::PathNode);
    if(actor.iPotentialCoverNodeCount<0||actor.iPotentialCoverNodeCount>1000)throw std::runtime_error("invalid actor cover node count");
    for(int i=0;i<actor.iPotentialCoverNodeCount;++i)v.Ref("cover_node",actor.pPotentialCoverNode[i],checkpoint::ResourceKind::PathNode);
    // Physics scratch/proximity caches and debug text have no persistent owner.
    // They must be rebuilt by physics setup during eventual world restoration.
}
template<class V>void SentientFields(V& v,sentient_s& sentient){
    v.Ref("ent",sentient.ent,checkpoint::ResourceKind::Entity);v.Field("team",sentient.eTeam);
    v.Handle("script_owner",sentient.scriptOwner);v.Field("threat_bias",sentient.iThreatBias);v.Field("threat_group",sentient.iThreatBiasGroupIndex);
    v.Field("ignore_me",sentient.bIgnoreMe);v.Field("ignore_all",sentient.bIgnoreAll);v.Field("ignore_friendly_fire",sentient.bIgnoreForFriendlyFire);
    v.Field("origin_changed",sentient.originChanged);v.Field("old_origin",sentient.oldOrigin);v.Field("max_visible_distance",sentient.maxVisibleDist);
    v.Field("enemy_notify_time",sentient.iEnemyNotifyTime);v.Field("attacker_count",sentient.attackerCount);
    v.Handle("last_attacker",sentient.lastAttacker);v.Handle("synced_melee",sentient.syncedMeleeEnt);v.Handle("target",sentient.targetEnt);v.Handle("script_target",sentient.scriptTargetEnt);
    v.StringId("script_target_tag",sentient.scriptTargetTag);v.Field("target_threat",sentient.entityTargetThreat);v.Field("melee_spots",sentient.meleeAttackerSpot);
    v.Field("attacker_accuracy",sentient.attackerAccuracy);v.Field("ignore_random_damage",sentient.ignoreRandomBulletDamage);v.Field("turret_invulnerability",sentient.turretInvulnerability);
    v.Ref("claimed_node",sentient.pClaimedNode,checkpoint::ResourceKind::PathNode);v.Ref("previous_claimed_node",sentient.pPrevClaimedNode,checkpoint::ResourceKind::PathNode);
    v.Ref("chain_position",sentient.pActualChainPos,checkpoint::ResourceKind::PathNode);v.Field("chain_position_time",sentient.iActualChainPosTime);
    v.Ref("nearest_node",sentient.pNearestNode,checkpoint::ResourceKind::PathNode);v.Field("nearest_valid",sentient.bNearestNodeValid);v.Field("nearest_bad",sentient.bNearestNodeBad);
    v.Field("node_check_position",sentient.vNearestNodeCheckPos);v.Field("inuse",sentient.inuse);v.Field("ban_node_time",sentient.banNodeTime);v.Field("melee_charge",sentient.bInMeleeCharge);
}
template<class V>void TagFields(V& v,tagInfo_s& tag){
    v.Ref("parent",tag.parent,checkpoint::ResourceKind::Entity);v.Ref("next",tag.next,checkpoint::ResourceKind::Entity);
    v.StringId("name",tag.name);v.Field("index",tag.index);v.Field("axis",tag.axis);v.Field("parent_inverse_axis",tag.parentInvAxis);
}
template<class V>void ScriptedAnimationFields(V& v,ScriptedAnimRecord_SP& animation){
    v.Field("base_axis",animation.baseAxis);v.Field("origin_offset",animation.originOffset);v.Field("angle_offset",animation.angleOffset);
    v.Field("animation",animation.animIndex);v.Field("root",animation.rootIndex);v.Field("updated_once",animation.updatedOnce);v.Field("mode",animation.mode);
    v.Field("start_time",animation.startTime);v.Field("death_plant",animation.deathPlant);
}
template<class V>void PlayerFields(V& v,gclient_s& client){
#include "checkpoint_player_fields.inc"
    v.Ref("lean_entity",client.leanEntity,checkpoint::ResourceKind::Entity);
    v.Handle("use_hold_entity",client.useHoldEntity);
    // Connection/session command state is covered explicitly: the WIP fixed
    // payload leaves weapon indices in these commands to a separate helper.
    v.Field("cmd_weapon",client.sess.cmd.weapon);v.Field("cmd_offhand",client.sess.cmd.offHandIndex);
    v.Field("oldcmd_weapon",client.sess.oldcmd.weapon);v.Field("oldcmd_offhand",client.sess.oldcmd.offHandIndex);
}
}
void G_CheckpointWriteEntity_SP(gentity_s& ent,checkpoint::Resources& refs,std::vector<std::uint8_t>& bytes){bytes.clear();checkpoint::FieldWriter writer(bytes,refs);EntityFields(writer,ent);}
void G_CheckpointReadEntity_SP(gentity_s& ent,checkpoint::Resources& refs,const std::vector<std::uint8_t>& bytes){checkpoint::FieldReader reader(bytes,refs);EntityFields(reader,ent);reader.Finish();}
void G_CheckpointWritePlayer_SP(gclient_s& client,checkpoint::Resources& refs,std::vector<std::uint8_t>& bytes){bytes.clear();checkpoint::FieldWriter writer(bytes,refs);PlayerFields(writer,client);}
void G_CheckpointReadPlayer_SP(gclient_s& client,checkpoint::Resources& refs,const std::vector<std::uint8_t>& bytes){checkpoint::FieldReader reader(bytes,refs);PlayerFields(reader,client);reader.Finish();}
void G_CheckpointReadActor_SP(actor_s& x,checkpoint::Resources& refs,const std::vector<std::uint8_t>& bytes){checkpoint::FieldReader reader(bytes,refs);ActorFields(reader,x);reader.Finish();}
void G_CheckpointReadSentient_SP(sentient_s& x,checkpoint::Resources& refs,const std::vector<std::uint8_t>& bytes){checkpoint::FieldReader reader(bytes,refs);SentientFields(reader,x);reader.Finish();}
void G_CheckpointReadTag_SP(tagInfo_s& x,checkpoint::Resources& refs,const std::vector<std::uint8_t>& bytes){checkpoint::FieldReader reader(bytes,refs);TagFields(reader,x);reader.Finish();}
void G_CheckpointReadScriptedAnimation_SP(ScriptedAnimRecord_SP& x,checkpoint::Resources& refs,const std::vector<std::uint8_t>& bytes){checkpoint::FieldReader reader(bytes,refs);ScriptedAnimationFields(reader,x);reader.Finish();}
namespace {
void AppendWord(std::vector<std::uint8_t>& out,std::uint32_t n){for(int i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
void AppendRecord(std::vector<std::uint8_t>& out,unsigned id,const std::vector<std::uint8_t>& record){
    if(record.size()>checkpoint::kMaxBytes-8||out.size()>checkpoint::kMaxBytes-8-record.size())throw std::runtime_error("checkpoint record section too large");
    AppendWord(out,id);AppendWord(out,static_cast<std::uint32_t>(record.size()));out.insert(out.end(),record.begin(),record.end());
}
void FinishRecords(std::vector<std::uint8_t>& out,unsigned count){for(int i=0;i<4;++i)out[4+i]=static_cast<std::uint8_t>(count>>(i*8));}
}
void G_CheckpointBindEntityHandles_SP(gentity_s& x){CheckpointHandleBinder_SP binder;EntityFields(binder,x);}
void G_CheckpointBindPlayerHandles_SP(gclient_s& x){CheckpointHandleBinder_SP binder;PlayerFields(binder,x);}
void G_CheckpointBindActorHandles_SP(actor_s& x){CheckpointHandleBinder_SP binder;ActorFields(binder,x);}
void G_CheckpointBindSentientHandles_SP(sentient_s& x){CheckpointHandleBinder_SP binder;SentientFields(binder,x);}
void G_CheckpointPrepareRecordRoots_SP(checkpoint::Resources& refs){
    refs.stringName=[](std::uint16_t id){return SL_ConvertToString(id,SCRIPTINSTANCE_SERVER);};
    refs.findString=[](const char* text){return static_cast<std::uint16_t>(SL_FindString(text,SCRIPTINSTANCE_SERVER));};
    auto bind=[&](unsigned id){if(id>=0x7ffe)throw std::runtime_error("invalid native script root");if(id)refs.BindScript(static_cast<unsigned short>(id),Scr_WriteSaveId(static_cast<unsigned short>(id),SCRIPTINSTANCE_SERVER));};
    for(int i=0;i<level.num_entities;++i)if(g_entities[i].r.inuse&&g_entities[i].client)bind(g_entities[i].client->sess.scriptPersId);
    if(level.actors)for(int i=0;i<MAX_ACTORS;++i)if(level.actors[i].inuse)bind(level.actors[i].AnimScriptHandle);
}
bool G_CheckpointCaptureActors_SP(checkpoint::Resources& refs,std::vector<std::uint8_t>& bytes,
    CheckpointCaptureStats_SP& stats,std::string& error){
    auto& actors=stats.actors;auto& sentients=stats.sentients;actors=sentients=0;bytes.clear();
    std::string context;
    try{
        AppendWord(bytes,1);AppendWord(bytes,0);AppendWord(bytes,0);
        auto actor=std::make_unique<actor_s>();auto sentient=std::make_unique<sentient_s>();
        std::vector<std::uint8_t> saved,again;
        auto capture=[&](auto& source,auto& staging,auto visitor,unsigned index,checkpoint::ResourceKind kind){
            saved.clear();checkpoint::FieldWriter writer(saved,refs);visitor(writer,source);
            if(checkpoint::verifyRoundTrip){checkpoint::FieldReader reader(saved,refs);visitor(reader,staging);reader.Finish();
            again.clear();checkpoint::FieldWriter rewriter(again,refs);visitor(rewriter,staging);
            if(saved!=again)throw std::runtime_error("AI record changed during round-trip");}
            AppendWord(bytes,static_cast<unsigned>(kind));AppendWord(bytes,refs.Intern(kind,&source));AppendRecord(bytes,index,saved);
        };
        if(level.actors)for(int i=0;i<MAX_ACTORS;++i)if(level.actors[i].inuse){
            context="actor "+std::to_string(i);capture(level.actors[i],*actor,[](auto& io,actor_s& x){ActorFields(io,x);},i,checkpoint::ResourceKind::Actor);++actors;
        }
        if(level.sentients)for(int i=0;i<MAX_SENTIENTS;++i)if(level.sentients[i].inuse){
            context="sentient "+std::to_string(i);capture(level.sentients[i],*sentient,[](auto& io,sentient_s& x){SentientFields(io,x);},i,checkpoint::ResourceKind::Sentient);++sentients;
        }
        FinishRecords(bytes,actors);for(int i=0;i<4;++i)bytes[8+i]=static_cast<std::uint8_t>(sentients>>(i*8));return true;
    }catch(const std::exception& e){error=context+": "+e.what();bytes.clear();return false;}
}
bool G_CheckpointCaptureRecords_SP(checkpoint::Resources& refs,std::vector<std::uint8_t>& entityBytes,
    std::vector<std::uint8_t>& playerBytes,CheckpointCaptureStats_SP& stats,std::string& error){
    auto& entities=stats.entities;auto& players=stats.players;auto& tags=stats.tags;auto& scripted=stats.scripted;
    entities=players=tags=scripted=stats.dobjs=stats.animationNodes=0;entityBytes.clear();playerBytes.clear();
    std::string context;
    try{
        if(level.num_entities<0||level.num_entities>MAX_GENTITIES)throw std::runtime_error("invalid live entity count");
        AppendWord(entityBytes,3);AppendWord(entityBytes,0);AppendWord(entityBytes,0);AppendWord(entityBytes,0);AppendWord(entityBytes,0);
        AppendWord(playerBytes,2);AppendWord(playerBytes,0);
        std::vector<tagInfo_s*> tagObjects;std::vector<ScriptedAnimRecord_SP*> animations;
        std::vector<std::uint8_t> saved,again;
        auto entity=std::make_unique<gentity_s>();auto client=std::make_unique<gclient_s>();
        for(int i=0;i<MAX_GENTITIES;++i){auto& source=g_entities[i];if(i>=level.num_entities&&!source.r.inuse)continue;
            context="entity "+std::to_string(i);G_CheckpointWriteEntity_SP(source,refs,saved);
            if(checkpoint::verifyRoundTrip){G_CheckpointReadEntity_SP(*entity,refs,saved);G_CheckpointWriteEntity_SP(*entity,refs,again);
            if(saved!=again)throw std::runtime_error("entity record changed during round-trip");}AppendWord(entityBytes,refs.Intern(checkpoint::ResourceKind::Entity,&source));AppendRecord(entityBytes,i,saved);++entities;
            if(source.tagInfo&&std::find(tagObjects.begin(),tagObjects.end(),source.tagInfo)==tagObjects.end())tagObjects.push_back(source.tagInfo);
            if(source.scriptedAnim_SP&&std::find(animations.begin(),animations.end(),source.scriptedAnim_SP)==animations.end())animations.push_back(source.scriptedAnim_SP);
            if(source.r.inuse&&source.client){context="player "+std::to_string(i);G_CheckpointWritePlayer_SP(*source.client,refs,saved);
                if(checkpoint::verifyRoundTrip){G_CheckpointReadPlayer_SP(*client,refs,saved);G_CheckpointWritePlayer_SP(*client,refs,again);
                if(saved!=again)throw std::runtime_error("player record changed during round-trip");}AppendWord(playerBytes,refs.Intern(checkpoint::ResourceKind::Client,source.client));AppendRecord(playerBytes,i,saved);++players;}
        }
        auto captureAttachment=[&](auto& source,auto visitor,unsigned index,checkpoint::ResourceKind kind){
            using T=std::remove_reference_t<decltype(source)>;
            saved.clear();checkpoint::FieldWriter writer(saved,refs);visitor(writer,source);
            if(checkpoint::verifyRoundTrip){T staging{};checkpoint::FieldReader reader(saved,refs);visitor(reader,staging);reader.Finish();
            again.clear();checkpoint::FieldWriter rewriter(again,refs);visitor(rewriter,staging);
            if(saved!=again)throw std::runtime_error("attachment record changed during round-trip");}
            AppendWord(entityBytes,refs.Intern(kind,&source));AppendRecord(entityBytes,index,saved);
        };
        for(auto* tag:tagObjects){context="tag attachment "+std::to_string(tags);captureAttachment(*tag,[](auto& io,tagInfo_s& x){TagFields(io,x);},tags,checkpoint::ResourceKind::TagInfo);++tags;}
        for(auto* animation:animations){context="scripted animation "+std::to_string(scripted);captureAttachment(*animation,[](auto& io,ScriptedAnimRecord_SP& x){ScriptedAnimationFields(io,x);},scripted,checkpoint::ResourceKind::ScriptedAnimation);++scripted;}
        for(int i=0;i<level.num_entities;++i){
            if(!g_entities[i].r.inuse)continue;DObj* object=Com_GetServerDObj(i);if(!object)continue;
            context="animation tree entity "+std::to_string(i);checkpoint::AnimationTree tree;DObjGetHidePartBits(object,tree.hiddenParts);
            if(object->tree){
                for(unsigned user=0;user<2;++user)for(unsigned index=1;index<MAX_XANIMTREE_NUM;++index)if(gScrAnimPub[SCRIPTINSTANCE_SERVER].xanim_lookup[user][index].anims==object->tree->anims){tree.published=static_cast<unsigned short>(index);tree.publishedUser=static_cast<unsigned char>(user);}
                tree.present=true;tree.asset=object->tree->anims->debugName;tree.assetSize=object->tree->anims->size;tree.instance=object->tree->inst;
                struct Pending {unsigned short index;std::uint32_t parent;};
                std::vector<Pending> pending;std::array<bool,4096> visited{};
                if(object->tree->children)pending.push_back({object->tree->children,0});
                while(!pending.empty()){
                    const auto next=pending.back();pending.pop_back();
                    if(!next.index||next.index>=visited.size()||visited[next.index])throw std::runtime_error("invalid live animation graph");
                    visited[next.index]=true;const auto& info=*GetAnimInfo(next.index);checkpoint::AnimationNode node;
                    node.savedInfo=static_cast<unsigned short>(next.index);node.parent=next.parent;node.animation=info.animIndex;node.notifyChild=info.notifyChild;node.notifyType=info.notifyType;node.notifyIndex=info.notifyIndex;
                    node.notifyName=info.notifyName?SL_ConvertToString(info.notifyName,SCRIPTINSTANCE_SERVER):nullptr;
                    node.streamedParts=info.streamedParts?info.streamedParts->name:nullptr;
                    node.time=info.state.currentAnimTime;node.oldTime=info.state.oldTime;node.cycle=info.state.cycleCount;node.oldCycle=info.state.oldCycleCount;
                    node.goalTime=info.state.goalTime;node.goalWeight=info.state.goalWeight;node.weight=info.state.weight;node.rate=info.state.rate;node.instant=info.state.instantWeightChange;
                    tree.nodes.push_back(node);
                    if(info.next)pending.push_back({info.next,next.parent});
                    if(info.children)pending.push_back({info.children,static_cast<std::uint32_t>(tree.nodes.size())});
                }
            }
            saved.clear();checkpoint::FieldWriter writer(saved,refs);checkpoint::AnimationFields(writer,tree);
            if(checkpoint::verifyRoundTrip){checkpoint::AnimationTree staging;checkpoint::FieldReader reader(saved,refs);checkpoint::AnimationFields(reader,staging);reader.Finish();
            again.clear();checkpoint::FieldWriter rewriter(again,refs);checkpoint::AnimationFields(rewriter,staging);
            if(saved!=again)throw std::runtime_error("animation tree changed during round-trip");}
            AppendWord(entityBytes,refs.Intern(checkpoint::ResourceKind::AnimationTree,object->tree));AppendRecord(entityBytes,i,saved);
            ++stats.dobjs;stats.animationNodes+=static_cast<unsigned>(tree.nodes.size());
        }
        FinishRecords(entityBytes,entities);FinishRecords(playerBytes,players);
        for(int i=0;i<4;++i){entityBytes[8+i]=static_cast<std::uint8_t>(tags>>(i*8));entityBytes[12+i]=static_cast<std::uint8_t>(scripted>>(i*8));entityBytes[16+i]=static_cast<std::uint8_t>(stats.dobjs>>(i*8));}
        // No setters, pool teardown or native resource ownership changes occur:
        // all decoded records above are inert staging objects, not live entities.
        return true;
    }catch(const std::exception& e){error=context+": "+e.what();entityBytes.clear();playerBytes.clear();return false;}
}
#endif
