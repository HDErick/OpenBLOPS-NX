"""Exercise community password-popup confirmation, cancellation and stable server identity."""
from pathlib import Path
import argparse,os,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--compiler',default='C:/msys64/ucrt64/bin/g++.exe');a=p.parse_args()
r=Path(__file__).resolve().parents[3];out=r/'build-nx/join-password-tests';out.mkdir(parents=True,exist_ok=True)
def function(path,sig):
 s=(r/path).read_text();start=s.index(sig);end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
code=r'''
#include <cassert>
#include <atomic>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <algorithm>
struct Row{std::string id,map;bool password=false;};
struct FakeClient{std::vector<Row>rows;const std::vector<Row>&Rows(){return rows;}}store;
auto*client=&store;bool enabled=true,popup=false;int opened=0,closed=0,joins=0,errors=0;
std::string passwordJoinId,joinedId,joinedPassword;unsigned passwordJoinGeneration=0;
std::atomic<unsigned>generation{1};
std::map<std::string,std::string>vars;
void Dvar_SetStringByName(const char*n,const char*v){vars[n]=v;}
const char*Dvar_GetString(const char*n){return vars[n].c_str();}
void _Dvar_RegisterString(const char*n,const char*v,unsigned flags,const char*){
 assert(!strcmp(n,"ui_directory_joinPassword")&&flags==0);if(!vars.count(n))vars[n]=v;}
struct UI{int uiDC=0;}ui;UI*UI_GetInfo(int){return &ui;}
bool Directory_IsRow(int s,int i){return enabled&&s==0&&client&&i>=0&&i<int(client->Rows().size());}
bool Directory_Join(int s,int i){assert(Directory_IsRow(s,i));++joins;joinedId=client->Rows()[i].id;joinedPassword=vars["password"];return true;}
void Directory_CancelJoinPassword();
void Menus_OpenByName(int,int*,const char*n){if(!strcmp(n,"join_password_popmenu")){popup=true;++opened;}else{assert(!strcmp(n,"error_popmenu"));++errors;}}
void Menus_CloseByName(int,int*,const char*n){assert(!strcmp(n,"join_password_popmenu"));popup=false;++closed;Directory_CancelJoinPassword();}
void CG_SetThirdPerson(int){}
'''
for sig in ['bool Directory_PromptJoinPassword(', 'bool Directory_HasPasswordPrompt(', 'void Directory_CancelJoinPassword(', 'bool Directory_AcceptJoinPassword(']:code+=function('src/openblops_directory/directory_engine.cpp',sig)+'\n'
code+=r'''
int main(){
 store.rows={{"public","mp_array",false},{"locked","mp_nuked",true}};
 assert(!Directory_PromptJoinPassword(0,0)&&!Directory_PromptJoinPassword(0,-1)&&!opened&&!joins);
 vars["password"]="old-secret";
 assert(Directory_PromptJoinPassword(0,1)&&popup&&Directory_HasPasswordPrompt()&&opened==1);
 assert(vars["password"].empty()&&vars["ui_directory_joinPassword"].empty()&&!joins);
 vars["ui_directory_joinPassword"]="typed-password";
 assert(vars["password"].empty()); // Typing does not broadcast userinfo.
 std::swap(store.rows[0],store.rows[1]); // Refresh/sort moves the protected row.
 assert(Directory_AcceptJoinPassword());
 assert(joins==1&&joinedId=="locked"&&joinedPassword=="typed-password"&&!popup&&!Directory_HasPasswordPrompt());
 assert(vars["ui_directory_joinPassword"].empty()&&vars["ui_mapname"]=="mp_nuked");
 assert(!Directory_AcceptJoinPassword()&&joins==1);
 // Cancel/Escape closes the popup without attempting a connection.
 assert(Directory_PromptJoinPassword(0,0));vars["ui_directory_joinPassword"]="cancel-me";
 Menus_CloseByName(0,&ui.uiDC,"join_password_popmenu");
 assert(!Directory_HasPasswordPrompt()&&vars["password"].empty()&&vars["ui_directory_joinPassword"].empty()&&joins==1);
 // A disappeared host or replaced transport cannot receive the password.
 assert(Directory_PromptJoinPassword(0,0));vars["ui_directory_joinPassword"]="never-send";
 store.rows.erase(store.rows.begin());assert(Directory_AcceptJoinPassword());
 assert(joins==1&&errors==1&&vars["password"].empty()&&!vars["ui_errorMessage"].empty());
 store.rows.push_back({"locked","mp_nuked",true});assert(Directory_PromptJoinPassword(0,1));
 ++generation;vars["ui_directory_joinPassword"]="wrong-transport";assert(Directory_AcceptJoinPassword());
 assert(joins==1&&errors==2&&vars["password"].empty());
 // Cancel and open another protected server always starts with an empty draft.
 assert(Directory_PromptJoinPassword(0,1));vars["ui_directory_joinPassword"]="discard";
 assert(Directory_PromptJoinPassword(0,1));assert(vars["ui_directory_joinPassword"].empty());
 Directory_CancelJoinPassword();assert(!Directory_AcceptJoinPassword()&&joins==1);
 puts("PASS: protected-server popup, confirmation-only connection, stable server ID through refresh, cancel/escape cleanup, vanished server, transport reset and non-userinfo password draft");
}
'''
cpp=out/'password.cpp';exe=cpp.with_suffix('.exe');cpp.write_text(code)
env=os.environ.copy();env['PATH']=str(Path(a.compiler).parent)+os.pathsep+env.get('PATH','')
subprocess.run([a.compiler,'-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)],check=True,env=env);subprocess.run([str(exe)],check=True,env=env)
assert 'Directory_PromptJoinPassword(source, index)' in function('src/ui_mp/ui_main_mp.cpp','void __cdecl UI_RunMenuScript_JoinServer(')
handler=function('src/openblops_directory/directory_frontend.cpp','bool Directory_HandleMenuEvent(')
for text in ['"join_password_popmenu"','"JoinServer"','"onAccept"','"onClose"','"onESC"','Directory_AcceptJoinPassword()','Directory_CancelJoinPassword()']:assert text in handler
close=function('src/ui/ui_shared.cpp','void Menu_RunCloseScript(')
assert 'Item_RunEventScript' in close and '"onClose"' in close
print('PASS: native popup Join/Enter/Cancel/Escape event hooks and browser entry point are wired')
