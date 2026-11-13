#include <game_mp/checkpoint_archive.h>
#include <cassert>
#include <cstdio>
#include <random>
using namespace checkpoint;
void word(std::vector<std::uint8_t>& b,size_t offset,std::uint32_t n){for(int i=0;i<4;++i)b[offset+i]=static_cast<std::uint8_t>(n>>(8*i));}
void checksum(std::vector<std::uint8_t>& b){word(b,40,Crc32(b.data()+48,b.size()-48));word(b,44,Crc32(b.data(),44));}
int main(){
 assert(Crc32("123456789",9)==0xcbf43926u);
 Image image;image.identity={11,22,1};image.time=123456;
 for(unsigned id=0;id<6;++id)image.sections.push_back({static_cast<SectionId>(id),{0,1,2,static_cast<std::uint8_t>(id),255}});
 std::vector<std::uint8_t> bytes;std::string error;assert(Encode(image,bytes,error));Image out;
 assert(Decode(bytes,image.identity,out,error));assert(out.time==image.time&&out.sections.size()==6);
 for(unsigned i=0;i<6;++i)assert(out.sections[i].bytes==image.sections[i].bytes);
 // Every single-byte corruption and every truncation must be rejected.
 for(size_t i=0;i<bytes.size();++i){auto bad=bytes;bad[i]^=0x80;assert(!Decode(bad,image.identity,out,error));}
 for(size_t n=0;n<bytes.size();++n){auto bad=bytes;bad.resize(n);assert(!Decode(bad,image.identity,out,error));}
 Identity wrong=image.identity;++wrong.map;assert(!Decode(bytes,wrong,out,error));wrong=image.identity;++wrong.script;assert(!Decode(bytes,wrong,out,error));wrong=image.identity;++wrong.schema;assert(!Decode(bytes,wrong,out,error));
 // Structurally invalid records still fail with valid checksums.
 auto bad=bytes;word(bad,48+13,0);checksum(bad);assert(!Decode(bad,image.identity,out,error)); // duplicate ID
 bad=bytes;word(bad,52,0xffffffffu);checksum(bad);assert(!Decode(bad,image.identity,out,error));
 bad=bytes;word(bad,36,7);checksum(bad);assert(!Decode(bad,image.identity,out,error));
 bad=bytes;word(bad,32,1);checksum(bad);assert(!Decode(bad,image.identity,out,error));
 // Commit is separate; a failed/incomplete new save retains the old checkpoint.
 Store store;assert(!store.HasCommitted());assert(!store.Commit(error));
 assert(store.Prepare(image,error));assert(!store.HasCommitted());assert(store.Commit(error));assert(store.HasCommitted());
 auto incomplete=image;incomplete.sections.pop_back();assert(!store.Prepare(incomplete,error));assert(!store.Commit(error));
 assert(store.Read(image.identity,out,error)&&out.time==123456);
 auto duplicate=image;duplicate.sections[1].id=SectionId::Script;std::vector<std::uint8_t> sentinel={7};assert(!Encode(duplicate,sentinel,error)&&sentinel==std::vector<std::uint8_t>{7});
 auto oversized=image;oversized.sections[0].bytes.resize(kMaxBytes);assert(!store.Prepare(oversized,error));assert(store.Read(image.identity,out,error)&&out.time==123456);
 image.time=222;assert(store.Prepare(image,error)&&store.Commit(error));assert(store.Read(image.identity,out,error)&&out.time==222);
 // Decoder does not mutate the destination on failure.
 auto preserved=out;assert(!Decode(bad,image.identity,out,error));assert(out.time==preserved.time);
 std::mt19937 random(47);for(int trial=0;trial<2000;++trial){std::vector<std::uint8_t> junk(random()%300);for(auto& b:junk)b=static_cast<std::uint8_t>(random());assert(!Decode(junk,image.identity,out,error));}
 store.Reset();assert(!store.Read(image.identity,out,error));
 puts("PASS: checkpoint CRC/version/identity, every-byte corruption, truncation, bounds, fuzz and last-good commit");
}
