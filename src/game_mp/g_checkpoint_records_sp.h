#pragma once
#ifdef KISAK_SP
#include "checkpoint_fields.h"
struct gentity_s;struct gclient_s;
struct CheckpointCaptureStats_SP {
    unsigned entities=0,players=0,actors=0,sentients=0,tags=0,scripted=0,dobjs=0,animationNodes=0;
};
void G_CheckpointWriteEntity_SP(gentity_s& entity,checkpoint::Resources& refs,std::vector<std::uint8_t>& bytes);
void G_CheckpointReadEntity_SP(gentity_s& entity,checkpoint::Resources& refs,const std::vector<std::uint8_t>& bytes);
void G_CheckpointWritePlayer_SP(gclient_s& client,checkpoint::Resources& refs,std::vector<std::uint8_t>& bytes);
void G_CheckpointReadPlayer_SP(gclient_s& client,checkpoint::Resources& refs,const std::vector<std::uint8_t>& bytes);
struct actor_s;struct sentient_s;struct tagInfo_s;struct ScriptedAnimRecord_SP;
void G_CheckpointReadActor_SP(actor_s&,checkpoint::Resources&,const std::vector<std::uint8_t>&);
void G_CheckpointReadSentient_SP(sentient_s&,checkpoint::Resources&,const std::vector<std::uint8_t>&);
void G_CheckpointReadTag_SP(tagInfo_s&,checkpoint::Resources&,const std::vector<std::uint8_t>&);
void G_CheckpointReadScriptedAnimation_SP(ScriptedAnimRecord_SP&,checkpoint::Resources&,const std::vector<std::uint8_t>&);
void G_CheckpointBindEntityHandles_SP(gentity_s&);
void G_CheckpointBindPlayerHandles_SP(gclient_s&);
void G_CheckpointBindActorHandles_SP(actor_s&);
void G_CheckpointBindSentientHandles_SP(sentient_s&);
void G_CheckpointPrepareRecordRoots_SP(checkpoint::Resources& refs);
bool G_CheckpointCaptureActors_SP(checkpoint::Resources& refs,std::vector<std::uint8_t>& bytes,
    CheckpointCaptureStats_SP& stats,std::string& error);
bool G_CheckpointCaptureRecords_SP(checkpoint::Resources& refs,std::vector<std::uint8_t>& entityBytes,
    std::vector<std::uint8_t>& playerBytes,CheckpointCaptureStats_SP& stats,std::string& error);
#endif
