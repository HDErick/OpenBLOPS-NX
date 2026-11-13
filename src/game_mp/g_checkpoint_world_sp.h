#pragma once
#ifdef KISAK_SP
#include "checkpoint_fields.h"
#include "checkpoint_record_set.h"
#include "checkpoint_resource_catalog.h"
#include <memory>
struct CheckpointWorldStats_SP { unsigned turrets=0,vehicles=0,destructibles=0,bodies=0,nodes=0,hud=0; };
struct level_locals_t;
struct gentity_s;
class CheckpointWorldStage_SP {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    explicit CheckpointWorldStage_SP(unsigned nodeCount);
    ~CheckpointWorldStage_SP();
    void* Pool(checkpoint::ResourceOwner owner,unsigned slot);
    level_locals_t& Level();
    void Read(const std::vector<checkpoint::NativeRecord>& records,checkpoint::Resources& refs);
};
void G_CheckpointApplyWorld_SP(const std::vector<checkpoint::NativeRecord>& records,checkpoint::Resources& refs);
void G_CheckpointBindWorldHandles_SP();
void G_CheckpointValidatePhysics_SP(const std::vector<checkpoint::NativeRecord>& records,const std::vector<checkpoint::ResourceDescriptor>& catalog,const gentity_s* entities,const checkpoint::Resources& refs);
void G_CheckpointRebuildPhysics_SP(const std::vector<checkpoint::NativeRecord>& records,const std::vector<checkpoint::ResourceDescriptor>& catalog,checkpoint::Resources& refs);
void G_CheckpointRebuildVehicles_SP(const std::vector<checkpoint::NativeRecord>& records,checkpoint::Resources& refs);
void* G_CheckpointLiveWorldPool_SP(checkpoint::ResourceOwner owner,unsigned slot);
bool G_CheckpointCaptureWorld_SP(checkpoint::Resources& refs,std::vector<std::uint8_t>& bytes,CheckpointWorldStats_SP& stats,std::string& error);
#endif
