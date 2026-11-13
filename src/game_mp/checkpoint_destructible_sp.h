#pragma once
#ifdef KISAK_SP
#include "checkpoint_fields.h"
#include <physics/destructible.h>
namespace checkpoint {
struct SavedDestructible_SP {
    Destructible value{};const char* definition=nullptr;std::vector<DESTRUCTIBLE_PIECE_INFO> pieces;
};
template<class V>void DestructibleFields(V& v,SavedDestructible_SP& x){
    v.Text("definition",x.definition);v.Field("entity",x.value.entNum);v.Field("burn_time",x.value.oldestBurnTime);v.Field("pose",x.value.destructiblePoseID);
    v.Field("flags",x.value.flags);v.Field("hit",x.value.bHasBeenHit);
    for(auto& state:x.value.states){v.Field("time",state.time);v.Field("model0",state.state.modelState0);v.Field("model1",state.state.modelState1);
        v.Field("model2",state.state.modelState2);v.Field("model3",state.state.modelState3);v.Field("model4",state.state.modelState4);v.Field("dummy",state.state.dummy);}
    unsigned count=static_cast<unsigned>(x.pieces.size());v.Field("piece_count",count);if(count>4096)throw std::runtime_error("checkpoint destructible has too many pieces");x.pieces.resize(count);
    for(auto& piece:x.pieces){v.Field("health",piece.health);v.Field("ragdoll",piece.xdollHandle);v.Field("effect",piece.fx);v.Field("burn_time",piece.burnData.burnTime);v.Field("burn_effect",piece.burnData.fx);v.Field("burn_sound",piece.burnData.sndId);}
}
}
#endif
