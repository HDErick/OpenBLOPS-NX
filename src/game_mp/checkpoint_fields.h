#pragma once
#include "checkpoint_archive.h"
#include <cstring>
#include <functional>
#include <deque>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace checkpoint {
enum class ResourceKind : std::uint32_t { Entity,Client,Actor,Sentient,Turret,Destructible,Vehicle,AnimationTree,TagInfo,ScriptedAnimation,Physics,PathNode,AnimationScript,CustomVehiclePath,NitrousVehicle };
// Context is deliberately NOT serialized. Full restore must rebuild this ID
// catalog from owning subsystem sections before applying records. A probe only
// resolves IDs against its still-live source world, never mutates that world.
struct Resources {
    struct Entry { ResourceKind kind;const void* pointer; };
    std::vector<Entry> entries;
    std::deque<std::string> text;
    std::function<const char*(std::uint16_t)> stringName;
    std::function<std::uint16_t(const char*)> findString;
    struct ScriptReference { std::uint16_t nativeId,archiveId; };
    std::vector<ScriptReference> scriptReferences;
    std::function<std::uint16_t(const char*,std::uint16_t)> scriptBinder;
    void BindScript(std::uint16_t nativeId,std::uint16_t archiveId) {
        if(!nativeId||!archiveId)throw std::runtime_error("invalid checkpoint script binding");
        for(auto& ref:scriptReferences)if(ref.nativeId==nativeId||ref.archiveId==archiveId){
            if(ref.nativeId!=nativeId||ref.archiveId!=archiveId)throw std::runtime_error("conflicting checkpoint script binding");return;
        }
        scriptReferences.push_back({nativeId,archiveId});
    }
    std::uint16_t EncodeScript(std::uint16_t nativeId)const {
        if(!nativeId)return 0;
        for(auto& ref:scriptReferences)if(ref.nativeId==nativeId)return ref.archiveId;
        throw std::runtime_error("unprepared checkpoint script root");
    }
    std::uint16_t DecodeScript(std::uint16_t archiveId)const {
        if(!archiveId)return 0;
        for(auto& ref:scriptReferences)if(ref.archiveId==archiveId)return ref.nativeId;
        throw std::runtime_error("missing checkpoint script root");
    }
    std::uint32_t Intern(ResourceKind kind,const void* p) {
        if(!p)return 0;
        for(std::size_t i=0;i<entries.size();++i)if(entries[i].kind==kind&&entries[i].pointer==p)return static_cast<std::uint32_t>(i+1);
        if(entries.size()>=65535)throw std::runtime_error("checkpoint resource catalog full");
        entries.push_back({kind,p});return static_cast<std::uint32_t>(entries.size());
    }
    void Declare(const std::vector<ResourceKind>& kinds){entries.clear();for(auto kind:kinds)entries.push_back({kind,nullptr});}
    void Bind(std::uint32_t id,ResourceKind kind,const void* pointer){
        if(!id||id>entries.size()||entries[id-1].kind!=kind||!pointer||entries[id-1].pointer)throw std::runtime_error("invalid/duplicate checkpoint resource binding");
        entries[id-1].pointer=pointer;
    }
    // Replace only an inert physics token after collision/model reconstruction.
    // Other resource kinds bind once and may never silently change owners.
    void BindRebuiltPhysics(std::uint32_t id,const void* token,const void* body){
        if(!id||id>entries.size()||entries[id-1].kind!=ResourceKind::Physics||!token||!body||entries[id-1].pointer!=token||token==body)
            throw std::runtime_error("invalid rebuilt checkpoint physics binding");
        entries[id-1].pointer=body;
    }
    const void* Resolve(ResourceKind kind,std::uint32_t id)const {
        if(!id)return nullptr;
        if(id>entries.size()||entries[id-1].kind!=kind||!entries[id-1].pointer)throw std::runtime_error("checkpoint resource reference mismatch");
        return entries[id-1].pointer;
    }
    const char* OwnText(std::string value) { text.push_back(std::move(value));return text.back().c_str(); }
};
class FieldWriter {
    std::vector<std::uint8_t>& bytes_;Resources& resources_;
    void Word(std::uint64_t n,unsigned size){if(bytes_.size()>kMaxBytes-size)throw std::runtime_error("checkpoint record too large");for(unsigned i=0;i<size;++i)bytes_.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
    void Tag(const char* name,unsigned type){Word(Crc32(name,std::strlen(name)),4);Word(type,1);}
public:
    std::size_t fields=0;
    FieldWriter(std::vector<std::uint8_t>& bytes,Resources& refs):bytes_(bytes),resources_(refs){}
    template<class T>void Field(const char* name,const T& value){
        static_assert(std::is_arithmetic_v<T>||std::is_enum_v<T>,"Native pointer/aggregate requires an explicit checkpoint codec");
        ++fields;
        if constexpr(std::is_enum_v<T>){using U=std::underlying_type_t<T>;Tag(name,0x40|sizeof(U));Word(static_cast<std::make_unsigned_t<U>>(value),sizeof(U));}
        else if constexpr(std::is_same_v<T,bool>){
            // Inspect the representation before evaluating a possibly invalid
            // native bool. Optimizers may assume a bool already contains 0/1.
            static_assert(sizeof(bool)==1);std::uint8_t raw;std::memcpy(&raw,&value,1);
            if(raw>1)throw std::runtime_error(std::string("invalid native checkpoint boolean: ")+name+"="+std::to_string(raw));
            Tag(name,0x31);Word(raw,1);
        }
        else if constexpr(std::is_floating_point_v<T>){static_assert(sizeof(T)==4||sizeof(T)==8);Tag(name,0x20|sizeof(T));std::uint64_t bits=0;std::memcpy(&bits,&value,sizeof(T));Word(bits,sizeof(T));}
        else {Tag(name,(std::is_signed_v<T>?0x10:0)|sizeof(T));Word(static_cast<std::make_unsigned_t<T>>(value),sizeof(T));}
    }
    template<class T,std::size_t N>void Field(const char* name,const T(&value)[N]){for(const auto& x:value)Field(name,x);}
    template<class T>void Ref(const char* name,T* value,ResourceKind kind){Tag(name,0x50);Word(static_cast<unsigned>(kind),4);Word(resources_.Intern(kind,value),4);++fields;}
    template<class H>void Handle(const char* name,const H& h){Field(name,h.number);}
    void Opaque(const char* name,std::intptr_t value){std::uint8_t state=value==-1?2:value?1:0;Field(name,state);if(state==1)Ref(name,reinterpret_cast<const void*>(value),ResourceKind::Physics);}
    void Text(const char* name,const char* value){
        Tag(name,0x60);++fields;if(!value){Word(0xffffffffu,4);return;}
        const auto size=std::strlen(value);if(size>4096)throw std::runtime_error("checkpoint objective text too long");Word(size,4);
        for(std::size_t i=0;i<size;++i)Word(static_cast<unsigned char>(value[i]),1);
    }
    void StringId(const char* name,std::uint16_t id){
        if(id&&!resources_.stringName)throw std::runtime_error("checkpoint string resolver missing");
        const char* text=id?resources_.stringName(id):nullptr;
        if(id&&!text)throw std::runtime_error("checkpoint string ID not found");Text(name,text);
    }
    template<class T>void ScriptObject(const char* name,T id){
        static_assert(std::is_integral_v<T>);if(id<0||id>=0x7ffe)throw std::runtime_error("invalid checkpoint script object");
        Field(name,resources_.EncodeScript(static_cast<std::uint16_t>(id)));
    }
    void ScalarUnion(const char* name,const void* data,std::size_t size){Tag(name,0x61);Word(size,4);auto p=static_cast<const std::uint8_t*>(data);for(std::size_t i=0;i<size;++i)Word(p[i],1);++fields;}
};
class FieldReader {
    const std::vector<std::uint8_t>& bytes_;Resources& resources_;std::size_t cursor_=0;
    std::uint64_t Word(unsigned size){if(size>bytes_.size()-cursor_)throw std::runtime_error("truncated checkpoint record");std::uint64_t n=0;for(unsigned i=0;i<size;++i)n|=std::uint64_t(bytes_[cursor_++])<<(i*8);return n;}
    void Tag(const char* name,unsigned type){if(Word(4)!=Crc32(name,std::strlen(name))||Word(1)!=type)throw std::runtime_error(std::string("checkpoint field schema mismatch: ")+name);}
public:
    std::size_t fields=0;
    FieldReader(const std::vector<std::uint8_t>& bytes,Resources& refs):bytes_(bytes),resources_(refs){}
    template<class T>void Field(const char* name,T& value){
        static_assert(std::is_arithmetic_v<T>||std::is_enum_v<T>,"Native pointer/aggregate requires an explicit checkpoint codec");
        ++fields;
        if constexpr(std::is_enum_v<T>){using U=std::underlying_type_t<T>;Tag(name,0x40|sizeof(U));auto n=static_cast<std::make_unsigned_t<U>>(Word(sizeof(U)));U v;std::memcpy(&v,&n,sizeof(v));value=static_cast<T>(v);}
        else if constexpr(std::is_same_v<T,bool>){Tag(name,0x31);auto n=Word(1);if(n>1)throw std::runtime_error(std::string("invalid checkpoint boolean: ")+name+"="+std::to_string(n));value=n!=0;}
        else if constexpr(std::is_floating_point_v<T>){Tag(name,0x20|sizeof(T));auto n=Word(sizeof(T));std::memcpy(&value,&n,sizeof(T));}
        else {Tag(name,(std::is_signed_v<T>?0x10:0)|sizeof(T));auto n=static_cast<std::make_unsigned_t<T>>(Word(sizeof(T)));std::memcpy(&value,&n,sizeof(T));}
    }
    template<class T,std::size_t N>void Field(const char* name,T(&value)[N]){for(auto& x:value)Field(name,x);}
    template<class T>void Ref(const char* name,T*& value,ResourceKind kind){Tag(name,0x50);if(Word(4)!=static_cast<unsigned>(kind))throw std::runtime_error(std::string("checkpoint resource kind mismatch: ")+name);value=static_cast<T*>(const_cast<void*>(resources_.Resolve(kind,static_cast<std::uint32_t>(Word(4)))));++fields;}
    // Staging records have no live handle registration. Final world rehydration
    // must use setEnt/setSentient; copying infoIndex would corrupt handle lists.
    template<class H>void Handle(const char* name,H& h){Field(name,h.number);if(h.number>1023)throw std::runtime_error("checkpoint entity handle out of range");h.infoIndex=0;}
    void Opaque(const char* name,std::intptr_t& value){std::uint8_t state=0;Field(name,state);if(state==2)value=-1;else if(state==0)value=0;else if(state==1){const void* p=nullptr;Ref(name,p,ResourceKind::Physics);value=reinterpret_cast<std::intptr_t>(p);}else throw std::runtime_error("invalid physics reference");}
    void Text(const char* name,const char*& value){Tag(name,0x60);++fields;auto size=Word(4);if(size==0xffffffffu){value=nullptr;return;}if(size>4096||size>bytes_.size()-cursor_)throw std::runtime_error("invalid checkpoint text length");std::string text;for(unsigned i=0;i<size;++i)text.push_back(static_cast<char>(Word(1)));value=resources_.OwnText(std::move(text));}
    void StringId(const char* name,std::uint16_t& id){
        const char* text=nullptr;Text(name,text);id=0;if(!text)return;
        if(!resources_.findString)throw std::runtime_error("checkpoint string binder missing");
        id=resources_.findString(text);if(!id)throw std::runtime_error("checkpoint string not bound");
    }
    template<class T>void ScriptObject(const char* name,T& id){static_assert(std::is_integral_v<T>);std::uint16_t saved=0;Field(name,saved);id=resources_.scriptBinder?resources_.scriptBinder(name,saved):resources_.DecodeScript(saved);}
    void ScalarUnion(const char* name,void* data,std::size_t size){Tag(name,0x61);if(Word(4)!=size)throw std::runtime_error("checkpoint scalar union size mismatch");auto p=static_cast<std::uint8_t*>(data);for(std::size_t i=0;i<size;++i)p[i]=static_cast<std::uint8_t>(Word(1));++fields;}
    void Finish()const{if(cursor_!=bytes_.size())throw std::runtime_error("checkpoint record trailing data");}
};
}
