#pragma once
#include "checkpoint_archive.h"
#include <cstring>
#include <initializer_list>
namespace checkpoint {
inline std::uint32_t IdentityWords(std::initializer_list<std::uint32_t> words){
    std::vector<std::uint8_t> bytes;bytes.reserve(words.size()*4);
    for(auto word:words)for(int i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(word>>(8*i)));
    return Crc32(bytes.data(),bytes.size());
}
inline std::uint32_t MapIdentity(const char* name,std::uint32_t collisionChecksum){
    return IdentityWords({Crc32(name,std::strlen(name)),collisionChecksum});
}
class SourceIdentity {
    std::uint32_t digest_=0,count_=0;
public:
    void Reset(){digest_=count_=0;}
    void Record(const char* filename,const char* source,int size){
        // Missing optional files are not compiled and do not contribute.
        if(!source||size<0)return;
        digest_=IdentityWords({digest_,Crc32(filename,std::strlen(filename)),static_cast<std::uint32_t>(size),Crc32(source,static_cast<std::size_t>(size))});++count_;
    }
    std::uint32_t Count()const{return count_;}
    std::uint32_t Finish(std::uint32_t compilerChecksum,std::uint32_t programBytes,std::uint32_t pointerBytes)const{
        // Code-position offsets depend on the compiled bytecode ABI. Equal
        // source on x86 and AArch64 is not sufficient to reuse those offsets.
        return IdentityWords({2,digest_,count_,compilerChecksum,programBytes,pointerBytes});
    }
};
}
