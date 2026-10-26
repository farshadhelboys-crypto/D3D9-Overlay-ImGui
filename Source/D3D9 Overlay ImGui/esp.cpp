// =====================================================================
//  ESP — manual attach + aimbot (RMB/LMB/ALT) — esp demo
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
    uintptr_t OBJ_HP_OFF=0, OBJ_TEAM_OFF=0;
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
    std::string k = std::string("\"")+key+"\"";
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
static const int kMaxLife=12;

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

void RenderESPMenu(){
    if(!UI::IsWindowTargeted())return; EnsureOffsets();
    ImGui::SetNextWindowSize(ImVec2(360,440),ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("farshad pm Esp",nullptr,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysVerticalScrollbar)){ImGui::End();return;}
    ImGui::TextColored(ImVec4(0.4f,0.85f,1.f,1.f),"farshad pm Esp"); ImGui::SameLine(); ImGui::TextDisabled("esp demo");
    ImGui::Separator();
    ImGui::Checkbox("Enable ESP",&cfg.enabled); ImGui::SameLine();
    if(ImGui::Button("Attach")) AttachFromUI();
    ImGui::SameLine(); ImGui::TextDisabled("F5 = Attach/Rescan");
    if(ImGui::Button("Reload config")){g_offsetsTried=false;Off.loaded=false;EnsureOffsets();}
    ImGui::TextColored(ImVec4(0.5f,1.f,0.5f,1.f),"%s",g_status);
    if(!Off.loaded) ImGui::TextDisabled("%s",Off.status);
    ImGui::Separator();
    if(ImGui::BeginTabBar("tabs")){
        if(ImGui::BeginTabItem("Main")){ImGui::Checkbox("Team Check",&cfg.teamCheck);ImGui::SliderFloat("Max Distance",&cfg.maxDistance,50.f,4000.f,"%.0f m");ImGui::EndTabItem();}
        if(ImGui::BeginTabItem("Visuals")){
            ImGui::Checkbox("Box",&cfg.drawBox);ImGui::SameLine();ImGui::Checkbox("Corners",&cfg.drawCorners);
            ImGui::Checkbox("Health Bar",&cfg.drawHealth);ImGui::SameLine();ImGui::Checkbox("Distance",&cfg.drawDistance);
            ImGui::SliderFloat("Thickness",&cfg.thickness,0.5f,6.f,"%.1f");
            ImGui::SliderFloat("Box Height",&cfg.headOffset,30.f,140.f,"%.0f");
            ImGui::SliderFloat("Width Ratio",&cfg.widthRatio,0.15f,0.9f,"%.2f");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Aimbot")){
            ImGui::Checkbox("Enable Aim Assist",&cfg.aimbot);
            ImGui::Separator();
            ImGui::TextDisabled("Activation");
            ImGui::Checkbox("Aim on RMB (right click)",&cfg.aimOnRMB);
            ImGui::Checkbox("Aim on LMB (left click)",&cfg.aimOnLMB);
            ImGui::Checkbox("Aim on ALT (pull + autofire)",&cfg.aimOnAlt);
            ImGui::Checkbox("Aim on ALT (aim only, no shoot)",&cfg.aimOnAltAimOnly);
            ImGui::Separator();
            ImGui::SliderFloat("Smooth (RMB/LMB)",&cfg.aimSmooth,1.f,20.f,"%.1f");
            ImGui::SliderFloat("ALT Smooth",&cfg.aimAltSmooth,1.f,25.f,"%.1f");
            ImGui::SliderFloat("ALT Precision (px)",&cfg.aimAltPrecision,1.f,40.f,"%.0f");
            ImGui::SliderFloat("Fire Delay (ms)",&cfg.aimFireDelayMs,50.f,1000.f,"%.0f");
            ImGui::SliderFloat("FOV (px)",&cfg.aimFov,0.f,600.f,"%.0f");
            ImGui::TextDisabled("ALT aim-only = pull only; Fire Delay = autofire rate");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Status")){
            ImGui::Text("Connected: %s",g_hProc?"Yes":"No");
            ImGui::Text("Local: %s",g_haveLocal?"Found":"Not found");
            ImGui::Text("Camera: %s",g_dbgVmOk?"OK":"Failed");
            ImGui::Text("Players: %d  Drawn: %d",g_dbgCount,g_dbgDrawn);
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Contact")){
            ImGui::Text("Telegram: @client_v34");
            if(ImGui::Button("Open Telegram",ImVec2(-1,0))) ShellExecuteA(nullptr,"open","https://t.me/client_v34",nullptr,nullptr,SW_SHOWNORMAL);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void RenderESPBoxes(){
    EnsureOffsets(); if(!Off.loaded) return;
    if (GetAsyncKeyState(VK_F5) & 1) {
        if (UI::IsWindowTargeted()) AttachFromUI();
    }
    if(UI::GetTargetWindow()!=g_lastTarget){
        g_lastTarget=UI::GetTargetWindow();
        Detach();
    }
    if(!g_hProc||!g_gameBase) return;
    UpdateLocal(); if(!cfg.enabled||!g_scanned) return;

    ImGuiIO& io=ImGui::GetIO(); const float sw=io.DisplaySize.x, sh=io.DisplaySize.y;
    ImDrawList* dl=ImGui::GetBackgroundDrawList(); if(!dl) return;
    float mat[16]={}; g_dbgVmOk=RPM(g_gameBase+Off.MATRIX_RVA,mat);
    if(!g_dbgVmOk){ dl->AddText(ImVec2(20,70),IM_COL32(255,80,80,255),"Camera read failed"); return; }

    g_dbgDrawn=0; g_dbgCount=0;
    float bestD=1e9f,bestX=0,bestY=0; bool hasAim=false;

    for(auto& te:g_tracked){
        Vec3 origin{};
        bool ok=RPM(te.obj+Off.V_OBJ_ORIGIN,origin.x)&&RPM(te.obj+Off.V_OBJ_ORIGIN+4,origin.y)&&RPM(te.obj+Off.V_OBJ_ORIGIN+8,origin.z);
        if(ok){
            if(fabsf(origin.x)>30000.f||fabsf(origin.y)>30000.f||fabsf(origin.z)>30000.f) ok=false;
            if(fabsf(origin.x)<0.05f&&fabsf(origin.y)<0.05f) ok=false;
        }
        int hp=0,team=0;
        if(ok){
            RPM(te.obj+Off.OBJ_HP_OFF,hp); RPM(te.obj+Off.OBJ_TEAM_OFF,team);
            te.hp=hp; te.team=team;
            te.pos=origin;
            te.life=kMaxLife;
            te.valid=true;
        } else {
            if(te.life>0) te.life--;
            if(te.life<=0) te.valid=false;
        }
        if(!te.valid) continue;
        if(cfg.teamCheck&&g_localTeam&&te.team==g_localTeam) continue;
        float dist=-1.f;
        if(g_haveLocal){
            float dx=te.pos.x-g_localPos.x,dy=te.pos.y-g_localPos.y,dz=te.pos.z-g_localPos.z;
            dist=sqrtf(dx*dx+dy*dy+dz*dz);
            if(dist<15.f||dist>cfg.maxDistance) continue;
        }
        ++g_dbgCount;
        Vec3 head=te.pos; head.z+=cfg.headOffset;
        Vec2 feet{},headS{};
        if(!WorldToScreen(te.pos,mat,sw,sh,feet)) continue;
        if(!WorldToScreen(head,mat,sw,sh,headS)) continue;
        float h=feet.y-headS.y; if(h<6.f||h>2000.f) continue;
        float w=h*cfg.widthRatio; if(w<2.f)w=2.f;
        float bx=headS.x-w*0.5f, by=headS.y;
        ImU32 col=C(cfg.enemyColor);
        if(cfg.drawBox) dl->AddRect(ImVec2(bx,by),ImVec2(bx+w,by+h),col,0.f,0,cfg.thickness);
        if(cfg.drawDistance&&dist>=0.f){ char b[32]; sprintf_s(b,"%.0fm",dist); ImVec2 ts=ImGui::CalcTextSize(b); dl->AddText(ImVec2(bx+w*0.5f-ts.x*0.5f,by+h+2.f),IM_COL32(255,255,255,230),b); }
        ++g_dbgDrawn;
        if(cfg.aimbot){
            float ax=headS.x, ay=headS.y+h*0.25f;
            float adx=ax-sw*0.5f, ady=ay-sh*0.5f; float ad=sqrtf(adx*adx+ady*ady);
            float fovLimit = cfg.aimFov <= 0.f ? 1.f : cfg.aimFov;
            if(ad<bestD&&ad<=fovLimit){bestD=ad;bestX=ax;bestY=ay;hasAim=true;}
        }
    }

    if(cfg.aimbot && cfg.aimFov > 0.f)
        dl->AddCircle(ImVec2(sw*0.5f,sh*0.5f),cfg.aimFov,IM_COL32(255,255,255,40),64,1.f);

    if(hasAim && cfg.aimbot){
        const bool rmb = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
        const bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;

        const bool useRmb = cfg.aimOnRMB && rmb;
        const bool useLmb = cfg.aimOnLMB && lmb;
        const bool useAltFire = cfg.aimOnAlt && alt;
        const bool useAltOnly = cfg.aimOnAltAimOnly && alt;
        const bool useAlt = useAltFire || useAltOnly;
        const bool anyMode = cfg.aimOnRMB || cfg.aimOnLMB || cfg.aimOnAlt || cfg.aimOnAltAimOnly;
        const bool active = useRmb || useLmb || useAlt || !anyMode;

        if(active){
            float cx = sw * 0.5f, cy = sh * 0.5f;
            float dx = bestX - cx, dy = bestY - cy;
            float dist = sqrtf(dx*dx + dy*dy);

            dl->AddLine(ImVec2(bestX-8,bestY),ImVec2(bestX+8,bestY),IM_COL32(0,255,0,230),1.5f);
            dl->AddLine(ImVec2(bestX,bestY-8),ImVec2(bestX,bestY+8),IM_COL32(0,255,0,230),1.5f);
            dl->AddLine(ImVec2(cx,cy),ImVec2(bestX,bestY),IM_COL32(0,255,0,120),1.0f);

            float sm = useAlt ? cfg.aimAltSmooth : cfg.aimSmooth;
            if(sm < 1.f) sm = 1.f;
            float stepX = dx / sm, stepY = dy / sm;
            const float maxStep = 45.f;
            if(stepX >  maxStep) stepX =  maxStep;
            if(stepX < -maxStep) stepX = -maxStep;
            if(stepY >  maxStep) stepY =  maxStep;
            if(stepY < -maxStep) stepY = -maxStep;

            int mx = (int)(stepX >= 0 ? stepX + 0.5f : stepX - 0.5f);
            int my = (int)(stepY >= 0 ? stepY + 0.5f : stepY - 0.5f);
            if(!mx && fabsf(dx) > 1.5f) mx = dx > 0 ? 1 : -1;
            if(!my && fabsf(dy) > 1.5f) my = dy > 0 ? 1 : -1;

            if(mx || my){
                INPUT in{}; in.type=INPUT_MOUSE;
                in.mi.dx=mx; in.mi.dy=my; in.mi.dwFlags=MOUSEEVENTF_MOVE;
                SendInput(1,&in,sizeof(INPUT));
            }

            if(useAltFire){
                float prec = cfg.aimAltPrecision < 1.f ? 1.f : cfg.aimAltPrecision;
                if(dist <= prec){
                    DWORD now = GetTickCount();
                    DWORD delay = (DWORD)(cfg.aimFireDelayMs < 50.f ? 50.f : cfg.aimFireDelayMs);
                    if(now - g_lastAutoShot > delay){
                        g_lastAutoShot = now;
                        INPUT click[2]{};
                        click[0].type = INPUT_MOUSE;
                        click[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                        click[1].type = INPUT_MOUSE;
                        click[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                        SendInput(2, click, sizeof(INPUT));
                    }
                }
            }
        }
    }
}

void RenderESP(){ RenderESPMenu(); RenderESPBoxes(); }
