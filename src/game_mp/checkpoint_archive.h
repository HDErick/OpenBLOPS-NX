#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Project-owned, little-endian checkpoint container. Never archive native pointers.
namespace checkpoint {
constexpr std::uint32_t kVersion=1;
constexpr std::uint32_t kAllSections=0x3f;
constexpr std::size_t kMaxBytes=16u*1024u*1024u;
enum class SectionId : std::uint32_t { Script=0,Entities=1,Players=2,Actors=3,World=4,Client=5 };
struct Identity {
    std::uint32_t map=0,script=0,schema=1;
    bool operator==(const Identity& b)const{return map==b.map&&script==b.script&&schema==b.schema;}
};
struct Section { SectionId id;std::vector<std::uint8_t> bytes; };
struct Image { Identity identity;std::uint32_t time=0;std::vector<Section> sections; };
std::uint32_t Crc32(const void* data,std::size_t bytes);
std::uint32_t Mask(const Image& image);
bool Encode(const Image& image,std::vector<std::uint8_t>& out,std::string& error);
bool Decode(const std::vector<std::uint8_t>& bytes,const Identity& expected,Image& out,std::string& error);
// Save-time self checks: every record is read back and rewritten, and the
// whole image is decoded and staged, before a save is accepted. Restore runs
// the same staging checks before applying, so these only move a failure
// earlier. Off for normal play (they were most of an autosave's hitch);
// checkpoint_verify and the checkpoint test modes turn them on.
inline bool verifyRoundTrip=true;
class Store {
    std::vector<std::uint8_t> committed_,prepared_;
    Identity preparedIdentity_;
public:
    // Failed/incomplete candidates never replace the last committed checkpoint.
    bool Prepare(const Image& image,std::string& error);
    // Takes an image already encoded (and checked) by the caller.
    bool PrepareEncoded(std::vector<std::uint8_t> bytes,const Identity& identity,std::string& error);
    bool Commit(std::string& error,bool verify=true);
    bool Read(const Identity& identity,Image& out,std::string& error)const;
    bool HasCommitted()const{return !committed_.empty();}
    void Reset(){committed_.clear();prepared_.clear();}
};
}
