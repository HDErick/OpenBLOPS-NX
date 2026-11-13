#pragma once
#ifdef KISAK_SP
#include "checkpoint_archive.h"
#include "checkpoint_fields.h"
void G_CheckpointClearLiveReferences_SP();
bool G_CheckpointCanApplyServer_SP(const checkpoint::Image& image,const checkpoint::Resources& source,std::string& error);
bool G_CheckpointApplyServer_SP(const checkpoint::Image& image,const checkpoint::Resources& source,std::string& error);
bool G_CheckpointValidateStaging_SP(const checkpoint::Image& image,const checkpoint::Resources& source,unsigned& relocated,std::string& error);
#endif
