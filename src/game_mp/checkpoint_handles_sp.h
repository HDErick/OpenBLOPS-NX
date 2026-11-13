#pragma once
#ifdef KISAK_SP
#include <game/enthandle.h>
#include <game/sentient.h>
#include "g_main_mp.h"
#include <stdexcept>
struct CheckpointHandleBinder_SP {
    template<class T>void Field(const char*,T&){}
    template<class T>void Ref(const char*,T*&,checkpoint::ResourceKind){}
    template<class T>void StringId(const char*,T&){}
    template<class T>void ScriptObject(const char*,T&){}
    void Text(const char*,const char*&){}
    void Opaque(const char*,std::intptr_t&){}
    void ScalarUnion(const char*,void*,std::size_t){}
    void Handle(const char*,EntHandle& handle){const auto number=handle.number;handle.number=handle.infoIndex=0;
        if(number){if(number>MAX_GENTITIES||!g_entities[number-1].r.inuse)throw std::runtime_error("checkpoint handle targets an inactive entity");handle.setEnt(&g_entities[number-1]);}}
    void Handle(const char*,SentientHandle& handle){const auto number=handle.number;handle.number=handle.infoIndex=0;
        if(number){if(number>MAX_SENTIENTS||!level.sentients[number-1].inuse)throw std::runtime_error("checkpoint handle targets an inactive sentient");handle.setSentient(&level.sentients[number-1]);}}
};
#endif
