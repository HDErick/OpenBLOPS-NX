#pragma once
#include "checkpoint_fields.h"
#include <cmath>

namespace checkpoint {
// State is the engine BodyState in production. The archive carries scalars,
// never body, shape, solver-group or contact-cache addresses.
template<class State>struct EntityPhysicsState {
    State body{};
    float gravity[3]={},linearDrag=0,angularDrag=0,maxAngularVelocity=0,stableTime=0;
    float eventTime=0,reevalTime=0;
    int flags=0,buoyantTime=0,rippleTime=0,references=1,constraints=0;
    std::uint16_t trajectoryDuration=0;
    bool stable=false,vehicle=false;
};
template<class State>void ValidateEntityPhysics(const EntityPhysicsState<State>& x){
    auto finite=[](float f){if(!std::isfinite(f))throw std::runtime_error("checkpoint physics contains non-finite state");};
    for(unsigned i=0;i<3;++i){finite(x.body.position[i]);finite(x.body.velocity[i]);finite(x.body.angVelocity[i]);finite(x.body.centerOfMassOffset[i]);finite(x.body.buoyancyBoxMin[i]);finite(x.body.buoyancyBoxMax[i]);finite(x.gravity[i]);
        for(unsigned j=0;j<3;++j)finite(x.body.rotation[i][j]);
        if(std::fabs(x.body.position[i])>=100000.0f||std::fabs(x.body.velocity[i])>=100000.0f||std::fabs(x.body.angVelocity[i])>=1000.0f)throw std::runtime_error("checkpoint physics exceeds solver bounds");
    }
    for(float f:{x.body.mass,x.body.friction,x.body.bounce,x.linearDrag,x.angularDrag,x.maxAngularVelocity,x.stableTime,x.eventTime,x.reevalTime})finite(f);
    if(x.body.mass<=0||x.body.id<0||x.body.id>=1024)throw std::runtime_error("checkpoint physics has invalid mass or entity ID");
    if(x.references!=1)throw std::runtime_error("shared entity physics bodies are not supported yet");
    if(x.constraints)throw std::runtime_error("constrained entity physics bodies are not supported yet");
    if(x.vehicle)throw std::runtime_error("vehicle physics must use the vehicle checkpoint owner");
}
template<class IO,class State>void EntityPhysicsFields(IO& v,EntityPhysicsState<State>& x){
    unsigned version=1;v.Field("entity_physics_version",version);if(version!=1)throw std::runtime_error("entity physics checkpoint version mismatch");
    auto& b=x.body;
    v.Field("position",b.position);v.Field("rotation",b.rotation);v.Field("velocity",b.velocity);v.Field("angular_velocity",b.angVelocity);
    v.Field("center_of_mass",b.centerOfMassOffset);v.Field("buoyancy_min",b.buoyancyBoxMin);v.Field("buoyancy_max",b.buoyancyBoxMax);
    v.Field("mass",b.mass);v.Field("friction",b.friction);v.Field("bounce",b.bounce);v.Field("asleep_time",b.timeLastAsleep);v.Field("entity",b.id);v.Field("buoyancy",b.buoyancy);v.Field("underwater",b.underwater);
    v.Field("gravity",x.gravity);v.Field("linear_drag",x.linearDrag);v.Field("angular_drag",x.angularDrag);v.Field("max_angular_velocity",x.maxAngularVelocity);v.Field("stable_time",x.stableTime);v.Field("stable",x.stable);
    v.Field("event_time",x.eventTime);v.Field("reevaluation_time",x.reevalTime);v.Field("collision_flags",x.flags);v.Field("buoyant_time",x.buoyantTime);v.Field("ripple_time",x.rippleTime);v.Field("trajectory_duration",x.trajectoryDuration);
    v.Field("references",x.references);v.Field("constraints",x.constraints);v.Field("vehicle",x.vehicle);
    ValidateEntityPhysics(x);
}
}
