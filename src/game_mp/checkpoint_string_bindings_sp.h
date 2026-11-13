#pragma once
#ifdef KISAK_SP
#include <clientscript/cscr_stringlist.h>
#include <functional>
#include <memory>
#include <vector>
// Preflight can encounter a saved dynamic name that the current timeline has
// already released. Intern it temporarily without transferring native ownership.
struct CheckpointStringPins_SP {
    scriptInstance_t instance;std::vector<unsigned short> ids;
    explicit CheckpointStringPins_SP(scriptInstance_t value):instance(value){}
    ~CheckpointStringPins_SP(){for(auto id:ids)SL_RemoveRefToString(instance,id);}
    unsigned short Find(const char* name){auto id=static_cast<unsigned short>(SL_GetString_(instance,name,0,15));ids.push_back(id);return id;}
};
inline std::function<unsigned short(const char*)> CheckpointTemporaryStrings_SP(scriptInstance_t instance){
    auto pins=std::make_shared<CheckpointStringPins_SP>(instance);return [pins](const char* name){return pins->Find(name);};
}
#endif
