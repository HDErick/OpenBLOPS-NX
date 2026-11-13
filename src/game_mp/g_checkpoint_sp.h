#pragma once
#ifdef KISAK_SP
void G_CheckpointInit_SP();
void G_CheckpointFrame_SP();
bool G_CheckpointTryLoad_SP();
int G_CheckpointRequest_SP(const char* name,bool commit);
bool G_CheckpointCommit_SP(int id);
bool G_CheckpointSaveSuccessful_SP();
bool G_CheckpointRecentlyLoaded_SP();
#endif
