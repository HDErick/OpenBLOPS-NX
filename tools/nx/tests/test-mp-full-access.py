"""Exercise real MP availability/purchase validators with zero-rank, unpurchased profiles."""
from pathlib import Path
import os,subprocess,argparse
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe');a=p.parse_args()
r=Path(__file__).resolve().parents[3];out=r/'build-nx/mp-full-access-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,name):
 s=(r/path).read_text();start=s.index(name);end=s.index('{',start)+1;depth=1
 while depth:
  depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
code=r"""
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <bgame/bg_unlock_policy.h>
#ifndef __cdecl
#define __cdecl
#endif
#define __debugbreak() assert(false)
struct dvar_t{} defaults;
const dvar_t *allItemsUnlocked=&defaults,*allItemsPurchased=&defaults,*ui_items_no_cost=&defaults;
constexpr int GAMEMODE_PRIVATE_MATCH=1,GAMEMODE_BASIC_TRAINING=2,GAMEMODE_LOCAL_SPLITSCREEN=3,GAMEMODE_SYSTEMLINK=4;
int mode=0;
bool Com_GameMode_IsGameMode(int m){return mode==m;}
bool Com_GameMode_IsPublicOnlineGame(){return mode==0;}
bool Com_GameMode_IsPrivateOnlineGame(){return mode==5;}
int Dvar_GetInt(const dvar_t *){return 0;}
struct itemInfo_t {int cost=1000,unlockLevel=49,unlockPLevel=15,dwIndex=2,numAttachments[5]{4,0,0,0,0};} validItem;
itemInfo_t *BG_UnlockablesGetItemInfo(int i){return i==1||i==2?&validItem:nullptr;}
bool BG_UnlockablesIsItemValidNotNull(int i){return BG_UnlockablesGetItemInfo(i)!=nullptr;}
struct ddlState_t{} root;
const ddlState_t *LiveStats_GetRootDDLState(){return &root;}
bool DDL_MoveTo(const ddlState_t *,ddlState_t *,int,...){return false;}
int DDL_GetInt(ddlState_t *,char *){return 0;}
void DDL_PrintError(const char *,...){}
struct persistentStats {unsigned char statsBuffer[16]{};} profile;
constexpr int STATS_LOCATION_NORMAL=0;
persistentStats *LiveStorage_GetStatsBuffer(int,int,int){return &profile;}
int LiveStats_GetRank(int){return 0;}
int LiveStats_GetPrestige(int){return 0;}
char *va(const char *format,...){static char b[8][256];static int index=0;auto *p=b[(index++)%8];va_list a;va_start(a,format);vsnprintf(p,256,format,a);va_end(a);return p;}
int Assert_MyHandler(const char *,int,int,const char *,...){return 1;}
int BG_GetWeaponOptionCount(){return 16;}
int BG_GetWeaponOptionCost(int){return 50000;}
int BG_GetWeaponOptionUnlockLvl(int){return 49;}
int BG_GetWeaponOptionUnlockPLevel(int){return 15;}
using eAttachment=int;
constexpr int ATTACHMENT_INVALID=24;
int BG_GetAttachmentCost(eAttachment){return 50000;}
eAttachment BG_GetAttachmentIndex(const char *s){return s&&std::strcmp(s,"reflex")==0?1:ATTACHMENT_INVALID;}
eAttachment BG_UnlockablesGetItemAttachment(int,int n){return n;}
const ddlState_t *BG_UnlockablesGetAttachmentStatsStateByRef(ddlState_t *,const char *){return &root;}
constexpr int CLANTAG_COL_COST=0,CLANTAG_COL_NAME=1;
int BG_UnlockablesGetClanTagFeatureCount(){return 3;}
const char *BG_UnlockablesGetClanTagFeature(unsigned,int column){return column==CLANTAG_COL_COST?"1000":"feature";}
bool SV_CACValidate_IsClanTagFeatureLocked(int,int,unsigned){return true;}
struct {struct {unsigned char purchasedItems[32]{};} clients[32];} svs;
"""
for name in [
 'char __cdecl BG_UnlockablesAllItemsUnlocked(',
 'char __cdecl BG_UnlockablesAllItemsFree(',
 'bool __cdecl BG_UnlockablesIsItemLockedForRank(',
 'bool __cdecl BG_UnlockablesIsItemLocked(',
 'bool __cdecl BG_UnlockablesIsItemPurchasedFromBuffer(',
 'bool __cdecl BG_UnlockablesIsItemPurchased(',
 'bool __cdecl BG_UnlockablesIsItemOptionPurchased(',
 'bool __cdecl BG_UnlockablesGetAttachmentPurchased(',
 'bool __cdecl BG_UnlockablesIsWeaponOptionLocked(',
 'bool __cdecl SV_CacValidate_IsItemPurchased(',
 'bool __cdecl SV_CACValidate_IsItemAttachmentPurchased(',
 'bool __cdecl SV_CACValidate_IsItemOptionPurchased(',
 'bool __cdecl SV_CACValidate_IsClanTagFeaturePurchased(']:
 code+=function('src/bgame/bg_unlockable_items.cpp',name)
code+=function('src/game_mp/g_scr_main_mp.cpp','bool __cdecl GScr_IsItemPurchasedForClientNum(')
code+=r"""
int main(){
 unsigned char emptyStats[16]{};
#if defined(KISAK_MP) && defined(OPENBLOPS_NO_STEAM_AUTH)
 assert(BG_UnlockablesHasFullAccess());
 for(int state:{0,1,2,3,4,5}){mode=state;assert(BG_UnlockablesAllItemsUnlocked()&&BG_UnlockablesAllItemsFree());}
 mode=0;
 for(int player:{0,1,2,31}){
  assert(!BG_UnlockablesIsItemLocked(player,1));
  assert(BG_UnlockablesIsItemPurchased(player,1));
  assert(GScr_IsItemPurchasedForClientNum(player,1));
  for(int option=0;option<BG_GetWeaponOptionCount();++option){
   assert(!BG_UnlockablesIsWeaponOptionLocked(player,option));
   assert(BG_UnlockablesIsItemOptionPurchased(player,1,option));
   assert(SV_CACValidate_IsItemOptionPurchased(emptyStats,1,option,0,0));
  }
 }
 assert(BG_UnlockablesGetAttachmentPurchased(0,"reflex"));
 assert(!BG_UnlockablesGetAttachmentPurchased(0,"not_an_attachment"));
 for(int n=0;n<4;++n)assert(SV_CACValidate_IsItemAttachmentPurchased(emptyStats,1,n,0,0));
 assert(!SV_CACValidate_IsItemAttachmentPurchased(emptyStats,1,-1,0,0));
 assert(!SV_CACValidate_IsItemAttachmentPurchased(emptyStats,1,4,0,0));
 assert(!SV_CACValidate_IsItemOptionPurchased(emptyStats,1,-1,0,0));
 assert(!SV_CACValidate_IsItemOptionPurchased(emptyStats,1,16,0,0));
 assert(!SV_CACValidate_IsItemOptionPurchased(emptyStats,255,1,0,0));
 assert(!SV_CacValidate_IsItemPurchased(emptyStats,255,0,0));
 for(unsigned n=0;n<3;++n)assert(SV_CACValidate_IsClanTagFeaturePurchased(emptyStats,n,0,0));
 assert(!SV_CACValidate_IsClanTagFeaturePurchased(emptyStats,3,0,0));
 assert(!GScr_IsItemPurchasedForClientNum(32,1));
 assert(!GScr_IsItemPurchasedForClientNum(0,256));
 puts("PASS: all zero-rank players, every camo/weapon option, attachments, script/server purchases, invalid IDs rejected");
#else
 assert(!BG_UnlockablesHasFullAccess());assert(!BG_UnlockablesAllItemsUnlocked()&&!BG_UnlockablesAllItemsFree());
 assert(BG_UnlockablesIsWeaponOptionLocked(0,15));
 assert(!BG_UnlockablesIsItemOptionPurchased(0,1,15));
 assert(!SV_CacValidate_IsItemPurchased(emptyStats,1,0,0));
 assert(!GScr_IsItemPurchasedForClientNum(0,1));
 mode=GAMEMODE_PRIVATE_MATCH;assert(BG_UnlockablesAllItemsUnlocked()&&BG_UnlockablesAllItemsFree());
 puts("PASS: non-direct/SP progression and existing private-match behavior preserved");
#endif
}
"""
# Range-for braced lists require this on the minimal harness.
code='#include <initializer_list>\n'+code
cpp=out/'full-access.cpp';cpp.write_text(code)
env=os.environ.copy();env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
for name,defines in [('pc-direct-mp',['KISAK_MP','OPENBLOPS_NO_STEAM_AUTH']),('nx-direct-mp',['KISAK_MP','KISAK_NX','OPENBLOPS_NO_STEAM_AUTH']),('legacy-mp',['KISAK_MP']),('direct-sp',['KISAK_SP','OPENBLOPS_NO_STEAM_AUTH'])]:
 exe=out/(name+'.exe')
 subprocess.run([a.compiler,'-std=c++20','-I'+str(r/'src'),*[('-D'+d) for d in defines],str(cpp),'-o',str(exe)],check=True,env=env)
 subprocess.run([str(exe)],check=True,env=env)
 print(name+': passed',flush=True)
