// =====================================================================
//  farshad pm Esp  v3.3
//  Player-array path (from working Python V8)
//  Anti-flicker: matrix LOCK + screen smooth + hold last boxes
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
#include <unordered_map>
#include "ImGui/imgui.h"
#include "UI.h"
#include "esp.h"

#pragma comment(lib, "shell32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Offsets matching esp_ultra_v8.py (working boxes)
namespace Off {
    constexpr uintptr_t MY_POS_X = 0x242F510;
    constexpr uintptr_t MY_POS_Y = 0x242F514;
    constexpr uintptr_t MY_POS_Z = 0x242F518;
    constexpr uintptr_t MY_YAW   = 0x25982CC;
    constexpr uintptr_t MY_PITCH = 0x25982D0;

    constexpr uintptr_t MATRIX_RVA     = 0x27603B0;
    constexpr uintptr_t MATRIX_RVA_ALT = 0x27603D0;
    constexpr uintptr_t RENDERER_RVA   = 0x275FCE0;
    constexpr uintptr_t REN_MAT_OFF    = 0x6D0;

    constexpr uintptr_t PLAYER_ARRAY_RVA = 0x26B2364;
    constexpr uintptr_t ARR_STRIDE       = 0x14;
    constexpr uintptr_t ARR_OBJ_OFF      = 0x0C;
    constexpr uintptr_t OBJ_POS_OFF      = 0x290;
    constexpr uintptr_t OBJ_HP_OFF       = 0x11C;
    constexpr uintptr_t OBJ_TEAM_OFF     = 0xD4;

    constexpr uintptr_t PLAYER_RES_RVA = 0x2478218;
    constexpr uintptr_t RES_HP         = 0x1270;
    constexpr uintptr_t RES_TEAM       = 0x1128;

    constexpr int MAX_SLOTS = 64;
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
static Loc L_Team={ "Team Check","چک تیم" };
static Loc L_MaxD={ "Max Distance","حداکثر فاصله" };
static Loc L_Box={ "Box","باکس" };
static Loc L_Corners={ "Corners","گوشه" };
static Loc L_HP={ "Health Bar","نوار جان" };
static Loc L_Dist={ "Distance","فاصله" };
static Loc L_Thick={ "Thickness","ضخامت" };
static Loc L_BoxH={ "Box Height","ارتفاع باکس" };
static Loc L_Width={ "Width Ratio","نسبت عرض" };
static Loc L_Smooth={ "Anti-Flicker","ضد چشمک" };
static Loc L_Hold={ "Hold Frames","فریم نگه‌دار" };
static Loc L_AimOn={ "Aim Assist","ایم اسیست" };
static Loc L_AimSm={ "Aim Smooth","نرمی ایم" };
static Loc L_Fov={ "FOV","میدان دید" };
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
static Loc L_Yes={ "Yes","بله" };
static Loc L_No={ "No","خیر" };
static Loc L_OK={ "OK","موفق" };
static Loc L_Fail={ "Fail","ناموفق" };

struct Vec3 { float x,y,z; };
struct Vec2 { float x,y; };

struct Player {
    int slot = -1;
    uintptr_t ptr = 0;
    Vec3 origin{};
    int hp = 0, team = 0;
};

// Screen-space hold (anti-flicker like V8)
struct HeldBox {
    float fx, fy, hx, hy;
    int left;
    int hp, team;
    float dist;
};

struct Cfg {
    bool  enabled = true;
    bool  teamCheck = true;
    bool  drawBox = true;
    bool  drawCorners = true;
    bool  drawHealth = true;
    bool  drawDistance = true;
    bool  smoothing = true;
    float smoothFactor = 0.70f; // higher = less flicker
    int   holdFrames = 8;
    bool  aimAssist = true;
    bool  autoAim = false;
    bool  autoFire = false;
    float aimSmooth = 3.5f;
    float aimFov = 200.f;
    float headZ = 72.f;
    float widthRatio = 0.45f;
    float thickness = 2.f;
    float maxDist = 3000.f;
    float healthMax = 100.f;
    ImVec4 enemy = {1.f,0.25f,0.25f,1.f};
    ImVec4 mate  = {0.25f,0.85f,1.f,1.f};
} cfg;

static HANDLE g_hProc = nullptr;
static uintptr_t g_base = 0;
static DWORD g_pid = 0;
static HWND g_lastHwnd = nullptr;
static Vec3 g_local{};
static bool g_haveLocal = false;
static int g_mySlot = -1, g_myTeam = 0;
static char g_status[160] = "Not attached";
static int g_count = 0, g_drawn = 0;
static bool g_matOk = false;
static char g_matSrc[32] = "?";

// Matrix lock (V8 anti-flicker core)
static uintptr_t g_lockedMatAddr = 0;
static char g_lockedMatName[16] = "";
static float g_lastGoodMat[16]{};
static bool g_haveLastMat = false;

static std::unordered_map<int, Vec2> g_smoothFeet;
static std::unordered_map<int, Vec2> g_smoothHead;
static std::unordered_map<int, HeldBox> g_hold;

template<typename T>
static bool RPM(uintptr_t a, T& o) {
    SIZE_T n = 0;
    return ReadProcessMemory(g_hProc, (LPCVOID)a, &o, sizeof(T), &n) && n == sizeof(T);
}
static bool ReadPtr32(uintptr_t a, uintptr_t& o) {
    uint32_t t = 0;
    if (!RPM(a, t) || t < 0x10000) return false;
    o = t; return true;
}

static uintptr_t ModBase(DWORD pid, const wchar_t* name) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (s == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me{ sizeof(me) };
    uintptr_t b = 0;
    if (Module32FirstW(s, &me)) {
        do {
            if (_wcsicmp(me.szModule, name) == 0) { b = (uintptr_t)me.modBaseAddr; break; }
        } while (Module32NextW(s, &me));
    }
    CloseHandle(s);
    return b;
}

static void Detach() {
    if (g_hProc) { CloseHandle(g_hProc); g_hProc = nullptr; }
    g_base = 0; g_pid = 0;
    g_haveLocal = false; g_mySlot = -1; g_myTeam = 0;
    g_count = g_drawn = 0; g_matOk = false;
    g_lockedMatAddr = 0; g_haveLastMat = false;
    g_smoothFeet.clear(); g_smoothHead.clear(); g_hold.clear();
    strcpy_s(g_status, "Not attached");
}

static bool Attach() {
    Detach();
    if (!UI::IsWindowTargeted()) { strcpy_s(g_status, "No target"); return false; }
    HWND hw = UI::GetTargetWindow();
    DWORD pid = UI::GetTargetPID();
    if (!pid && hw) GetWindowThreadProcessId(hw, &pid);
    if (!pid) { strcpy_s(g_status, "PID invalid"); return false; }
    g_hProc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!g_hProc) { strcpy_s(g_status, "OpenProcess fail (admin)"); return false; }
    g_base = ModBase(pid, L"clientmod_game.dll");
    if (!g_base) {
        strcpy_s(g_status, "dll not loaded");
        CloseHandle(g_hProc); g_hProc = nullptr; return false;
    }
    g_pid = pid;
    strcpy_s(g_status, "Attached (array)");
    return true;
}

static void UpdateLocal() {
    g_haveLocal = false;
    if (!g_hProc || !g_base) return;
    Vec3 p{};
    if (RPM(g_base + Off::MY_POS_X, p.x) &&
        RPM(g_base + Off::MY_POS_Y, p.y) &&
        RPM(g_base + Off::MY_POS_Z, p.z)) {
        if (fabsf(p.x) > 0.5f || fabsf(p.y) > 0.5f) {
            g_local = p;
            g_haveLocal = true;
        }
    }
}

static float ScoreMatrix(const float* m) {
    if (!m) return -1000.f;
    int nz = 0;
    for (int i = 0; i < 16; i++) {
        float c = m[i];
        if (c != c || fabsf(c) > 90000.f) return -1000.f;
        if (fabsf(c) > 0.001f) nz++;
    }
    if (nz < 5) return -400.f;
    float s = (float)(nz < 12 ? nz : 12);
    if (fabsf(m[15] - 1.f) < 0.4f) s += 8.f;
    return s;
}

// V8 matrix lock — do not switch source every frame
static bool GetMatrixLocked(float* out16) {
    struct Cand { const char* name; uintptr_t addr; };
    Cand cands[] = {
        { "3B0", g_base + Off::MATRIX_RVA },
        { "ALT", g_base + Off::MATRIX_RVA_ALT },
        { "6D0", g_base + Off::RENDERER_RVA + Off::REN_MAT_OFF },
    };

    if (g_lockedMatAddr) {
        float m[16]{};
        if (RPM(g_lockedMatAddr, m) && ScoreMatrix(m) >= 8.f) {
            memcpy(out16, m, 64);
            memcpy(g_lastGoodMat, m, 64);
            g_haveLastMat = true;
            sprintf_s(g_matSrc, "LOCK:%s", g_lockedMatName);
            return true;
        }
        g_lockedMatAddr = 0;
    }

    float bestScore = -1.f;
    float bestMat[16]{};
    uintptr_t bestAddr = 0;
    const char* bestName = "?";
    for (auto& c : cands) {
        float m[16]{};
        if (!RPM(c.addr, m)) continue;
        float sc = ScoreMatrix(m);
        if (sc < 8.f) continue;
        if (sc > bestScore) {
            bestScore = sc;
            memcpy(bestMat, m, 64);
            bestAddr = c.addr;
            bestName = c.name;
        }
    }
    if (bestAddr) {
        g_lockedMatAddr = bestAddr;
        strcpy_s(g_lockedMatName, bestName);
        memcpy(out16, bestMat, 64);
        memcpy(g_lastGoodMat, bestMat, 64);
        g_haveLastMat = true;
        sprintf_s(g_matSrc, "LOCK:%s", bestName);
        return true;
    }
    if (g_haveLastMat) {
        memcpy(out16, g_lastGoodMat, 64);
        strcpy_s(g_matSrc, "CACHE");
        return true;
    }
    strcpy_s(g_matSrc, "FAIL");
    return false;
}

// Same W2S as working V6/V8 Python
static bool W2S(const Vec3& pos, const float* mat, float sw, float sh, Vec2& out) {
    float x = pos.x, y = pos.y, z = pos.z;
    float cx = mat[0]*x + mat[1]*y + mat[2]*z + mat[3];
    float cy = mat[4]*x + mat[5]*y + mat[6]*z + mat[7];
    float cw = mat[12]*x + mat[13]*y + mat[14]*z + mat[15];
    if (fabsf(cw) < 0.001f) return false;
    float ndc_x = cx / cw;
    float ndc_y = cy / cw;
    // V8 formula (accurate for this game)
    out.x = (sw / 2.f * ndc_x) + (ndc_x + sw / 2.f);
    out.y = -(sh / 2.f * ndc_y) + (ndc_y + sh / 2.f);
    return cw > 0.2f;
}

static bool ValidPos(const Vec3& v) {
    if (v.x != v.x || v.y != v.y || v.z != v.z) return false;
    if (fabsf(v.x) > 20000 || fabsf(v.y) > 20000 || fabsf(v.z) > 20000) return false;
    if (fabsf(v.x) < 0.5f && fabsf(v.y) < 0.5f && fabsf(v.z) < 0.5f) return false;
    return true;
}

static void CollectPlayers(std::vector<Player>& out) {
    out.clear();
    if (!g_hProc || !g_base) return;

    uintptr_t arrBases[2] = {};
    int nBase = 0;
    uintptr_t deref = 0;
    if (ReadPtr32(g_base + Off::PLAYER_ARRAY_RVA, deref))
        arrBases[nBase++] = deref;
    arrBases[nBase++] = g_base + Off::PLAYER_ARRAY_RVA;

    // resource table for HP/team override
    int resHp[Off::MAX_SLOTS];
    int resTeam[Off::MAX_SLOTS];
    memset(resHp, -1, sizeof(resHp));
    memset(resTeam, -1, sizeof(resTeam));
    for (int i = 0; i < Off::MAX_SLOTS; i++) {
        int hp = 0, team = 0;
        if (RPM(g_base + Off::PLAYER_RES_RVA + Off::RES_HP + i * 4, hp))
            if (hp >= 0 && hp <= 200) resHp[i] = hp;
        if (RPM(g_base + Off::PLAYER_RES_RVA + Off::RES_TEAM + i * 4, team))
            if (team >= 0 && team <= 5) resTeam[i] = team;
    }

    std::vector<Player> best;
    for (int bi = 0; bi < nBase; bi++) {
        std::vector<Player> trial;
        uintptr_t base = arrBases[bi];
        for (int i = 0; i < Off::MAX_SLOTS; i++) {
            uintptr_t obj = 0;
            if (!ReadPtr32(base + i * Off::ARR_STRIDE + Off::ARR_OBJ_OFF, obj))
                continue;

            Vec3 pos{};
            if (!RPM(obj + Off::OBJ_POS_OFF, pos.x) ||
                !RPM(obj + Off::OBJ_POS_OFF + 4, pos.y) ||
                !RPM(obj + Off::OBJ_POS_OFF + 8, pos.z))
                continue;
            if (!ValidPos(pos)) continue;

            int hp = 0, team = 0;
            RPM(obj + Off::OBJ_HP_OFF, hp);
            RPM(obj + Off::OBJ_TEAM_OFF, team);
            if (resHp[i] >= 1 && resHp[i] <= 150) hp = resHp[i];
            if (resTeam[i] >= 1 && resTeam[i] <= 5) team = resTeam[i];
            if (hp < 2 || hp > 150) continue;
            if (team < 1 || team > 5) continue;

            Player p;
            p.slot = i; p.ptr = obj; p.origin = pos; p.hp = hp; p.team = team;
            trial.push_back(p);
        }
        if (trial.size() > best.size()) best = std::move(trial);
    }
    out = std::move(best);
}

static void DetectSelf(const std::vector<Player>& players) {
    if (!g_haveLocal) return;
    float bestD = 1e9f;
    int bestSlot = -1, bestTeam = 0;
    for (auto& p : players) {
        float dx = p.origin.x - g_local.x;
        float dy = p.origin.y - g_local.y;
        float dz = p.origin.z - g_local.z;
        float d = sqrtf(dx*dx + dy*dy + dz*dz);
        if (d < bestD) { bestD = d; bestSlot = p.slot; bestTeam = p.team; }
    }
    if (bestSlot >= 0 && bestD < 40.f) {
        g_mySlot = bestSlot;
        g_myTeam = bestTeam;
    }
}

static inline ImU32 C4(ImVec4 v) { return ImGui::ColorConvertFloat4ToU32(v); }

static void DrawCorners(ImDrawList* dl, float x, float y, float w, float h, ImU32 c, float t) {
    float lx = w * 0.22f; if (lx < 3) lx = 3;
    float ly = h * 0.20f; if (ly < 3) ly = 3;
    dl->AddLine(ImVec2(x,y), ImVec2(x+lx,y), c, t);
    dl->AddLine(ImVec2(x,y), ImVec2(x,y+ly), c, t);
    dl->AddLine(ImVec2(x+w,y), ImVec2(x+w-lx,y), c, t);
    dl->AddLine(ImVec2(x+w,y), ImVec2(x+w,y+ly), c, t);
    dl->AddLine(ImVec2(x,y+h), ImVec2(x+lx,y+h), c, t);
    dl->AddLine(ImVec2(x,y+h), ImVec2(x,y+h-ly), c, t);
    dl->AddLine(ImVec2(x+w,y+h), ImVec2(x+w-lx,y+h), c, t);
    dl->AddLine(ImVec2(x+w,y+h), ImVec2(x+w,y+h-ly), c, t);
}

static void DrawHP(ImDrawList* dl, float x1, float y1, float x2, float y2, int hp) {
    float f = (float)hp / 100.f; if (f < 0) f = 0; if (f > 1) f = 1;
    ImU32 hc = hp > 75 ? IM_COL32(40,220,40,255) : hp > 40 ? IM_COL32(230,200,40,255) : IM_COL32(230,50,50,255);
    dl->AddRectFilled(ImVec2(x1 - 6, y1), ImVec2(x1 - 3, y2), IM_COL32(30,30,30,255));
    float h = y2 - y1;
    dl->AddRectFilled(ImVec2(x1 - 6, y2 - h * f), ImVec2(x1 - 3, y2), hc);
}

static void MoveRel(int dx, int dy) {
    if (!dx && !dy) return;
    INPUT in{}; in.type = INPUT_MOUSE; in.mi.dx = dx; in.mi.dy = dy; in.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &in, sizeof(INPUT));
}
static void ClickOnce() {
    INPUT in[2]{};
    in[0].type = INPUT_MOUSE; in[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    in[1].type = INPUT_MOUSE; in[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(2, in, sizeof(INPUT));
}
static void PullTo(float tx, float ty, float sw, float sh, float smooth) {
    float dx = tx - sw * 0.5f, dy = ty - sh * 0.5f;
    float d = sqrtf(dx*dx + dy*dy); if (d < 1.2f) return;
    float s = smooth < 1.f ? 1.f : smooth;
    float sx = dx / s, sy = dy / s;
    if (sx > 45) sx = 45; if (sx < -45) sx = -45;
    if (sy > 45) sy = 45; if (sy < -45) sy = -45;
    int mx = (int)(sx >= 0 ? sx + 0.5f : sx - 0.5f);
    int my = (int)(sy >= 0 ? sy + 0.5f : sy - 0.5f);
    if (!mx && fabsf(dx) > 2) mx = dx > 0 ? 1 : -1;
    if (!my && fabsf(dy) > 2) my = dy > 0 ? 1 : -1;
    MoveRel(mx, my);
}
static void SnapTo(float tx, float ty, float sw, float sh) {
    float dx = tx - sw * 0.5f, dy = ty - sh * 0.5f;
    for (int i = 0; i < 3; i++) {
        int mx = (int)(dx * (0.55f + i * 0.15f));
        int my = (int)(dy * (0.55f + i * 0.15f));
        if (mx > 80) mx = 80; if (mx < -80) mx = -80;
        if (my > 80) my = 80; if (my < -80) my = -80;
        MoveRel(mx, my);
    }
}

static int g_rot = -1;
static void RStart(ImDrawList* dl) { g_rot = dl->VtxBuffer.Size; }
static void REnd(ImDrawList* dl, float rad, ImVec2 c) {
    float s = sinf(rad), co = cosf(rad);
    for (int i = g_rot; i < dl->VtxBuffer.Size; i++) {
        auto& v = dl->VtxBuffer.Data[i];
        float px = v.pos.x - c.x, py = v.pos.y - c.y;
        v.pos.x = px * co - py * s + c.x;
        v.pos.y = px * s + py * co + c.y;
    }
}
static void Logo() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList(); if (!dl) return;
    float px = 28, py = 28, r = 22, a = (float)ImGui::GetTime() * 2.1f;
    dl->AddCircleFilled(ImVec2(px, py), r + 4, IM_COL32(0,0,0,120), 32);
    dl->AddCircle(ImVec2(px, py), r + 2, IM_COL32(60,180,255,180), 32, 1.5f);
    for (int i = 0; i < 48; i++) {
        float a0 = a + i / 48.f * 2.f * (float)M_PI;
        float a1 = a + (i + 1) / 48.f * 2.f * (float)M_PI;
        float al = 80 + 120 * (0.5f + 0.5f * sinf(a0 * 2));
        dl->AddLine(ImVec2(px + cosf(a0)*r, py + sinf(a0)*r),
                    ImVec2(px + cosf(a1)*r, py + sinf(a1)*r),
                    IM_COL32(80,200,255,(int)al), 2.f);
    }
    const char* t = "F ESP"; ImFont* f = ImGui::GetFont();
    RStart(dl);
    ImVec2 ts = f->CalcTextSizeA(14.f, FLT_MAX, 0, t);
    ImVec2 tp(px - ts.x * 0.5f, py - ts.y * 0.5f);
    dl->AddText(f, 14.f, ImVec2(tp.x+1,tp.y+1), IM_COL32(0,0,0,200), t);
    dl->AddText(f, 14.f, tp, IM_COL32(100,220,255,255), t);
    REnd(dl, a, ImVec2(px, py));
}

void RenderESPMenu() {
    if (!UI::IsWindowTargeted()) return;
    ImGui::SetNextWindowSize(ImVec2(360, 460), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320, 280), ImVec2(520, 720));
    if (!ImGui::Begin(L_Title(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        ImGui::End(); return;
    }
    ImGui::TextColored(ImVec4(0.4f,0.85f,1.f,1.f), "%s", L_Title());
    ImGui::SameLine(); ImGui::TextDisabled("v3.3");
    ImGui::Separator();

    ImGui::Text("%s", L_Lang()); ImGui::SameLine();
    if (ImGui::RadioButton("EN", g_lang == Lang::EN)) g_lang = Lang::EN;
    ImGui::SameLine();
    if (ImGui::RadioButton("FA", g_lang == Lang::FA)) g_lang = Lang::FA;

    ImGui::Checkbox(L_Enable(), &cfg.enabled);
    ImGui::TextColored(ImVec4(0.5f,1.f,0.5f,1.f), "%s", g_status);
    ImGui::Separator();

    if (ImGui::BeginTabBar("t", ImGuiTabBarFlags_FittingPolicyScroll)) {
        if (ImGui::BeginTabItem(L_TabMain())) {
            ImGui::Checkbox(L_Team(), &cfg.teamCheck);
            ImGui::SliderFloat(L_MaxD(), &cfg.maxDist, 500.f, 8000.f, "%.0f");
            ImGui::Separator();
            ImGui::Checkbox(L_Smooth(), &cfg.smoothing);
            ImGui::SliderFloat("Smooth factor", &cfg.smoothFactor, 0.30f, 0.90f, "%.2f");
            ImGui::SliderInt(L_Hold(), &cfg.holdFrames, 0, 20);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabVis())) {
            ImGui::Checkbox(L_Box(), &cfg.drawBox); ImGui::SameLine();
            ImGui::Checkbox(L_Corners(), &cfg.drawCorners);
            ImGui::Checkbox(L_HP(), &cfg.drawHealth); ImGui::SameLine();
            ImGui::Checkbox(L_Dist(), &cfg.drawDistance);
            ImGui::SliderFloat(L_Thick(), &cfg.thickness, 1.f, 5.f, "%.0f");
            ImGui::SliderFloat(L_BoxH(), &cfg.headZ, 30.f, 120.f, "%.0f");
            ImGui::SliderFloat(L_Width(), &cfg.widthRatio, 0.2f, 0.8f, "%.2f");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabAim())) {
            ImGui::Checkbox(L_AimOn(), &cfg.aimAssist);
            ImGui::SliderFloat(L_AimSm(), &cfg.aimSmooth, 1.f, 12.f, "%.1f");
            ImGui::SliderFloat(L_Fov(), &cfg.aimFov, 40.f, 500.f, "%.0f");
            ImGui::Checkbox(L_AutoAim(), &cfg.autoAim);
            ImGui::Checkbox(L_AutoFire(), &cfg.autoFire);
            ImGui::TextDisabled("LMB=snap | Alt=auto");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabCol())) {
            ImGui::ColorEdit4(L_Enemy(), (float*)&cfg.enemy);
            ImGui::ColorEdit4(L_Mate(), (float*)&cfg.mate);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabSt())) {
            ImGui::Text("Connected : %s", g_hProc ? "Yes" : "No");
            ImGui::Text("Local     : %s", g_haveLocal ? "OK" : "Fail");
            ImGui::Text("Matrix    : %s (%s)", g_matOk ? "OK" : "Fail", g_matSrc);
            ImGui::Text("Players   : %d", g_count);
            ImGui::Text("Drawn     : %d", g_drawn);
            ImGui::Text("MySlot/Team: %d / %d", g_mySlot, g_myTeam);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabCt())) {
            ImGui::Text("%s", L_TG()); ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f,0.7f,1.f,1.f), "@client_v34");
            if (ImGui::Button(L_OpenTG(), ImVec2(-1,0)))
                ShellExecuteA(nullptr, "open", "https://t.me/client_v34", nullptr, nullptr, SW_SHOWNORMAL);
            if (ImGui::Button(L_CopyTG(), ImVec2(-1,0))) {
                if (OpenClipboard(nullptr)) {
                    EmptyClipboard();
                    const char* t = "@client_v34";
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, strlen(t)+1);
                    if (h) { memcpy(GlobalLock(h), t, strlen(t)+1); GlobalUnlock(h); SetClipboardData(CF_TEXT, h); }
                    CloseClipboard();
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void RenderESPBoxes() {
    if (UI::IsWindowTargeted()) Logo();

    if (UI::GetTargetWindow() != g_lastHwnd) {
        g_lastHwnd = UI::GetTargetWindow();
        if (g_lastHwnd) Attach(); else Detach();
    }
    if ((!g_hProc || !g_base) && UI::IsWindowTargeted()) Attach();
    if (!g_hProc || !g_base) return;

    UpdateLocal();
    if (!cfg.enabled) return;

    ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x, sh = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    float mat[16]{};
    g_matOk = GetMatrixLocked(mat);
    if (!g_matOk) {
        dl->AddText(ImVec2(20, 70), IM_COL32(255,80,80,255), "Matrix fail");
        return;
    }

    std::vector<Player> players;
    CollectPlayers(players);
    DetectSelf(players);
    g_count = (int)players.size();

    const float sf = cfg.smoothFactor;
    std::unordered_map<int, bool> seen;
    g_drawn = 0;

    float bestD = 1e9f, bestX = 0, bestY = 0;
    bool hasT = false;

    for (auto& p : players) {
        if (g_mySlot >= 0 && p.slot == g_mySlot) continue;

        float dist = -1.f;
        if (g_haveLocal) {
            float dx = p.origin.x - g_local.x;
            float dy = p.origin.y - g_local.y;
            float dz = p.origin.z - g_local.z;
            dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist < 3.f) continue;
            if (dist > cfg.maxDist) continue;
        }

        if (cfg.teamCheck && g_myTeam > 0 && p.team == g_myTeam) continue;

        Vec3 head = p.origin; head.z += cfg.headZ;
        Vec2 feet, hs;
        if (!W2S(p.origin, mat, sw, sh, feet)) continue;
        if (!W2S(head, mat, sw, sh, hs)) continue;

        // screen-space smooth (V8)
        float fx = feet.x, fy = feet.y, hx = hs.x, hy = hs.y;
        if (cfg.smoothing) {
            auto itF = g_smoothFeet.find(p.slot);
            auto itH = g_smoothHead.find(p.slot);
            if (itF != g_smoothFeet.end()) {
                fx = itF->second.x * sf + fx * (1.f - sf);
                fy = itF->second.y * sf + fy * (1.f - sf);
            }
            if (itH != g_smoothHead.end()) {
                hx = itH->second.x * sf + hx * (1.f - sf);
                hy = itH->second.y * sf + hy * (1.f - sf);
            }
            g_smoothFeet[p.slot] = { fx, fy };
            g_smoothHead[p.slot] = { hx, hy };
        }

        if (fx < 0 || fx > sw || fy < 0 || fy > sh) continue;

        float bh = fabsf(fy - hy);
        float bw = bh * cfg.widthRatio; if (bw < 10.f) bw = 10.f;
        float x1 = fx - bw * 0.5f, y1 = hy, x2 = fx + bw * 0.5f, y2 = fy;

        // update hold cache
        HeldBox hb{ fx, fy, hx, hy, cfg.holdFrames, p.hp, p.team, dist };
        g_hold[p.slot] = hb;
        seen[p.slot] = true;

        ImU32 col = C4(cfg.enemy);
        if (cfg.drawBox)
            dl->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), col, 0, 0, cfg.thickness);
        if (cfg.drawCorners)
            DrawCorners(dl, x1, y1, x2 - x1, y2 - y1, col, cfg.thickness);
        if (cfg.drawHealth)
            DrawHP(dl, x1, y1, x2, y2, p.hp);
        if (cfg.drawDistance && dist >= 0.f) {
            char b[24]; sprintf_s(b, "%du", (int)dist);
            dl->AddText(ImVec2(x1, y2 + 2), col, b);
        }
        ++g_drawn;

        float ax = hx, ay = hy + bh * 0.22f;
        float adx = ax - sw * 0.5f, ady = ay - sh * 0.5f;
        float ad = sqrtf(adx*adx + ady*ady);
        if (ad < bestD && ad <= cfg.aimFov) {
            bestD = ad; bestX = ax; bestY = ay; hasT = true;
        }
    }

    // hold last boxes (anti-flicker when one frame miss)
    for (auto it = g_hold.begin(); it != g_hold.end(); ) {
        if (seen.count(it->first)) { ++it; continue; }
        HeldBox& hb = it->second;
        if (hb.left <= 0) { it = g_hold.erase(it); continue; }
        hb.left--;
        float bh = fabsf(hb.fy - hb.hy);
        float bw = bh * cfg.widthRatio; if (bw < 10.f) bw = 10.f;
        float x1 = hb.fx - bw * 0.5f, y1 = hb.hy, x2 = hb.fx + bw * 0.5f, y2 = hb.fy;
        if (cfg.drawBox)
            dl->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), IM_COL32(150,150,150,160), 0, 0, 1.f);
        ++it;
    }

    // Aim visuals
    const float cx = sw * 0.5f, cy = sh * 0.5f;
    bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    bool lmb = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

    if (cfg.aimAssist || cfg.autoAim)
        dl->AddCircle(ImVec2(cx, cy), cfg.aimFov, IM_COL32(0,255,100,55), 64, 1.2f);

    if (hasT && (cfg.aimAssist || cfg.autoAim || cfg.autoFire)) {
        dl->AddLine(ImVec2(bestX-9, bestY), ImVec2(bestX+9, bestY), IM_COL32(0,255,0,230), 1.6f);
        dl->AddLine(ImVec2(bestX, bestY-9), ImVec2(bestX, bestY+9), IM_COL32(0,255,0,230), 1.6f);
        float dx = bestX - cx, dy = bestY - cy;
        float len = sqrtf(dx*dx + dy*dy);
        if (len > 1.f) {
            float ux = dx / len, uy = dy / len;
            if (len > cfg.aimFov)
                dl->AddLine(ImVec2(cx + ux*cfg.aimFov, cy + uy*cfg.aimFov), ImVec2(bestX, bestY), IM_COL32(0,255,0,180), 1.4f);
            else
                dl->AddLine(ImVec2(cx, cy), ImVec2(bestX, bestY), IM_COL32(0,255,0,160), 1.2f);
        }
        if (cfg.aimAssist && lmb && !alt) SnapTo(bestX, bestY, sw, sh);
        if (alt) {
            if (cfg.autoAim) PullTo(bestX, bestY, sw, sh, cfg.aimSmooth);
            if (cfg.autoFire && bestD < 14.f) ClickOnce();
        }
    }
}

void RenderESP() {
    RenderESPMenu();
    RenderESPBoxes();
}
