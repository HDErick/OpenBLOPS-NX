#pragma once
#ifdef KISAK_SP
#include "checkpoint_fields.h"
struct CheckpointClientStats_SP { unsigned objects=0,bytes=0,identity=0,fakes=0,destructibles=0; };
bool G_CheckpointCaptureClient_SP(std::vector<std::uint8_t>& bytes,CheckpointClientStats_SP& stats,std::string& error);
bool G_CheckpointValidateClient_SP(const std::vector<std::uint8_t>& bytes,std::string& error);
void G_CheckpointRetireClient_SP();
bool G_CheckpointApplyClient_SP(const std::vector<std::uint8_t>& bytes,std::string& error);
#endif
