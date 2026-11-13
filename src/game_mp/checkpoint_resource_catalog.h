#pragma once
#include "checkpoint_fields.h"
namespace checkpoint {
enum class ResourceOwner : std::uint32_t {Entity,Client,Actor,Sentient,Turret,Destructible,Vehicle,AnimationTree,TagInfo,ScriptedAnimation,PhysicsEntity,PathNode,SpecificAnimation,GlobalAnimation,CustomVehiclePath,NitrousVehicle};
struct ResourceDescriptor {ResourceKind kind{};ResourceOwner owner{};std::uint32_t slot=0,auxiliary=0;};
inline void ValidateResourceDescriptor(const ResourceDescriptor& x){
    ResourceKind expected;unsigned limit=0,auxLimit=1;
    switch(x.owner){
    case ResourceOwner::Entity:expected=ResourceKind::Entity;limit=1024;break;
    case ResourceOwner::Client:expected=ResourceKind::Client;limit=32;break;
    case ResourceOwner::Actor:expected=ResourceKind::Actor;limit=32;break;
    case ResourceOwner::Sentient:expected=ResourceKind::Sentient;limit=48;break;
    case ResourceOwner::Turret:expected=ResourceKind::Turret;limit=96;break;
    case ResourceOwner::Destructible:expected=ResourceKind::Destructible;limit=256;break;
    case ResourceOwner::Vehicle:expected=ResourceKind::Vehicle;limit=64;break;
    case ResourceOwner::AnimationTree:expected=ResourceKind::AnimationTree;limit=1024;auxLimit=33;break;
    case ResourceOwner::TagInfo:expected=ResourceKind::TagInfo;limit=1024;break;
    case ResourceOwner::ScriptedAnimation:expected=ResourceKind::ScriptedAnimation;limit=1024;break;
    case ResourceOwner::PhysicsEntity:expected=ResourceKind::Physics;limit=1024;break;
    case ResourceOwner::PathNode:expected=ResourceKind::PathNode;limit=65535;break;
    case ResourceOwner::SpecificAnimation:expected=ResourceKind::AnimationScript;limit=32;break;
    case ResourceOwner::GlobalAnimation:expected=ResourceKind::AnimationScript;limit=2073;auxLimit=4;break;
    case ResourceOwner::CustomVehiclePath:expected=ResourceKind::CustomVehiclePath;limit=10;break;
    case ResourceOwner::NitrousVehicle:expected=ResourceKind::NitrousVehicle;limit=64;break;
    default:throw std::runtime_error("unknown checkpoint resource owner");
    }
    if(x.kind!=expected||x.slot>=limit||x.auxiliary>=auxLimit)throw std::runtime_error("invalid checkpoint resource owner/kind/slot");
}
template<class IO>void ResourceCatalogFields(IO& io,std::vector<ResourceDescriptor>& catalog){
    unsigned count=static_cast<unsigned>(catalog.size());io.Field("resources",count);if(count>65535)throw std::runtime_error("checkpoint resource catalog too large");catalog.resize(count);
    for(auto& entry:catalog){io.Field("kind",entry.kind);io.Field("owner",entry.owner);io.Field("slot",entry.slot);io.Field("auxiliary",entry.auxiliary);ValidateResourceDescriptor(entry);}
}
}
