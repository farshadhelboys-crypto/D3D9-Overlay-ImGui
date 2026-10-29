// =====================================================================
//  ESP — manual attach + aimbot — esp demo (no soft rescan)
// =====================================================================
#include <Windows.h>
#include <TlHelp32.h>
#include <winhttp.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cfloat>
#include <vector>
#include <string>
#include <algorithm>
#include "ImGui/imgui.h"
#include "UI.h"
#include "esp.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winhttp.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Offsets {
    uintptr_t MY_POS_PTR_RVA=0, MY_TEAM_PTR_RVA=0, MY_TEAM_OFFSET=0;
    uintptr_t HP_PTR_RVA=0, HP_OFFSET1=0, HP_OFFSET2=0;
    uintptr_t MY_POS_X=0, MY_POS_Y=0, MY_POS_Z=0;
    uintptr_t MATRIX_RVA=0, VTABLE_1=0, VTABLE_2=0, V_OBJ_ORIGIN=0;
    uintptr_t OBJ_HP_OFF=0, OBJ_TEAM_OFF=0, OBJ_LIFE_OFF=0;
    bool loaded=false;
    char status[96]="Ready";
} Off;

static std::string HttpGetHttps(const wchar_t* host, const wchar_t* path) {
    std::string result;
    HINTERNET hSession = WinHttpOpen(L"FarshadESP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return {};
    HINTERNET hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return {}; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return {}; }
    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);
    if (ok) {
        DWORD dwSize=0;
        do {
            dwSize=0; WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (!dwSize) break;
            std::vector<char> buf(dwSize+1,0); DWORD dwRead=0;
            WinHttpReadData(hRequest, buf.data(), dwSize, &dwRead);
            result.append(buf.data(), dwRead);
        } while (dwSize>0);
    }
    WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
    return result;
}

static uintptr_t JsonHex(const std::string& j, const char* key) {
    std::string k = std::string("\")+key+"\"";
    size_t p=j.find(k); if(p==std::string::npos) return 0;
    p=j.find(':',p); if(p==std::string::npos) return 0;
    while(p<j.size()&&(j[p]==':'||j[p]==' '||j[p]=='\"')) p++;
    size_t e=p; while(e<j.size()&&j[e]!='\"'&&j[e]!=','&&j[e]!='}') e++;
    std::string v=j.substr(p,e-p);
    while(!v.empty()&&(v.back()==' '||v.back()=='\"')) v.pop_back();
    if(v.empty()) return 0;
    return (uintptr_t)strtoull(v.c_str(), nullptr, 0);
}

static bool LoadOffsetsFromGitHub() {
    std::string j = HttpGetHttps(L"raw.githubusercontent.com",
        L"/farshadhelboys-crypto/D3D9-Overlay-ImGui/main/offsets/offsets.json");
    if (j.empty() || j.find("matrix_rva")==std::string::npos) {
        strcpy_s(Off.status, "Config load failed"); Off.loaded=false; return false;
    }
    Off.MY_POS_PTR_RVA=JsonHex(j,"my_pos_ptr_rva");
    Off.MY_TEAM_PTR_RVA=JsonHex(j,"my_team_ptr_rva");
    Off.MY_TEAM_OFFSET=JsonHex(j,"my_team_offset");
    Off.HP_PTR_RVA=JsonHex(j,"hp_ptr_rva");
    Off.HP_OFFSET1=JsonHex(j,"hp_offset1");
    Off.HP_OFFSET2=JsonHex(j,"hp_offset2");
    Off.MY_POS_X=JsonHex(j,"my_pos_x");
    Off.MY_POS_Y=JsonHex(j,"my_pos_y");
    Off.MY_POS_Z=JsonHex(j,"my_pos_z");
    Off.MATRIX_RVA=JsonHex(j,"matrix_rva");
    Off.VTABLE_1=JsonHex(j,"vtable_1");
    Off.VTABLE_2=JsonHex(j,"vtable_2");
    Off.V_OBJ_ORIGIN=JsonHex(j,"v_obj_origin");
    Off.OBJ_HP_OFF=JsonHex(j,"obj_hp_off");
    Off.OBJ_TEAM_OFF=JsonHex(j,"obj_team_off");
    Off.OBJ_LIFE_OFF=JsonHex(j,"obj_life_state");
    if(!Off.OBJ_LIFE_OFF) Off.OBJ_LIFE_OFF=0x120;
    if (!Off.MATRIX_RVA || !Off.VTABLE_1 || !Off.V_OBJ_ORIGIN) {
        strcpy_s(Off.status,"Config incomplete"); Off.loaded=false; return false;
    }
    Off.loaded=true; strcpy_s(Off.status,"Ready"); return true;
}

struct Vec3{float x,y,z;}; struct Vec2{float x,y;};
struct TrackedEntity{uintptr_t obj=0; Vec3 pos{0,0,0}; int hp=0,team=0,life=0; bool valid=false;};
struct EspConfig{
    bool enabled=true,teamCheck=false,drawBox=true,drawCorners=false,drawHealth=false,drawDistance=true;
    bool aimbot=false;
    bool aimOnRMB=true;
    bool aimOnLMB=false;
    bool aimOnAlt=false;
    bool aimOnAltAimOnly=false;
    float aimSmooth=4.f;
    float aimAltSmooth=6.f;
    float aimAltPrecision=6.f;
    float aimFireDelayMs=150.f;
    float aimFov=120.f;
    float headOffset=72.f,widthRatio=0.42f,thickness=1.8f,maxDistance=2000.f,healthMax=100.f;
    int healthSide=0;
    ImVec4 enemyColor{1.f,0.25f,0.25f,1.f}, mateColor{0.25f,0.85f,1.f,1.f};
} cfg;

static DWORD g_lastAutoShot = 0;
static HANDLE g_hProc=nullptr; static uintptr_t g_gameBase=0; static DWORD g_attachedPid=0;
static HWND g_lastTarget=nullptr; static Vec3 g_localPos{0,0,0}; static bool g_haveLocal=false;
static int g_localTeam=0,g_localHP=0; static char g_status[160]="Not attached";
static int g_dbgCount=0,g_dbgDrawn=0; static bool g_dbgVmOk=false;
static std::vector<uintptr_t> g_rawObjects; static std::vector<TrackedEntity> g_tracked;
static bool g_scanned=false,g_offsetsTried=false;
static const int kMaxLife=4;  // short hold: less ghost, still anti-flicker

template<typename T> static inline bool RPM(uintptr_t addr,T& out){ SIZE_T r=0; return ReadProcessMemory(g_hProc,(LPCVOID)addr,&out,sizeof(T),&r)&&r==sizeof(T); }
static inline bool ReadPtr32(uintptr_t addr,uintptr_t& out){ uint32_t t=0; if(!RPM(addr,t))return false; out=(uintptr_t)t; return out>0x10000; }
static uintptr_t GetModuleBase(DWORD pid,const wchar_t* mod){
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(s==INVALID_HANDLE_VALUE)return 0; MODULEENTRY32W me{sizeof(me)}; uintptr_t base=0;
    if(Module32FirstW(s,&me)){ do{ if(_wcsicmp(me.szModule,mod)==0){base=(uintptr_t)me.modBaseAddr;break;} }while(Module32NextW(s,&me)); }
    CloseHandle(s); return base;
}
static void EnsureOffsets(){ if(Off.loaded||g_offsetsTried)return; g_offsetsTried=true; LoadOffsetsFromGitHub(); }
static void Detach(){
    if(g_hProc){CloseHandle(g_hProc);g_hProc=nullptr;} g_gameBase=0;g_attachedPid=0;g_haveLocal=false;g_localTeam=0;g_localHP=0;
    g_dbgCount=0;g_dbgDrawn=0;g_dbgVmOk=false;g_rawObjects.clear();g_tracked.clear();g_scanned=false;strcpy_s(g_status,"Not attached");
}

static void ScanVTableObjects(bool full_reset=false){
    if(!g_hProc||!g_gameBase||!Off.loaded)return;
    if(full_reset){g_rawObjects.clear();g_tracked.clear();}
    const uint32_t targets[2]={(uint32_t)(g_gameBase+Off.VTABLE_1),(uint32_t)(g_gameBase+Off.VTABLE_2)};
    std::vector<uintptr_t> found;
    const uintptr_t memStart=0x10000000, memEnd=0x40000000; const size_t chunk=0x100000;
    std::vector<uint8_t> buf(chunk);
    for(uintptr_t addr=memStart;addr<memEnd;addr+=chunk){
        SIZE_T bytesRead=0;
        if(!ReadProcessMemory(g_hProc,(LPCVOID)addr,buf.data(),chunk,&bytesRead)||bytesRead<16) continue;
        for(size_t i=0;i+4<=bytesRead;i+=4){
            uint32_t val=*reinterpret_cast<uint32_t*>(buf.data()+i);
            if(val==targets[0]||val==targets[1]) found.push_back(addr+i);
        }
    }
    std::vector<TrackedEntity> kept;
    for(uintptr_t o:found){
        TrackedEntity* existing=nullptr;
        for(auto& te:g_tracked){ if(te.obj==o){ existing=&te; break; } }
        if(existing) kept.push_back(*existing);
        else { TrackedEntity te; te.obj=o; kept.push_back(te); }
    }
    g_tracked.swap(kept);
    g_rawObjects=found; g_scanned=true;
    sprintf_s(g_status,"Attached | Players: %zu",found.size());
}

static bool AttachFromUI(){
    Detach(); EnsureOffsets();
    if(!Off.loaded){strcpy_s(g_status,Off.status);return false;}
    if(!UI::IsWindowTargeted()){strcpy_s(g_status,"No target window");return false;}
    HWND hwnd=UI::GetTargetWindow(); DWORD pid=UI::GetTargetPID();
    if(!pid&&hwnd) GetWindowThreadProcessId(hwnd,&pid);
    if(!pid){strcpy_s(g_status,"Target PID invalid");return false;}
    g_hProc=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,FALSE,pid);
    if(!g_hProc){strcpy_s(g_status,"OpenProcess failed (run as admin)");return false;}
    g_gameBase=GetModuleBase(pid,L"clientmod_game.dll");
    if(!g_gameBase){strcpy_s(g_status,"Game module not loaded yet");CloseHandle(g_hProc);g_hProc=nullptr;return false;}
    g_attachedPid=pid; strcpy_s(g_status,"Scanning players..."); ScanVTableObjects(true); return true;
}
static void UpdateLocal(){
    g_haveLocal=false;g_localTeam=0;g_localHP=0; if(!g_hProc||!g_gameBase||!Off.loaded)return;
    uintptr_t posPtr=0;
    if(Off.MY_POS_PTR_RVA&&ReadPtr32(g_gameBase+Off.MY_POS_PTR_RVA,posPtr)){
        Vec3 pos{}; if(RPM(posPtr,pos.x)&&RPM(posPtr+4,pos.y)&&RPM(posPtr+8,pos.z))
            if(fabsf(pos.x)>0.5f||fabsf(pos.y)>0.5f||fabsf(pos.z)>0.5f){g_localPos=pos;g_haveLocal=true;}
    }
    if(!g_haveLocal&&Off.MY_POS_X){
        Vec3 pos{}; if(RPM(g_gameBase+Off.MY_POS_X,pos.x)&&RPM(g_gameBase+Off.MY_POS_Y,pos.y)&&RPM(g_gameBase+Off.MY_POS_Z,pos.z))
            if(fabsf(pos.x)>0.5f||fabsf(pos.y)>0.5f||fabsf(pos.z)>0.5f){g_localPos=pos;g_haveLocal=true;}
    }
    uintptr_t teamBase=0; if(Off.MY_TEAM_PTR_RVA&&ReadPtr32(g_gameBase+Off.MY_TEAM_PTR_RVA,teamBase)){int team=0;if(RPM(teamBase+Off.MY_TEAM_OFFSET,team))g_localTeam=team;}
    uintptr_t hp1=0,hp2=0; if(Off.HP_PTR_RVA&&ReadPtr32(g_gameBase+Off.HP_PTR_RVA,hp1)&&ReadPtr32(hp1+Off.HP_OFFSET1,hp2)){int hp=0;if(RPM(hp2+Off.HP_OFFSET2,hp))g_localHP=hp;}
}
static bool WorldToScreen(const Vec3& pos,const float* mat,float sw,float sh,Vec2& out){
    float cx=mat[0]*pos.x+mat[1]*pos.y+mat[2]*pos.z+mat[3];
    float cy=mat[4]*pos.x+mat[5]*pos.y+mat[6]*pos.z+mat[7];
    float cw=mat[12]*pos.x+mat[13]*pos.y+mat[14]*pos.z+mat[15];
    if(fabsf(cw)<0.001f||cw<0.15f)return false;
    out.x=(sw*0.5f)*(1.f+cx/cw); out.y=(sh*0.5f)*(1.f-cy/cw); return true;
}
static inline ImU32 C(ImVec4 v){return ImGui::ColorConvertFloat4ToU32(v);}


#include "esp_render.inl"
