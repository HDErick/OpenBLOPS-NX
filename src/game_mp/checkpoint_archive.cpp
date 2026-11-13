#include "checkpoint_archive.h"
#include <cstring>
#include <utility>
namespace checkpoint {
namespace {
constexpr std::size_t Header=48;
void put(std::vector<std::uint8_t>& out,std::uint32_t n){for(int i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
std::uint32_t get(const std::vector<std::uint8_t>& b,std::size_t offset){std::uint32_t n=0;for(int i=0;i<4;++i)n|=std::uint32_t(b[offset+i])<<(i*8);return n;}
bool fail(std::string& error,const char* reason){error=reason;return false;}
}
std::uint32_t Crc32(const void* data,std::size_t bytes){
    auto p=static_cast<const std::uint8_t*>(data);std::uint32_t crc=~0u;
    for(std::size_t i=0;i<bytes;++i){crc^=p[i];for(int bit=0;bit<8;++bit)crc=(crc>>1)^((0u-(crc&1u))&0xedb88320u);}
    return ~crc;
}
std::uint32_t Mask(const Image& image){std::uint32_t mask=0;for(const auto& s:image.sections){auto id=static_cast<std::uint32_t>(s.id);if(id<6)mask|=1u<<id;}return mask;}
bool Encode(const Image& image,std::vector<std::uint8_t>& out,std::string& error){
    error.clear();if(image.sections.empty()||image.sections.size()>6)return fail(error,"invalid section count");
    std::vector<std::uint8_t> payload;std::uint32_t mask=0;
    for(const auto& s:image.sections){
        auto id=static_cast<std::uint32_t>(s.id);
        if(id>=6||(mask&(1u<<id)))return fail(error,"invalid or duplicate section");
        if(s.bytes.empty())return fail(error,"empty section");
        if(s.bytes.size()>kMaxBytes-Header-8||payload.size()>kMaxBytes-Header-8-s.bytes.size())return fail(error,"checkpoint size limit");
        mask|=1u<<id;put(payload,id);put(payload,static_cast<std::uint32_t>(s.bytes.size()));payload.insert(payload.end(),s.bytes.begin(),s.bytes.end());
    }
    std::vector<std::uint8_t> b={'K','B','C','K','P','T','0','1'};
    put(b,kVersion);put(b,static_cast<std::uint32_t>(Header+payload.size()));
    put(b,image.identity.map);put(b,image.identity.script);put(b,image.identity.schema);put(b,image.time);
    put(b,mask);put(b,static_cast<std::uint32_t>(image.sections.size()));put(b,Crc32(payload.data(),payload.size()));
    put(b,Crc32(b.data(),b.size()));b.insert(b.end(),payload.begin(),payload.end());out=std::move(b);return true;
}
bool Decode(const std::vector<std::uint8_t>& b,const Identity& expected,Image& out,std::string& error){
    error.clear();if(b.size()<Header||b.size()>kMaxBytes)return fail(error,"invalid checkpoint length");
    if(std::memcmp(b.data(),"KBCKPT01",8)||get(b,8)!=kVersion)return fail(error,"incompatible checkpoint version");
    if(get(b,12)!=b.size()||get(b,44)!=Crc32(b.data(),44)||get(b,40)!=Crc32(b.data()+Header,b.size()-Header))return fail(error,"checkpoint checksum/length mismatch");
    Image result;result.identity={get(b,16),get(b,20),get(b,24)};result.time=get(b,28);
    if(!(result.identity==expected))return fail(error,"checkpoint map/script/schema mismatch");
    const auto count=get(b,36);if(!count||count>6)return fail(error,"invalid section count");
    std::size_t cursor=Header;std::uint32_t mask=0;
    for(std::uint32_t i=0;i<count;++i){
        if(b.size()-cursor<8)return fail(error,"truncated section header");
        auto id=get(b,cursor),size=get(b,cursor+4);cursor+=8;
        if(id>=6||(mask&(1u<<id)))return fail(error,"invalid or duplicate section");
        if(!size||size>b.size()-cursor)return fail(error,"invalid section length");
        mask|=1u<<id;Section section{static_cast<SectionId>(id),{}};
        section.bytes.assign(b.begin()+cursor,b.begin()+cursor+size);cursor+=size;result.sections.push_back(std::move(section));
    }
    if(cursor!=b.size()||mask!=get(b,32))return fail(error,"checkpoint section mask/trailing data mismatch");
    out=std::move(result);return true;
}
bool Store::Prepare(const Image& image,std::string& error){
    prepared_.clear();if(Mask(image)!=kAllSections)return fail(error,"checkpoint incomplete: gameplay restore sections missing");
    if(!Encode(image,prepared_,error))return false;preparedIdentity_=image.identity;return true;
}
bool Store::Commit(std::string& error){
    if(prepared_.empty())return fail(error,"no prepared checkpoint");
    Image verify;if(!Decode(prepared_,preparedIdentity_,verify,error)){prepared_.clear();return false;}
    committed_.swap(prepared_);prepared_.clear();error.clear();return true;
}
bool Store::Read(const Identity& id,Image& out,std::string& error)const{
    if(committed_.empty())return fail(error,"no committed checkpoint");return Decode(committed_,id,out,error);
}
}
