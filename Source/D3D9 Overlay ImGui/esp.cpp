// =====================================================================
//  ESP — port of working Python logic (VTable + matrix 0x27603D0)
//  Anti-ghost = static position (like Python DEAD_STATIC_FRAMES)
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

static const uintptr_t K_MATRIX_RVA   = 0x27603D0;
static const uintptr_t K_MY_POS_X     = 0x242F510;
static const uintptr_t K_MY_POS_Y     = 0x242F514;
static const uintptr_t K_MY_POS_Z     = 0x242F518;
static const uintptr_t K_VTABLE_1     = 0x01DDAC3C;
static const uintptr_t K_VTABLE_2     = 0x01DDA9C8;
static const uintptr_t K_V_OBJ_ORIGIN = 0x538;

static const float  HEAD_OFFSET         = 72.f;
static const float  MIN_DIST            = 80.f;
static const float  MAX_DIST            = 5000.f;
static const float  MIN_BOX_H           = 5.f;
static const float  MAX_BOX_H           = 1500.f;
static const int    DEAD_STATIC_FRAMES  = 300;
static const float  POS_EPS_SQ          = 16.f;
static const int    HOLD_FRAMES         = 12;

struct Offsets {
    uintptr_t MATRIX_RVA=K_MATRIX_RVA, VTABLE_1=K_VTABLE_1, VTABLE_2=K_VTABLE_2, V_OBJ_ORIGIN=K_V_OBJ_ORIGIN;
    uintptr_t MY_POS_X=K_MY_POS_X, MY_POS_Y=K_MY_POS_Y, MY_POS_Z=K_MY_POS_Z;
    bool loaded=true;
    char status[96]="Hardcoded OK";
} Off;

struct Vec3{float x,y,z;}; struct Vec2{float x,y;};

struct TrackedEntity {
    uintptr_t obj=0;
    Vec3 pos{0,0,0};
    Vec3 prev{0,0,0};
    int staticFrames=0;
    int hold=0;
    bool hasPrev=false;
    bool valid=false;
};

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
    float headOffset=HEAD_OFFSET,widthRatio=0.40f,thickness=1.8f,maxDistance=MAX_DIST,healthMax=100.f;
    int healthSide=0;
    ImVec4 enemyColor{1.f,0.25f,0.25f,1.f}, mateColor{0.25f,0.85f,1.f,1.f};
} cfg;

static DWORD g_lastAutoShot = 0;
static HANDLE g_hProc=nullptr; static uintptr_t g_gameBase=0; static DWORD g_attachedPid=0;
static HWND g_lastTarget=nullptr; static Vec3 g_localPos{0,0,0}; static bool g_haveLocal=false;
static int g_localTeam=0,g_localHP=0; static char g_status[160]="Not attached";
static int g_dbgCount=0,g_dbgDrawn=0; static bool g_dbgVmOk=false;
static std::vector<uintptr_t> g_rawObjects;
static std::vector<TrackedEntity> g_tracked;
static bool g_scanned=false;
static int g_frame=0;

template<typename T> static inline bool RPM(uintptr_t addr,T& out){
    SIZE_T r=0; return ReadProcessMemory(g_hProc,(LPCVOID)addr,&out,sizeof(T),&r)&&r==sizeof(T);
}
static inline bool ReadPtr32(uintptr_t addr,uintptr_t& out){
    uint32_t t=0; if(!RPM(addr,t))return false; out=(uintptr_t)t; return out>0x10000;
}
static uintptr_t GetModuleBase(DWORD pid,const wchar_t* mod){
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(s==INVALID_HANDLE_VALUE)return 0; MODULEENTRY32W me{sizeof(me)}; uintptr_t base=0;
    if(Module32FirstW(s,&me)){ do{ if(_wcsicmp(me.szModule,mod)==0){base=(uintptr_t)me.modBaseAddr;break;} }while(Module32NextW(s,&me)); }
    CloseHandle(s); return base;
}

static DWORD FindGamePidWithModule(){
    const wchar_t* mods[] = { L"clientmod_game.dll", L"ClientMod_Game.dll", L"clientmod_game.DLL" };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(snap==INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{sizeof(pe)};
    DWORD found=0;
    if(Process32FirstW(snap,&pe)){
        do{
            bool nameOk = (_wcsicmp(pe.szExeFile,L"CMLauncher.exe")==0)
                       || (_wcsicmp(pe.szExeFile,L"cmlauncher.exe")==0);
            for(const wchar_t* m : mods){
                uintptr_t b = GetModuleBase(pe.th32ProcessID, m);
                if(b){
                    if(nameOk){ found=pe.th32ProcessID; break; }
                    if(!found) found=pe.th32ProcessID;
                }
            }
            if(found && nameOk) break;
        }while(Process32NextW(snap,&pe));
    }
    CloseHandle(snap);
    return found;
}

static uintptr_t GetGameModuleBase(DWORD pid){
    const wchar_t* mods[] = { L"clientmod_game.dll", L"ClientMod_Game.dll", L"clientmod_game.DLL" };
    for(const wchar_t* m : mods){
        uintptr_t b = GetModuleBase(pid, m);
        if(b) return b;
    }
    return 0;
}

static void EnsureOffsets(){ Off.loaded=true; }

static void Detach(){
    if(g_hProc){CloseHandle(g_hProc);g_hProc=nullptr;}
    g_gameBase=0;g_attachedPid=0;g_haveLocal=false;g_localTeam=0;g_localHP=0;
    g_dbgCount=0;g_dbgDrawn=0;g_dbgVmOk=false;
    g_rawObjects.clear();g_tracked.clear();g_scanned=false;
    strcpy_s(g_status,"Not attached");
}

static void ScanVTableObjects(bool full_reset=false){
    if(!g_hProc||!g_gameBase)return;
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
    sprintf_s(g_status,"Attached | Objects: %zu",found.size());
}

static bool AttachFromUI(){
    Detach(); EnsureOffsets();
    DWORD pid=0;
    if(UI::IsWindowTargeted()){
        HWND hwnd=UI::GetTargetWindow();
        pid=UI::GetTargetPID();
        if(!pid&&hwnd) GetWindowThreadProcessId(hwnd,&pid);
    }
    uintptr_t base=0;
    if(pid) base=GetGameModuleBase(pid);
    if(!base){
        DWORD gp=FindGamePidWithModule();
        if(gp){ pid=gp; base=GetGameModuleBase(pid); }
    }
    if(!pid){strcpy_s(g_status,"No game process (open CMLauncher)");return false;}
    g_hProc=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,FALSE,pid);
    if(!g_hProc){strcpy_s(g_status,"OpenProcess failed (run as ADMIN)");return false;}
    if(!base){
        sprintf_s(g_status,"Module not loaded (PID %lu) — enter match & Attach again", (unsigned long)pid);
        CloseHandle(g_hProc); g_hProc=nullptr; return false;
    }
    g_gameBase=base;
    g_attachedPid=pid;
    sprintf_s(g_status,"OK PID=%lu base=0x%llX — scanning...", (unsigned long)pid, (unsigned long long)base);
    ScanVTableObjects(true);
    return true;
}

static void UpdateLocal(){
    g_haveLocal=false;g_localTeam=0;g_localHP=0;
    if(!g_hProc||!g_gameBase)return;
    Vec3 pos{};
    if(RPM(g_gameBase+Off.MY_POS_X,pos.x)&&RPM(g_gameBase+Off.MY_POS_Y,pos.y)&&RPM(g_gameBase+Off.MY_POS_Z,pos.z)){
        if(fabsf(pos.x)>0.5f||fabsf(pos.y)>0.5f||fabsf(pos.z)>0.5f){g_localPos=pos;g_haveLocal=true;}
    }
}

static bool WorldToScreen(const Vec3& pos,const float* mat,float sw,float sh,Vec2& out){
    float cx=mat[0]*pos.x+mat[1]*pos.y+mat[2]*pos.z+mat[3];
    float cy=mat[4]*pos.x+mat[5]*pos.y+mat[6]*pos.z+mat[7];
    float cw=mat[12]*pos.x+mat[13]*pos.y+mat[14]*pos.z+mat[15];
    if(fabsf(cw)<0.001f) return false;
    float ndc_x = cx / cw;
    float ndc_y = cy / cw;
    out.x = (sw * 0.5f) * ndc_x + sw * 0.5f;
    out.y = -(sh * 0.5f) * ndc_y + sh * 0.5f;
    return cw > 0.2f;
}

static inline ImU32 C(ImVec4 v){return ImGui::ColorConvertFloat4ToU32(v);}

void RenderESPMenu(){
    if(!UI::IsWindowTargeted())return; EnsureOffsets();
    ImGui::SetNextWindowSize(ImVec2(360,440),ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("farshad pm Esp",nullptr,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysVerticalScrollbar)){ImGui::End();return;}
    ImGui::TextColored(ImVec4(0.4f,0.85f,1.f,1.f),"farshad pm Esp"); ImGui::SameLine(); ImGui::TextDisabled("Python-port");
    ImGui::Separator();
    ImGui::Checkbox("Enable ESP",&cfg.enabled); ImGui::SameLine();
    if(ImGui::Button("Attach")) AttachFromUI();
    ImGui::SameLine(); ImGui::TextDisabled("F5 = Attach/Rescan");
    ImGui::TextColored(ImVec4(0.5f,1.f,0.5f,1.f),"%s",g_status);
    ImGui::TextDisabled("Matrix: 0x27603D0 | Hold: 12");
    ImGui::Separator();
    if(ImGui::BeginTabBar("tabs")){
        if(ImGui::BeginTabItem("Main")){
            ImGui::Checkbox("Team Check",&cfg.teamCheck);
            ImGui::SliderFloat("Max Distance",&cfg.maxDistance,50.f,5000.f,"%.0f m");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Visuals")){
            ImGui::Checkbox("Box",&cfg.drawBox);ImGui::SameLine();ImGui::Checkbox("Corners",&cfg.drawCorners);
            ImGui::Checkbox("Distance",&cfg.drawDistance);
            ImGui::SliderFloat("Thickness",&cfg.thickness,0.5f,6.f,"%.1f");
            ImGui::SliderFloat("Box Height",&cfg.headOffset,30.f,140.f,"%.0f");
            ImGui::SliderFloat("Width Ratio",&cfg.widthRatio,0.15f,0.9f,"%.2f");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Aimbot")){
            ImGui::Checkbox("Enable Aim Assist",&cfg.aimbot);
            ImGui::Separator();
            ImGui::TextDisabled("Activation");
            ImGui::Checkbox("Aim on RMB",&cfg.aimOnRMB);
            ImGui::Checkbox("Aim on LMB",&cfg.aimOnLMB);
            ImGui::Checkbox("Aim on ALT (autofire)",&cfg.aimOnAlt);
            ImGui::Checkbox("Aim on ALT (aim only)",&cfg.aimOnAltAimOnly);
            ImGui::Separator();
            ImGui::SliderFloat("Smooth",&cfg.aimSmooth,1.f,20.f,"%.1f");
            ImGui::SliderFloat("ALT Smooth",&cfg.aimAltSmooth,1.f,25.f,"%.1f");
            ImGui::SliderFloat("ALT Precision",&cfg.aimAltPrecision,1.f,40.f,"%.0f");
            ImGui::SliderFloat("Fire Delay",&cfg.aimFireDelayMs,50.f,1000.f,"%.0f");
            ImGui::SliderFloat("FOV",&cfg.aimFov,0.f,600.f,"%.0f");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem("Status")){
            ImGui::Text("Connected: %s",g_hProc?"Yes":"No");
            ImGui::Text("PID: %lu", (unsigned long)g_attachedPid);
            ImGui::Text("Base: 0x%llX", (unsigned long long)g_gameBase);
            ImGui::Text("Local: %s",g_haveLocal?"Found":"Not found");
            ImGui::Text("Camera: %s",g_dbgVmOk?"OK":"Failed");
            ImGui::Text("Players: %d  Drawn: %d",g_dbgCount,g_dbgDrawn);
            ImGui::Text("Objects: %zu",g_rawObjects.size());
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
    EnsureOffsets();
    if (GetAsyncKeyState(VK_F5) & 1) AttachFromUI();
    if(UI::IsWindowTargeted() && UI::GetTargetWindow()!=g_lastTarget){
        g_lastTarget=UI::GetTargetWindow();
    }
    if(!g_hProc||!g_gameBase) return;
    UpdateLocal(); if(!cfg.enabled||!g_scanned) return;

    ImGuiIO& io=ImGui::GetIO(); const float sw=io.DisplaySize.x, sh=io.DisplaySize.y;
    ImDrawList* dl=ImGui::GetBackgroundDrawList(); if(!dl) return;

    float mat[16]={};
    g_dbgVmOk=RPM(g_gameBase+Off.MATRIX_RVA,mat);
    if(!g_dbgVmOk){
        dl->AddText(ImVec2(20,70),IM_COL32(255,80,80,255),"Camera read failed");
        return;
    }

    g_dbgDrawn=0; g_dbgCount=0;
    float bestD=1e9f,bestX=0,bestY=0; bool hasAim=false;
    ++g_frame;

    for(auto& te : g_tracked){
        Vec3 origin{};
        bool ok = RPM(te.obj+Off.V_OBJ_ORIGIN,origin.x)
               && RPM(te.obj+Off.V_OBJ_ORIGIN+4,origin.y)
               && RPM(te.obj+Off.V_OBJ_ORIGIN+8,origin.z);

        if(!ok){
            if(te.hold>0){ te.hold--; te.valid = (te.hold>0); }
            else te.valid=false;
            if(!te.valid) continue;
            origin = te.pos;
        } else {
            if(fabsf(origin.x)>20000.f||fabsf(origin.y)>20000.f||fabsf(origin.z)>20000.f){ te.valid=false; continue; }
            if(fabsf(origin.x)<0.1f && fabsf(origin.y)<0.1f){ te.valid=false; continue; }
            if(origin.z < -500.f || origin.z > 500.f){ te.valid=false; continue; }

            if(te.hasPrev){
                float dx=origin.x-te.prev.x, dy=origin.y-te.prev.y, dz=origin.z-te.prev.z;
                float d2=dx*dx+dy*dy+dz*dz;
                if(d2 > POS_EPS_SQ) te.staticFrames = 0;
                else te.staticFrames++;
            } else te.staticFrames = 0;
            te.prev = origin;
            te.hasPrev = true;

            if(te.staticFrames > DEAD_STATIC_FRAMES){
                te.valid=false; te.hold=0;
                continue;
            }

            te.pos = origin;
            te.hold = HOLD_FRAMES;
            te.valid = true;
        }

        if(!te.valid) continue;

        float dist = -1.f;
        if(g_haveLocal){
            float dx=te.pos.x-g_localPos.x, dy=te.pos.y-g_localPos.y, dz=te.pos.z-g_localPos.z;
            dist = sqrtf(dx*dx+dy*dy+dz*dz);
            if(dist < MIN_DIST || dist > cfg.maxDistance) continue;
        }

        ++g_dbgCount;

        Vec3 head = te.pos; head.z += cfg.headOffset;
        Vec2 feetS{}, headS{};
        if(!WorldToScreen(te.pos, mat, sw, sh, feetS)) continue;
        if(!WorldToScreen(head, mat, sw, sh, headS)) continue;

        float box_h = fabsf(feetS.y - headS.y);
        if(box_h < MIN_BOX_H || box_h > MAX_BOX_H) continue;
        float box_w = box_h * cfg.widthRatio;
        if(box_w < 2.f) box_w = 2.f;

        float top_y = (feetS.y < headS.y) ? feetS.y : headS.y;
        float cx = (feetS.x + headS.x) * 0.5f;
        if(cx < -200.f || cx > sw+200.f || top_y < -200.f || top_y > sh+200.f) continue;

        ImU32 col = C(cfg.enemyColor);
        if(cfg.drawBox)
            dl->AddRect(ImVec2(cx - box_w*0.5f, top_y), ImVec2(cx + box_w*0.5f, top_y + box_h), col, 0.f, 0, cfg.thickness);

        if(cfg.drawDistance && dist >= 0.f){
            char b[32]; sprintf_s(b,"%.0fm",dist);
            ImVec2 ts=ImGui::CalcTextSize(b);
            dl->AddText(ImVec2(cx - ts.x*0.5f, top_y - 14.f), IM_COL32(255,80,80,230), b);
        }
        ++g_dbgDrawn;

        if(cfg.aimbot){
            float ax=cx, ay=top_y + box_h*0.25f;
            float adx=ax-sw*0.5f, ady=ay-sh*0.5f;
            float ad=sqrtf(adx*adx+ady*ady);
            float fovLimit = cfg.aimFov <= 0.f ? 9999.f : cfg.aimFov;
            if(ad < bestD && ad <= fovLimit){ bestD=ad; bestX=ax; bestY=ay; hasAim=true; }
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
            float sm = useAlt ? cfg.aimAltSmooth : cfg.aimSmooth;
            if(sm < 1.f) sm = 1.f;
            float stepX = dx / sm, stepY = dy / sm;
            const float maxStep = 45.f;
            if(stepX > maxStep) stepX = maxStep; if(stepX < -maxStep) stepX = -maxStep;
            if(stepY > maxStep) stepY = maxStep; if(stepY < -maxStep) stepY = -maxStep;
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
                        click[0].type = INPUT_MOUSE; click[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
                        click[1].type = INPUT_MOUSE; click[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
                        SendInput(2, click, sizeof(INPUT));
                    }
                }
            }
        }
    }
}

void RenderESP(){ RenderESPMenu(); RenderESPBoxes(); }
