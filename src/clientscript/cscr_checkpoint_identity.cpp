#include "cscr_checkpoint_identity.h"
#ifdef KISAK_SP
#include "cscr_variable.h"
#include <game_mp/checkpoint_identity.h>
namespace { checkpoint::SourceIdentity sources[2]; }
void Scr_ResetCheckpointIdentity_SP(scriptInstance_t inst){sources[inst].Reset();}
void Scr_RecordCheckpointSource_SP(scriptInstance_t inst,const char* filename,const char* source,int size){sources[inst].Record(filename,source,size);}
std::uint32_t Scr_CheckpointIdentity_SP(scriptInstance_t inst){
    const auto& pub=gScrVarPub[inst];
    return sources[inst].Finish(pub.checksum,static_cast<std::uint32_t>(pub.endScriptBuffer-pub.programBuffer),sizeof(void*));
}
std::uint32_t Scr_CheckpointSourceCount_SP(scriptInstance_t inst){return sources[inst].Count();}
#endif
