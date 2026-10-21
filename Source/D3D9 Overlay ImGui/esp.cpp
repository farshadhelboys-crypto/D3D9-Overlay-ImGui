// =====================================================================
//  ESP - Integrated with UI / Drawing framework
// =====================================================================
#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <imgui.h>
#include "UI.h"

// ---------------- OFFSETS ----------------
namespace Off {
    constexpr uintptr_t ENTITY_ARRAY_PTR = 0x25B9390;
    constexpr uintptr_t ENTITY_COUNT     = 0x25B939C;
    constexpr uintptr_t ENTITY_STRIDE    = 0x274;
    constexpr uintptr_t VIEW_MATRIX      = 0x257B2C8;
    constexpr uintptr_t VIEW_MATRIX_ALT  = 0xFC7B00;
    constexpr uintptr_t OBJ_HP           = 0x11C;
    constexpr uintptr_t OBJ_TEAM         = 0x1FC;
    constexpr uintptr_t OBJ_ORIGIN       = 0x538;
    constexpr uintptr_t LOCAL_PLAYER_ENC = 0x72950F;
    constexpr uint32_t  LOCAL_PLAYER_XOR = 0x1599C945;
}

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };
struct Mat4 { float m[4][4]; };

// ---------------- CONFIG ----------------
struct EspConfig {
    bool  enabled       = true;
    bool  teamCheck     = false;
    bool  drawBox       = true;
    bool  drawCorners   = false;
    bool  drawHealth    = true;
    bool  drawDistance  = true;
    bool  drawLocal     = false;
    bool  pointerArray  = true;
    int   matrixChoice  = 0;
    float headOffset    = 72.0f;
    float widthRatio    = 0.45f;
    float thickness     = 1.6f;
    float maxDistance   = 800.0f;
    float healthMax     = 100.0f;
    int   healthSide    = 0;
    ImVec4 enemyColor   = ImVec4(1.00f, 0.25f, 0.25f, 1.00f);
    ImVec4 mateColor    = ImVec4(0.25f, 0.85f, 1.00f, 1.00f);
    ImVec4 localColor   = ImVec4(1.00f, 1.00f, 0.30f, 1.00f);
} cfg;

// ---------------- STATE ----------------
static HANDLE     g_hProc       = nullptr;
static uintptr_t  g_gameBase    = 0;
static DWORD      g_attachedPid = 0;
static HWND       g_lastTarget  = nullptr;
static int        g_localTeam   = 0;
static uintptr_t  g_localPtr    = 0;
static Vec3       g_localPos    = {0, 0, 0};
static bool       g_haveLocal   = false;
static char       g_status[160] = "Not attached";
static int        g_dbgCount    = 0;
static int        g_dbgDrawn    = 0;
static bool       g_dbgVmOk     = false;

// ---------------- MEMORY ----------------
template<typename T>
static inline bool RPM(uintptr_t addr, T& out) {
    return ReadProcessMemory(g_hProc, (LPCVOID)addr, &out, sizeof(T), nullptr);
}

static uintptr_t GetModuleBase(DWORD pid, const wchar_t* mod) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    MODULEENTRY32W me{ sizeof(me) };
    uintptr_t base = 0;
    while (Module32NextW(s, &me)) {
        if (_wcsicmp(me.szModule, mod) == 0) { base = (uintptr_t)me.modBaseAddr; break; }
    }
    CloseHandle(s);
    return base;
}

static void Detach() {
    if (g_hProc) { CloseHandle(g_hProc); g_hProc = nullptr; }
    g_gameBase    = 0;
    g_attachedPid = 0;
    g_localTeam   = 0;
    g_localPtr    = 0;
    g_haveLocal   = false;
    g_dbgCount    = 0;
    g_dbgDrawn    = 0;
    g_dbgVmOk     = false;
    strcpy_s(g_status, "Not attached");
}

static bool AttachFromUI() {
    Detach();
    if (!UI::IsWindowTargeted()) {
        strcpy_s(g_status, "No target window");
        return false;
    }
    HWND  hwnd = UI::GetTargetWindow();
    DWORD pid  = UI::GetTargetPID();
    if (!pid && hwnd) GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) { strcpy_s(g_status, "Target PID invalid"); return false; }

    g_hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!g_hProc) {
        strcpy_s(g_status, "OpenProcess failed (run as admin)");
        return false;
    }
    g_gameBase = GetModuleBase(pid, L"clientmod_game.dll");
    if (!g_gameBase) {
        strcpy_s(g_status, "clientmod_game.dll not loaded yet");
        CloseHandle(g_hProc); g_hProc = nullptr;
        return false;
    }
    g_attachedPid = pid;
    sprintf_s(g_status, "Attached: PID=%lu base=0x%llX",
              (unsigned long)pid, (unsigned long long)g_gameBase);
    return true;
}

// ---------------- LOCAL PLAYER ----------------
static void UpdateLocal() {
    g_localTeam = 0;
    g_localPtr  = 0;
    g_haveLocal = false;
    if (!g_hProc || !g_gameBase) return;

    uint32_t enc = 0;
    if (!RPM(g_gameBase + Off::LOCAL_PLAYER_ENC, enc)) return;
    uintptr_t lp = (uintptr_t)(enc ^ Off::LOCAL_PLAYER_XOR);
    if (lp < 0x10000) return;
    g_localPtr = lp;

    int team = 0;
    if (RPM(lp + Off::OBJ_TEAM, team)) g_localTeam = team;

    Vec3 o{};
    if (RPM(lp + Off::OBJ_ORIGIN, o)) { g_localPos = o; g_haveLocal = true; }
}

// ---------------- W2S ----------------
static bool WorldToScreen(const Vec3& w, Vec2& s, const Mat4& vm, float sw, float sh) {
    float clip = vm.m[3][0]*w.x + vm.m[3][1]*w.y + vm.m[3][2]*w.z + vm.m[3][3];
    if (clip < 0.001f) return false;
    float x = vm.m[0][0]*w.x + vm.m[0][1]*w.y + vm.m[0][2]*w.z + vm.m[0][3];
    float y = vm.m[1][0]*w.x + vm.m[1][1]*w.y + vm.m[1][2]*w.z + vm.m[1][3];
    float iw = 1.0f / clip;
    s.x = (sw * 0.5f) + (x * iw) * (sw * 0.5f);
    s.y = (sh * 0.5f) - (y * iw) * (sh * 0.5f);
    return true;
}

// ---------------- DRAW HELPERS ----------------
static inline ImU32 C(ImVec4 v) { return ImGui::ColorConvertFloat4ToU32(v); }

static void DrawCorners(ImDrawList* dl, float x, float y, float w, float h,
                        ImU32 c, float t) {
    float l = (w < h ? w : h) * 0.28f;
    if (l < 4.0f) l = 4.0f;
    dl->AddLine(ImVec2(x, y), ImVec2(x + l, y), c, t);
    dl->AddLine(ImVec2(x, y), ImVec2(x, y + l), c, t);
    dl->AddLine(ImVec2(x + w, y), ImVec2(x + w - l, y), c, t);
    dl->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + l), c, t);
    dl->AddLine(ImVec2(x, y + h), ImVec2(x + l, y + h), c, t);
    dl->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - l), c, t);
    dl->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - l, y + h), c, t);
    dl->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - l), c, t);
}

static void DrawHealthBar(ImDrawList* dl, float bx, float by, float w, float h,
                          float frac, int side) {
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    ImU32 hc = frac > 0.6f ? IM_COL32(60, 220, 60, 255)
             : frac > 0.3f ? IM_COL32(240, 220, 40, 255)
                           : IM_COL32(240, 60, 60, 255);
    ImU32 bg = IM_COL32(0, 0, 0, 180);

    if (side == 0 || side == 1) {
        float bw = 3.5f, gap = 4.0f;
        float bxx = (side == 0) ? (bx - bw - gap) : (bx + w + gap);
        dl->AddRectFilled(ImVec2(bxx, by), ImVec2(bxx + bw, by + h), bg);
        float fill = h * frac;
        dl->AddRectFilled(ImVec2(bxx, by + h - fill),
                          ImVec2(bxx + bw, by + h), hc);
    } else {
        float bh = 3.5f, gap = 4.0f;
        float byy = (side == 2) ? (by - bh - gap) : (by + h + gap);
        dl->AddRectFilled(ImVec2(bx, byy), ImVec2(bx + w, byy + bh), bg);
        float fill = w * frac;
        dl->AddRectFilled(ImVec2(bx, byy), ImVec2(bx + fill, byy + bh), hc);
    }
}

// =====================================================================
//  RENDER MENU (only when INSERT pressed)
// =====================================================================
void RenderESPMenu() {
    if (!UI::IsWindowTargeted()) return;

    ImGui::SetNextWindowSize(ImVec2(370, 720), ImGuiCond_FirstUseEver);
    ImGui::Begin("ESP Settings", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Checkbox("Enable ESP", &cfg.enabled);
    ImGui::SameLine();
    if (ImGui::Button("Re-attach")) AttachFromUI();
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", g_status);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Filters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Team Check", &cfg.teamCheck);
        ImGui::SameLine();
        ImGui::TextDisabled("(LocalTeam=%d)", g_localTeam);
        ImGui::SliderFloat("Max Distance", &cfg.maxDistance, 0.0f, 3000.0f, "%.0f");
    }

    if (ImGui::CollapsingHeader("Visuals", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Box", &cfg.drawBox);
        ImGui::SameLine();
        ImGui::Checkbox("Corners", &cfg.drawCorners);
        ImGui::Checkbox("Health Bar", &cfg.drawHealth);
        ImGui::SameLine();
        ImGui::Checkbox("Distance", &cfg.drawDistance);
        ImGui::Checkbox("Draw Local Player", &cfg.drawLocal);
    }

    if (ImGui::CollapsingHeader("Box Tuning", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Thickness",   &cfg.thickness,   0.5f, 5.0f, "%.1f");
        ImGui::SliderFloat("Head Offset", &cfg.headOffset, 50.0f, 110.0f, "%.0f");
        ImGui::SliderFloat("Width Ratio", &cfg.widthRatio,  0.2f, 0.8f, "%.2f");
    }

    if (ImGui::CollapsingHeader("Health Bar")) {
        ImGui::SliderFloat("Max HP", &cfg.healthMax, 50.0f, 500.0f, "%.0f");
        ImGui::Combo("Side", &cfg.healthSide, "Left\0Right\0Top\0Bottom\0");
    }

    if (ImGui::CollapsingHeader("Matrix / Entity", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Combo("Matrix Source", &cfg.matrixChoice,
                     "Main (0x257B2C8)\0Alt (0xFC7B00)\0");
        ImGui::Checkbox("Entity List = Pointer Array", &cfg.pointerArray);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Default ON. If boxes do NOT appear, untick this.\n"
                "Controls whether 0x25B9390 is a pointer to the array\n"
                "or the array itself.");
    }

    if (ImGui::CollapsingHeader("Colors")) {
        ImGui::ColorEdit4("Enemy", (float*)&cfg.enemyColor);
        ImGui::ColorEdit4("Mate",  (float*)&cfg.mateColor);
        ImGui::ColorEdit4("Local", (float*)&cfg.localColor);
    }

    if (ImGui::CollapsingHeader("Debug")) {
        ImGui::Text("Base     : 0x%llX", (unsigned long long)g_gameBase);
        ImGui::Text("PID      : %lu",    (unsigned long)g_attachedPid);
        ImGui::Text("Entity   : %d",     g_dbgCount);
        ImGui::Text("Drawn    : %d",     g_dbgDrawn);
        ImGui::Text("Matrix OK: %s",     g_dbgVmOk ? "yes" : "no");
        ImGui::Text("LocalPtr : 0x%llX", (unsigned long long)g_localPtr);
        ImGui::Text("LocalTeam: %d",     g_localTeam);
    }

    ImGui::End();
}

// =====================================================================
//  RENDER BOXES (always, when target set & ESP enabled)
// =====================================================================
void RenderESPBoxes() {
    // Detect target window change
    if (UI::GetTargetWindow() != g_lastTarget) {
        g_lastTarget = UI::GetTargetWindow();
        if (g_lastTarget) AttachFromUI();
        else              Detach();
    }
    if ((!g_hProc || !g_gameBase) && UI::IsWindowTargeted()) {
        AttachFromUI();
    }
    if (!g_hProc || !g_gameBase) return;

    UpdateLocal();

    if (!cfg.enabled) return;

    Mat4 vm{};
    uintptr_t vmAddr = g_gameBase +
        (cfg.matrixChoice == 0 ? Off::VIEW_MATRIX : Off::VIEW_MATRIX_ALT);
    g_dbgVmOk = RPM(vmAddr, vm);
    if (!g_dbgVmOk) return;

    int count = 0;
    if (!RPM(g_gameBase + Off::ENTITY_COUNT, count)) return;
    g_dbgCount = count;
    g_dbgDrawn = 0;
    if (count <= 0 || count > 512) return;

    uintptr_t listBase = 0;
    if (cfg.pointerArray) {
        if (!RPM(g_gameBase + Off::ENTITY_ARRAY_PTR, listBase)) return;
        if (listBase < 0x10000) return;
    } else {
        listBase = g_gameBase + Off::ENTITY_ARRAY_PTR;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x;
    const float sh = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    for (int i = 0; i < count; ++i) {
        uintptr_t ent = listBase + (uintptr_t)i * Off::ENTITY_STRIDE;

        int  hp = 0;
        int  team = 0;
        Vec3 origin{};
        if (!RPM(ent + Off::OBJ_HP, hp))         continue;
        if (!RPM(ent + Off::OBJ_TEAM, team))     continue;
        if (!RPM(ent + Off::OBJ_ORIGIN, origin)) continue;

        if (hp <= 0 || hp > 1000) continue;
        if (origin.x == 0.0f && origin.y == 0.0f && origin.z == 0.0f) continue;

        const bool isLocal = (g_localPtr != 0 && ent == g_localPtr);
        if (isLocal && !cfg.drawLocal) continue;
        if (!isLocal && cfg.teamCheck && g_localTeam != 0 && team == g_localTeam)
            continue;

        Vec3 head = origin;
        head.z += cfg.headOffset;

        Vec2 sf{}, shd{};
        if (!WorldToScreen(origin, sf, vm, sw, sh)) continue;
        if (!WorldToScreen(head,   shd, vm, sw, sh)) continue;

        float h = sf.y - shd.y;
        if (h < 3.0f || h > 3000.0f) continue;
        float w = h * cfg.widthRatio;

        float bx = shd.x - w * 0.5f;
        float by = shd.y;

        if (bx + w < 0 || bx > sw || by + h < 0 || by > sh) continue;

        float dist;
        if (g_haveLocal) {
            float dx = origin.x - g_localPos.x;
            float dy = origin.y - g_localPos.y;
            float dz = origin.z - g_localPos.z;
            dist = sqrtf(dx*dx + dy*dy + dz*dz);
        } else {
            dist = sqrtf(origin.x*origin.x + origin.y*origin.y + origin.z*origin.z);
        }
        if (cfg.maxDistance > 0.0f && dist > cfg.maxDistance) continue;

        ImU32 col;
        if (isLocal)                                       col = C(cfg.localColor);
        else if (g_localTeam != 0 && team == g_localTeam)  col = C(cfg.mateColor);
        else                                               col = C(cfg.enemyColor);

        if (cfg.drawBox)
            dl->AddRect(ImVec2(bx, by), ImVec2(bx + w, by + h),
                        col, 0.0f, 0, cfg.thickness);
        if (cfg.drawCorners)
            DrawCorners(dl, bx, by, w, h, col, cfg.thickness + 0.5f);
        if (cfg.drawHealth)
            DrawHealthBar(dl, bx, by, w, h, (float)hp / cfg.healthMax,
                          cfg.healthSide);
        if (cfg.drawDistance) {
            char buf[48];
            sprintf_s(buf, "#%d  %.0f", i, dist);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(bx + w * 0.5f - ts.x * 0.5f, by + h + 2.0f),
                        IM_COL32(255, 255, 255, 230), buf);
        }
        ++g_dbgDrawn;
    }
}

// Wrapper (optional, in case you want to call both)
void RenderESP() {
    RenderESPMenu();
    RenderESPBoxes();
}
