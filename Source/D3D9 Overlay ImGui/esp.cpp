// =====================================================================
//  farshad pm Esp — VTable path (proven working)
//  v3.1  anti-blink / anti-ghost / auto-rescan / aim line + LMB snap
// =====================================================================
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cfloat>
#include <string>
#include <vector>
#include "ImGui/imgui.h"
#include "UI.h"
#include "esp.h"

#pragma comment(lib, "shell32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Off {
    // Local
    constexpr uintptr_t MY_POS_X       = 0x242F510;
    constexpr uintptr_t MY_POS_Y       = 0x242F514;
    constexpr uintptr_t MY_POS_Z       = 0x242F518;
    constexpr uintptr_t MY_POS_PTR_RVA = 0x00E1F664;
    constexpr uintptr_t MY_TEAM_PTR_RVA= 0x02E1616C;
    constexpr uintptr_t MY_TEAM_OFFSET = 0x1D8;

    // Matrix (working)
    constexpr uintptr_t MATRIX_RVA     = 0x27603D0;

    // VTable entities
    constexpr uintptr_t VTABLE_1       = 0x01DDAC3C;
    constexpr uintptr_t VTABLE_2       = 0x01DDA9C8;
    constexpr uintptr_t V_OBJ_ORIGIN   = 0x538;
    constexpr uintptr_t OBJ_HP         = 0x11C;
    constexpr uintptr_t OBJ_TEAM       = 0xD4;
}

enum class Lang { EN = 0, FA = 1 };
static Lang g_lang = Lang::EN;

static std::string FaFix(const char* s) {
    if (!s || !*s) return {};
    std::vector<std::string> ch;
    const unsigned char* p = (const unsigned char*)s;
    while (*p) {
        int n = 1;
        if ((*p & 0x80) == 0) n = 1;
        else if ((*p & 0xE0) == 0xC0) n = 2;
        else if ((*p & 0xF0) == 0xE0) n = 3;
        else if ((*p & 0xF8) == 0xF0) n = 4;
        ch.emplace_back((const char*)p, n);
        p += n;
    }
    std::string o;
    for (int i = (int)ch.size() - 1; i >= 0; --i) o += ch[i];
    return o;
}

struct Loc {
    const char* en; const char* fa; std::string buf;
    const char* operator()() {
        if (g_lang != Lang::FA) return en;
        buf = FaFix(fa); return buf.c_str();
    }
};

static Loc L_Title={ "farshad pm Esp","farshad pm Esp" };
static Loc L_Lang={ "Language","زبان" };
static Loc L_Enable={ "Enable ESP","فعال کردن ای‌اس‌پی" };
static Loc L_Rescan={ "Force Rescan","اسکن دوباره" };
static Loc L_Team={ "Team Check","چک تیم" };
static Loc L_MaxD={ "Max Distance","حداکثر فاصله" };
static Loc L_Box={ "Box","باکس" };
static Loc L_Corners={ "Corners","گوشه" };
static Loc L_HP={ "Health Bar","نوار جان" };
static Loc L_Dist={ "Distance","فاصله" };
static Loc L_Thick={ "Thickness","ضخامت" };
static Loc L_BoxH={ "Box Height","ارتفاع باکس" };
static Loc L_Width={ "Width Ratio","نسبت عرض" };
static Loc L_MaxHP={ "Max HP","حداکثر جان" };
static Loc L_AimOn={ "Aim Assist","ایم اسیست" };
static Loc L_AimSmooth={ "Smooth","نرمی" };
static Loc L_AimFov={ "FOV","میدان دید" };
static Loc L_AutoAim={ "Auto Aim (Alt)","اتو ایم (آلت)" };
static Loc L_AutoFire={ "Auto Fire (Alt)","اتو فایر (آلت)" };
static Loc L_Enemy={ "Enemy","دشمن" };
static Loc L_Mate={ "Mate","همتیمی" };
static Loc L_TG={ "Telegram:","تلگرام:" };
static Loc L_OpenTG={ "Open Telegram","باز کردن تلگرام" };
static Loc L_CopyTG={ "Copy @client_v34","کپی شناسه" };
static Loc L_TabMain={ "Main","اصلی" };
static Loc L_TabVis={ "Visuals","نمایش" };
static Loc L_TabAim={ "Aim","ایم" };
static Loc L_TabCol={ "Colors","رنگ" };
static Loc L_TabSt={ "Status","وضعیت" };
static Loc L_TabCt={ "Contact","ارتباط" };
static Loc L_Conn={ "Connected","اتصال" };
static Loc L_Loc={ "Local","خودی" };
static Loc L_Cam={ "Camera","دوربین" };
static Loc L_Pl={ "Players","بازیکن" };
static Loc L_Dr={ "Drawn","رسم شده" };
static Loc L_Yes={ "Yes","بله" };
static Loc L_No={ "No","خیر" };
static Loc L_OK={ "OK","موفق" };
static Loc L_Fail={ "Fail","ناموفق" };

struct Vec3 { float x,y,z; };
struct Vec2 { float x,y; };

struct Tracked {
    uintptr_t obj = 0;
    Vec3 pos{};
    int hp = 0, team = 0;
    int life = 0;          // frames left if read fails (anti-blink)
    int badReads = 0;      // consecutive invalid → drop (anti-ghost)
    bool valid = false;
};

struct Cfg {
    bool  enabled = true;
    bool  teamCheck = false;
    bool  drawBox = true;
    bool  drawCorners = false;
    bool  drawHealth = false;
    bool  drawDistance = true;
    bool  aimAssist = true;     // show FOV + line + wait for LMB snap
    bool  autoAim = false;      // while Alt: continuously pull to target
    bool  autoFire = false;     // while Alt: click when on target
    float aimSmooth = 3.5f;
    float aimFov = 200.f;
    float headZ = 72.f;
    float widthRatio = 0.42f;
    float thickness = 1.8f;
    float maxDist = 2000.f;
    float healthMax = 100.f;
    int   healthSide = 0;
    ImVec4 enemy = {1.f,0.25f,0.25f,1.f};
    ImVec4 mate  = {0.25f,0.85f,1.f,1.f};
} cfg;

static HANDLE g_hProc = nullptr;
static uintptr_t g_base = 0;
static DWORD g_pid = 0;
static HWND g_lastHwnd = nullptr;
static Vec3 g_local{};
static bool g_haveLocal = false;
static int g_localTeam = 0;
static char g_status[160] = "Not attached";
static int g_count = 0, g_drawn = 0;
static bool g_matOk = false;

static std::vector<uintptr_t> g_raw;
static std::vector<Tracked> g_tr;
static bool g_scanned = false;
static double g_lastScan = 0;
static const int kMaxLife = 12;       // keep drawing briefly if one read fails
static const int kMaxBad  = 6;        // drop after N consecutive invalid reads
static const double kRescanSec = 8.0; // auto rescan interval

template<typename T>
static bool RPM(uintptr_t a, T& o) {
    SIZE_T n=0;
    return ReadProcessMemory(g_hProc,(LPCVOID)a,&o,sizeof(T),&n)&&n==sizeof(T);
}
static bool ReadPtr32(uintptr_t a, uintptr_t& o) {
    uint32_t t=0; if(!RPM(a,t)||t<0x10000) return false; o=t; return true;
}

static uintptr_t ModBase(DWORD pid, const wchar_t* name) {
    HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid);
    if(s==INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me{sizeof(me)}; uintptr_t b=0;
    if(Module32FirstW(s,&me)) do {
        if(_wcsicmp(me.szModule,name)==0){ b=(uintptr_t)me.modBaseAddr; break; }
    } while(Module32NextW(s,&me));
    CloseHandle(s); return b;
}

static void Detach() {
    if(g_hProc){ CloseHandle(g_hProc); g_hProc=nullptr; }
    g_base=0; g_pid=0; g_haveLocal=false; g_localTeam=0;
    g_count=g_drawn=0; g_matOk=false;
    g_raw.clear(); g_tr.clear(); g_scanned=false; g_lastScan=0;
    strcpy_s(g_status, "Not attached");
}

static void ScanVTables() {
    g_raw.clear(); g_tr.clear(); g_scanned=false;
    if(!g_hProc||!g_base) return;

    const uint32_t targets[2] = {
        (uint32_t)(g_base + Off::VTABLE_1),
        (uint32_t)(g_base + Off::VTABLE_2)
    };

    const uintptr_t start=0x10000000, end=0x40000000;
    const size_t chunk=0x100000;
    std::vector<uint8_t> buf(chunk);

    for(uintptr_t addr=start; addr<end; addr+=chunk) {
        SIZE_T got=0;
        if(!ReadProcessMemory(g_hProc,(LPCVOID)addr,buf.data(),chunk,&got)||got<16) continue;
        for(size_t i=0;i+4<=got;i+=4) {
            uint32_t v=*reinterpret_cast<uint32_t*>(buf.data()+i);
            if(v==targets[0]||v==targets[1]) g_raw.push_back(addr+i);
        }
    }
    g_tr.reserve(g_raw.size());
    for(uintptr_t o: g_raw) {
        Tracked t; t.obj=o; g_tr.push_back(t);
    }
    g_scanned=true;
    g_lastScan=ImGui::GetTime();
    sprintf_s(g_status, g_lang==Lang::FA ? "متصل | %zu" : "Attached | %zu", g_raw.size());
}

static bool Attach() {
    Detach();
    if(!UI::IsWindowTargeted()){ strcpy_s(g_status,"No target"); return false; }
    HWND hw=UI::GetTargetWindow();
    DWORD pid=UI::GetTargetPID();
    if(!pid&&hw) GetWindowThreadProcessId(hw,&pid);
    if(!pid){ strcpy_s(g_status,"PID invalid"); return false; }
    g_hProc=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,FALSE,pid);
    if(!g_hProc){ strcpy_s(g_status,"OpenProcess fail (admin)"); return false; }
    g_base=ModBase(pid,L"clientmod_game.dll");
    if(!g_base){
        strcpy_s(g_status,"dll not loaded");
        CloseHandle(g_hProc); g_hProc=nullptr; return false;
    }
    g_pid=pid;
    strcpy_s(g_status,"Scanning...");
    ScanVTables();
    return true;
}

static void UpdateLocal() {
    g_haveLocal=false; g_localTeam=0;
    if(!g_hProc||!g_base) return;
    uintptr_t pp=0;
    if(ReadPtr32(g_base+Off::MY_POS_PTR_RVA,pp)) {
        Vec3 p{};
        if(RPM(pp,p.x)&&RPM(pp+4,p.y)&&RPM(pp+8,p.z))
            if(fabsf(p.x)>0.5f||fabsf(p.y)>0.5f){ g_local=p; g_haveLocal=true; }
    }
    if(!g_haveLocal) {
        Vec3 p{};
        if(RPM(g_base+Off::MY_POS_X,p.x)&&RPM(g_base+Off::MY_POS_Y,p.y)&&RPM(g_base+Off::MY_POS_Z,p.z))
            if(fabsf(p.x)>0.5f||fabsf(p.y)>0.5f){ g_local=p; g_haveLocal=true; }
    }
    uintptr_t tb=0;
    if(ReadPtr32(g_base+Off::MY_TEAM_PTR_RVA,tb)) {
        int t=0; if(RPM(tb+Off::MY_TEAM_OFFSET,t)) g_localTeam=t;
    }
}

static bool W2S(const Vec3& pos, const float* m, float sw, float sh, Vec2& o) {
    float cx=m[0]*pos.x+m[1]*pos.y+m[2]*pos.z+m[3];
    float cy=m[4]*pos.x+m[5]*pos.y+m[6]*pos.z+m[7];
    float cw=m[12]*pos.x+m[13]*pos.y+m[14]*pos.z+m[15];
    if(fabsf(cw)<0.001f||cw<0.15f) return false;
    float nx=cx/cw, ny=cy/cw;
    o.x=sw*0.5f*(1.f+nx);
    o.y=sh*0.5f*(1.f-ny);
    return true;
}

static inline ImU32 C(ImVec4 v){ return ImGui::ColorConvertFloat4ToU32(v); }

static void DrawCorners(ImDrawList* dl,float x,float y,float w,float h,ImU32 c,float t){
    float l=(w<h?w:h)*0.28f; if(l<4)l=4;
    dl->AddLine(ImVec2(x,y),ImVec2(x+l,y),c,t);
    dl->AddLine(ImVec2(x,y),ImVec2(x,y+l),c,t);
    dl->AddLine(ImVec2(x+w,y),ImVec2(x+w-l,y),c,t);
    dl->AddLine(ImVec2(x+w,y),ImVec2(x+w,y+l),c,t);
    dl->AddLine(ImVec2(x,y+h),ImVec2(x+l,y+h),c,t);
    dl->AddLine(ImVec2(x,y+h),ImVec2(x,y+h-l),c,t);
    dl->AddLine(ImVec2(x+w,y+h),ImVec2(x+w-l,y+h),c,t);
    dl->AddLine(ImVec2(x+w,y+h),ImVec2(x+w,y+h-l),c,t);
}

static void DrawHP(ImDrawList* dl,float bx,float by,float w,float h,float f,int side){
    f=f<0?0:(f>1?1:f);
    ImU32 hc=f>0.6f?IM_COL32(60,220,60,255):f>0.3f?IM_COL32(240,220,40,255):IM_COL32(240,60,60,255);
    ImU32 bg=IM_COL32(0,0,0,180);
    if(side<=1){
        float bw=3.5f,gap=4.f;
        float x=side==0?bx-bw-gap:bx+w+gap;
        dl->AddRectFilled(ImVec2(x,by),ImVec2(x+bw,by+h),bg);
        float fill=h*f;
        dl->AddRectFilled(ImVec2(x,by+h-fill),ImVec2(x+bw,by+h),hc);
    } else {
        float bh=3.5f,gap=4.f;
        float y=side==2?by-bh-gap:by+h+gap;
        dl->AddRectFilled(ImVec2(bx,y),ImVec2(bx+w,y+bh),bg);
        dl->AddRectFilled(ImVec2(bx,y),ImVec2(bx+w*f,y+bh),hc);
    }
}

static void MoveRel(int dx,int dy){
    if(!dx&&!dy)return;
    INPUT in{}; in.type=INPUT_MOUSE; in.mi.dx=dx; in.mi.dy=dy; in.mi.dwFlags=MOUSEEVENTF_MOVE;
    SendInput(1,&in,sizeof(INPUT));
}
static void ClickOnce(){
    INPUT in[2]{};
    in[0].type=INPUT_MOUSE; in[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;
    in[1].type=INPUT_MOUSE; in[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    SendInput(2,in,sizeof(INPUT));
}

// smooth pull toward target
static void PullTo(float tx,float ty,float sw,float sh,float smooth){
    float dx=tx-sw*0.5f, dy=ty-sh*0.5f;
    float d=sqrtf(dx*dx+dy*dy);
    if(d<1.2f) return;
    float s=smooth<1.f?1.f:smooth;
    float sx=dx/s, sy=dy/s;
    if(sx>45)sx=45; if(sx<-45)sx=-45;
    if(sy>45)sy=45; if(sy<-45)sy=-45;
    int mx=(int)(sx>=0?sx+0.5f:sx-0.5f);
    int my=(int)(sy>=0?sy+0.5f:sy-0.5f);
    if(!mx&&fabsf(dx)>2) mx=dx>0?1:-1;
    if(!my&&fabsf(dy)>2) my=dy>0?1:-1;
    MoveRel(mx,my);
}

// hard snap (for LMB)
static void SnapTo(float tx,float ty,float sw,float sh){
    float dx=tx-sw*0.5f, dy=ty-sh*0.5f;
    // multi-step snap so it lands closer in one frame budget
    for(int i=0;i<3;i++){
        int mx=(int)(dx*(0.55f+i*0.15f));
        int my=(int)(dy*(0.55f+i*0.15f));
        if(mx>80)mx=80; if(mx<-80)mx=-80;
        if(my>80)my=80; if(my<-80)my=-80;
        MoveRel(mx,my);
    }
}

static int g_rot=-1;
static void RStart(ImDrawList* dl){ g_rot=dl->VtxBuffer.Size; }
static void REnd(ImDrawList* dl,float rad,ImVec2 c){
    float s=sinf(rad),co=cosf(rad);
    for(int i=g_rot;i<dl->VtxBuffer.Size;i++){
        auto& v=dl->VtxBuffer.Data[i];
        float px=v.pos.x-c.x,py=v.pos.y-c.y;
        v.pos.x=px*co-py*s+c.x; v.pos.y=px*s+py*co+c.y;
    }
}
static void Logo(){
    ImDrawList* dl=ImGui::GetBackgroundDrawList(); if(!dl)return;
    float px=28,py=28,r=22,a=(float)ImGui::GetTime()*2.1f;
    dl->AddCircleFilled(ImVec2(px,py),r+4,IM_COL32(0,0,0,120),32);
    dl->AddCircle(ImVec2(px,py),r+2,IM_COL32(60,180,255,180),32,1.5f);
    for(int i=0;i<48;i++){
        float a0=a+i/48.f*2.f*(float)M_PI, a1=a+(i+1)/48.f*2.f*(float)M_PI;
        float al=80+120*(0.5f+0.5f*sinf(a0*2));
        dl->AddLine(ImVec2(px+cosf(a0)*r,py+sinf(a0)*r),ImVec2(px+cosf(a1)*r,py+sinf(a1)*r),IM_COL32(80,200,255,(int)al),2.f);
    }
    const char* t="F ESP"; ImFont* f=ImGui::GetFont();
    RStart(dl);
    ImVec2 ts=f->CalcTextSizeA(14.f,FLT_MAX,0,t);
    ImVec2 tp(px-ts.x*0.5f,py-ts.y*0.5f);
    dl->AddText(f,14.f,ImVec2(tp.x+1,tp.y+1),IM_COL32(0,0,0,200),t);
    dl->AddText(f,14.f,tp,IM_COL32(100,220,255,255),t);
    REnd(dl,a,ImVec2(px,py));
}

void RenderESPMenu(){
    if(!UI::IsWindowTargeted()) return;
    ImGui::SetNextWindowSize(ImVec2(360,440),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320,280),ImVec2(520,720));
    if(!ImGui::Begin(L_Title(),nullptr,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysVerticalScrollbar)){
        ImGui::End(); return;
    }
    ImGui::TextColored(ImVec4(0.4f,0.85f,1.f,1.f),"%s",L_Title());
    ImGui::SameLine(); ImGui::TextDisabled("v3.1");
    ImGui::Separator();

    ImGui::Text("%s",L_Lang()); ImGui::SameLine();
    if(ImGui::RadioButton("EN",g_lang==Lang::EN)) g_lang=Lang::EN;
    ImGui::SameLine();
    if(ImGui::RadioButton("FA",g_lang==Lang::FA)) g_lang=Lang::FA;

    ImGui::Checkbox(L_Enable(),&cfg.enabled);
    ImGui::SameLine();
    if(ImGui::Button(L_Rescan())) { if(g_hProc) ScanVTables(); else Attach(); }
    ImGui::TextColored(ImVec4(0.5f,1.f,0.5f,1.f),"%s",g_status);
    ImGui::Separator();

    if(ImGui::BeginTabBar("t",ImGuiTabBarFlags_FittingPolicyScroll)){
        if(ImGui::BeginTabItem(L_TabMain())){
            ImGui::Checkbox(L_Team(),&cfg.teamCheck);
            ImGui::SliderFloat(L_MaxD(),&cfg.maxDist,50.f,4000.f,"%.0f");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem(L_TabVis())){
            ImGui::Checkbox(L_Box(),&cfg.drawBox); ImGui::SameLine();
            ImGui::Checkbox(L_Corners(),&cfg.drawCorners);
            ImGui::Checkbox(L_HP(),&cfg.drawHealth); ImGui::SameLine();
            ImGui::Checkbox(L_Dist(),&cfg.drawDistance);
            ImGui::SliderFloat(L_Thick(),&cfg.thickness,0.5f,6.f,"%.1f");
            ImGui::SliderFloat(L_BoxH(),&cfg.headZ,30.f,120.f,"%.0f");
            ImGui::SliderFloat(L_Width(),&cfg.widthRatio,0.15f,0.9f,"%.2f");
            ImGui::SliderFloat(L_MaxHP(),&cfg.healthMax,50.f,200.f,"%.0f");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem(L_TabAim())){
            ImGui::Checkbox(L_AimOn(),&cfg.aimAssist);
            ImGui::SliderFloat(L_AimSmooth(),&cfg.aimSmooth,1.f,12.f,"%.1f");
            ImGui::SliderFloat(L_AimFov(),&cfg.aimFov,40.f,500.f,"%.0f");
            ImGui::Separator();
            ImGui::Checkbox(L_AutoAim(),&cfg.autoAim);
            ImGui::Checkbox(L_AutoFire(),&cfg.autoFire);
            ImGui::TextDisabled(g_lang==Lang::FA ?
                FaFix("Alt نگه دار = اتو | کلیک چپ = اسنپ سریع").c_str() :
                "Hold Alt = auto | LMB = fast snap");
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem(L_TabCol())){
            ImGui::ColorEdit4(L_Enemy(),(float*)&cfg.enemy);
            ImGui::ColorEdit4(L_Mate(),(float*)&cfg.mate);
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem(L_TabSt())){
            ImGui::Text("%s : %s",L_Conn(),g_hProc?L_Yes():L_No());
            ImGui::Text("%s : %s",L_Loc(),g_haveLocal?L_OK():L_Fail());
            ImGui::Text("%s : %s",L_Cam(),g_matOk?L_OK():L_Fail());
            ImGui::Text("%s : %d",L_Pl(),g_count);
            ImGui::Text("%s : %d",L_Dr(),g_drawn);
            ImGui::Text("Tracked : %zu", g_tr.size());
            ImGui::EndTabItem();
        }
        if(ImGui::BeginTabItem(L_TabCt())){
            ImGui::Text("%s",L_TG()); ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f,0.7f,1.f,1.f),"@client_v34");
            if(ImGui::Button(L_OpenTG(),ImVec2(-1,0)))
                ShellExecuteA(nullptr,"open","https://t.me/client_v34",nullptr,nullptr,SW_SHOWNORMAL);
            if(ImGui::Button(L_CopyTG(),ImVec2(-1,0))){
                if(OpenClipboard(nullptr)){
                    EmptyClipboard();
                    const char* t="@client_v34";
                    HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,strlen(t)+1);
                    if(h){ memcpy(GlobalLock(h),t,strlen(t)+1); GlobalUnlock(h); SetClipboardData(CF_TEXT,h); }
                    CloseClipboard();
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void RenderESPBoxes(){
    if(UI::IsWindowTargeted()) Logo();

    if(UI::GetTargetWindow()!=g_lastHwnd){
        g_lastHwnd=UI::GetTargetWindow();
        if(g_lastHwnd) Attach(); else Detach();
    }
    if((!g_hProc||!g_base)&&UI::IsWindowTargeted()) Attach();
    if(!g_hProc||!g_base) return;

    // auto rescan every kRescanSec
    double now=ImGui::GetTime();
    if(g_scanned && (now-g_lastScan)>kRescanSec)
        ScanVTables();

    UpdateLocal();
    if(!cfg.enabled||!g_scanned) return;

    ImGuiIO& io=ImGui::GetIO();
    const float sw=io.DisplaySize.x, sh=io.DisplaySize.y;
    ImDrawList* dl=ImGui::GetBackgroundDrawList();
    if(!dl) return;

    float mat[16]{};
    g_matOk=RPM(g_base+Off::MATRIX_RVA,mat);
    if(!g_matOk){ dl->AddText(ImVec2(20,70),IM_COL32(255,80,80,255),"Matrix fail"); return; }

    g_count=g_drawn=0;
    float bestD=1e9f, bestX=0, bestY=0;
    bool hasT=false;

    for(auto& te : g_tr){
        Vec3 origin{};
        bool ok = RPM(te.obj+Off::V_OBJ_ORIGIN, origin.x) &&
                  RPM(te.obj+Off::V_OBJ_ORIGIN+4, origin.y) &&
                  RPM(te.obj+Off::V_OBJ_ORIGIN+8, origin.z);

        int hp=0, team=0;
        if(ok){
            RPM(te.obj+Off::OBJ_HP, hp);
            RPM(te.obj+Off::OBJ_TEAM, team);

            // hard invalid → ghost prevention
            bool badPos = (fabsf(origin.x)>30000.f||fabsf(origin.y)>30000.f||fabsf(origin.z)>30000.f)
                       || (fabsf(origin.x)<0.05f && fabsf(origin.y)<0.05f);
            bool badHp  = (hp<=0 || hp>500);

            if(badPos || badHp){
                ok=false;
            }
        }

        if(ok){
            // position jump filter (teleport / reused memory) — treat as new sample
            if(te.valid){
                float jdx=origin.x-te.pos.x, jdy=origin.y-te.pos.y, jdz=origin.z-te.pos.z;
                float jump=sqrtf(jdx*jdx+jdy*jdy+jdz*jdz);
                if(jump>800.f){ // reused slot far away — reset
                    te.badReads=0;
                }
            }
            te.pos=origin; te.hp=hp; te.team=team;
            te.life=kMaxLife; te.badReads=0; te.valid=true;
        } else {
            te.badReads++;
            if(te.life>0) te.life--;
            // drop completely after consecutive bad reads (kills ghost boxes)
            if(te.badReads>=kMaxBad || te.life<=0)
                te.valid=false;
        }

        if(!te.valid) continue;

        float dist=-1.f;
        if(g_haveLocal){
            float dx=te.pos.x-g_local.x, dy=te.pos.y-g_local.y, dz=te.pos.z-g_local.z;
            dist=sqrtf(dx*dx+dy*dy+dz*dz);
            if(dist<40.f) continue;
            if(dist>cfg.maxDist) continue;
        }

        if(cfg.teamCheck && g_localTeam && te.team==g_localTeam) continue;

        ++g_count;

        Vec3 head=te.pos; head.z+=cfg.headZ;
        Vec2 feet, hs;
        if(!W2S(te.pos,mat,sw,sh,feet)) continue;
        if(!W2S(head,mat,sw,sh,hs)) continue;

        float h=feet.y-hs.y;
        if(h<6.f||h>1800.f) continue;
        float w=h*cfg.widthRatio; if(w<2.f)w=2.f;
        float bx=hs.x-w*0.5f, by=hs.y;

        ImU32 col=C(cfg.enemy);

        if(cfg.drawBox)
            dl->AddRect(ImVec2(bx,by),ImVec2(bx+w,by+h),col,0,0,cfg.thickness);
        if(cfg.drawCorners)
            DrawCorners(dl,bx,by,w,h,col,cfg.thickness+0.5f);
        if(cfg.drawHealth && te.hp>0)
            DrawHP(dl,bx,by,w,h,(float)te.hp/cfg.healthMax,cfg.healthSide);
        if(cfg.drawDistance && dist>=0.f){
            char b[24]; sprintf_s(b,"%.0fm",dist);
            ImVec2 ts=ImGui::CalcTextSize(b);
            dl->AddText(ImVec2(bx+w*0.5f-ts.x*0.5f,by+h+2),IM_COL32(255,255,255,230),b);
        }
        ++g_drawn;

        // aim candidate — upper body
        float ax=hs.x, ay=hs.y+h*0.22f;
        float adx=ax-sw*0.5f, ady=ay-sh*0.5f;
        float ad=sqrtf(adx*adx+ady*ady);
        if(ad<bestD && ad<=cfg.aimFov){
            bestD=ad; bestX=ax; bestY=ay; hasT=true;
        }
    }

    // ---- Aim visuals + logic ----
    const float cx=sw*0.5f, cy=sh*0.5f;
    bool alt = (GetAsyncKeyState(VK_MENU)&0x8000)!=0;
    bool lmb = (GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;

    if(cfg.aimAssist || cfg.autoAim){
        // FOV circle
        dl->AddCircle(ImVec2(cx,cy), cfg.aimFov, IM_COL32(0,255,100,50), 64, 1.2f);
    }

    if(hasT && (cfg.aimAssist||cfg.autoAim||cfg.autoFire)){
        // green cross on target
        dl->AddLine(ImVec2(bestX-9,bestY),ImVec2(bestX+9,bestY),IM_COL32(0,255,0,230),1.6f);
        dl->AddLine(ImVec2(bestX,bestY-9),ImVec2(bestX,bestY+9),IM_COL32(0,255,0,230),1.6f);

        // green line from FOV edge toward target (from crosshair through FOV radius point)
        float dx=bestX-cx, dy=bestY-cy;
        float len=sqrtf(dx*dx+dy*dy);
        if(len>1.f){
            float ux=dx/len, uy=dy/len;
            // start at FOV circle edge
            float sx=cx+ux*cfg.aimFov, sy=cy+uy*cfg.aimFov;
            // only draw if target is outside or on FOV — if inside, line from center
            if(len > cfg.aimFov)
                dl->AddLine(ImVec2(sx,sy), ImVec2(bestX,bestY), IM_COL32(0,255,0,180), 1.4f);
            else
                dl->AddLine(ImVec2(cx,cy), ImVec2(bestX,bestY), IM_COL32(0,255,0,160), 1.2f);
        }

        // LMB = fast snap to green cross (aim assist mode)
        if(cfg.aimAssist && lmb && !alt)
            SnapTo(bestX,bestY,sw,sh);

        // Alt held → auto aim / auto fire
        if(alt){
            if(cfg.autoAim)
                PullTo(bestX,bestY,sw,sh,cfg.aimSmooth);
            if(cfg.autoFire && bestD < 12.f)
                ClickOnce();
        }
    }
}

void RenderESP(){
    RenderESPMenu();
    RenderESPBoxes();
}
