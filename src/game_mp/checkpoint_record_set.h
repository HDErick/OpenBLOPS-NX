#pragma once
#include "checkpoint_archive.h"
#include <set>
#include <stdexcept>
namespace checkpoint {
struct NativeRecord {std::uint32_t group=0,resource=0,slot=0;std::vector<std::uint8_t> bytes;};
class RecordInput {
    const std::vector<std::uint8_t>& data_;std::size_t at_=0;
public:
    explicit RecordInput(const std::vector<std::uint8_t>& data):data_(data){if(data.size()>kMaxBytes)throw std::runtime_error("record section exceeds limit");}
    bool Empty()const{return at_==data_.size();}
    std::uint32_t Word(){if(data_.size()-at_<4)throw std::runtime_error("truncated record header");std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(data_[at_++])<<(8*i);return n;}
    std::vector<std::uint8_t> Blob(){auto count=Word();if(count>data_.size()-at_)throw std::runtime_error("truncated native record");std::vector<std::uint8_t> bytes(data_.begin()+at_,data_.begin()+at_+count);at_+=count;return bytes;}
};
inline std::vector<NativeRecord> ParseNativeRecords(SectionId section,const std::vector<std::uint8_t>& data){
    RecordInput in(data);std::vector<NativeRecord> records;std::set<std::pair<unsigned,unsigned>> slots;
    auto append=[&](NativeRecord record,unsigned limit,bool ownerRequired){
        if(record.slot>=limit||(ownerRequired&&!record.resource)||record.resource>65535||!slots.insert({record.group,record.slot}).second)throw std::runtime_error("invalid/duplicate native record owner");
        records.push_back(std::move(record));
    };
    const auto version=in.Word();
    if(section==SectionId::Entities||section==SectionId::Players){
        if(version!=(section==SectionId::Entities?3u:2u))throw std::runtime_error("native pool schema mismatch");
        const auto count=in.Word();unsigned tags=0,animations=0,objects=0;
        if(section==SectionId::Entities){tags=in.Word();animations=in.Word();objects=in.Word();}
        if(count>(section==SectionId::Entities?1024u:32u)||tags>1024||animations>1024||objects>1024)throw std::runtime_error("native pool count exceeds limit");
        for(unsigned group=0;group<4;++group){const auto n=group==0?count:group==1?tags:group==2?animations:objects;
            for(unsigned i=0;i<n;++i){NativeRecord record;record.group=group;record.resource=in.Word();record.slot=in.Word();record.bytes=in.Blob();append(std::move(record),section==SectionId::Players?32u:1024u,group!=3);}
        }
    }else if(section==SectionId::Actors){
        if(version!=1)throw std::runtime_error("AI pool schema mismatch");auto actors=in.Word(),sentients=in.Word();
        if(actors>32||sentients>48)throw std::runtime_error("AI pool count exceeds limit");
        for(unsigned i=0;i<actors+sentients;++i){NativeRecord record;record.group=in.Word();record.resource=in.Word();record.slot=in.Word();record.bytes=in.Blob();
            if(record.group!=(i<actors?2u:3u))throw std::runtime_error("AI pool record type mismatch");append(std::move(record),i<actors?32u:48u,true);}
    }else if(section==SectionId::World){
        if(version!=1)throw std::runtime_error("world schema mismatch");constexpr unsigned limits[]={1,96,64,1024,256,65535,10,65535,1,1,64,1,32,1024};
        while(!in.Empty()){NativeRecord record;record.group=in.Word();record.slot=in.Word();record.resource=in.Word();record.bytes=in.Blob();
            if(record.group>=sizeof(limits)/sizeof(limits[0]))throw std::runtime_error("unknown world record type");append(std::move(record),limits[record.group],false);}
        if(!slots.count({0,0})||!slots.count({8,0}))throw std::runtime_error("world level/catalog record missing");
    }else throw std::runtime_error("section is not a native record pool");
    if(!in.Empty())throw std::runtime_error("trailing native pool bytes");return records;
}
}
