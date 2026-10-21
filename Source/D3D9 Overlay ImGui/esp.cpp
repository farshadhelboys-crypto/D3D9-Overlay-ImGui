// =====================================================================
//  ESP - Ready to build for D3D9-Overlay-ImGui
//  External ESP using ReadProcessMemory (no injection)
// =====================================================================
#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <imgui.h>

// ---------------- OFFSETS (from dump) ----------------
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
    bool  pointerArray  = true;    // if boxes don't show, untick this
    int   matrixChoice  = 0;       // 0=Main, 1=Alt
    float headOffset    = 72.0f;
    float widthRatio    = 0.45f;
    float thickness     = 1.6f;
    float maxDistance   = 800.0f;
    ImVec4 enemyColor   = ImVec4(1.00f, 0.25f, 0.25f, 1.00f);
    ImVec4 mateColor    = ImVec4(0.25f, 0.85f, 1.00f, 1.00f);
    ImVec4 localColor   = ImVec4(1.00f, 1.00f, 0.30f, 1.00f);
} cfg;

// ---------------- STATE ----------------
static HANDLE     g_hProc      = nullptr;
static uintptr_t  g_gameBase   = 0;
static int        g_localTeam  = 0;
static uintptr_t  g_localPtr   = 0;
static Vec3       g_localPos   = {0, 0, 0};
static bool       g_haveLocal  = false;
static char       g_status[128] = "Not attached";

// ---------------- MEMORY ----------------
template<typename T>
static inline bool RPM(uintptr_t addr, T& out) {
    return ReadProcessMemory(g_hProc, (LPCVOID)addr, &out, sizeof(T), nullptr);
}

static DWORD GetPidByName(const wchar_t* name) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{ sizeof(pe) };
    DWORD pid = 0;
    while (Process32NextW(s, &pe)) {
        if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
    }
    CloseHandle(s);
    return pid;
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
    g_gameBase = 0;
    g_localTeam = 0;
    g_localPtr = 0;
    g_haveLocal = false;
    strcpy_s(g_status, "Not attached");
}

static bool Attach() {
    Detach();
    DWORD pid = GetPidByName(L"CMLauncher.exe");
    if (!pid) { strcpy_s(g_status, "Game not running"); return false; }
    g_hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!g_hProc) { strcpy_s(g_status, "OpenProcess failed (run as admin)"); return false; }
    g_gameBase = GetModuleBase(pid, L"clientmod_game.dll");
    if (!g_gameBase) { strcpy_s(g_status, "clientmod_game.dll not found"); return false; }
    sprintf_s(g_status, "Attached: base=0x%llX", (unsigned long long)g_gameBase);
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

// ---------------- MENU ----------------
static void RenderMenu() {
    ImGui::SetNextWindowSize(ImVec2(340, 600), ImGuiCond_FirstUseEver);
    ImGui::Begin("ESP Settings", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Checkbox("Enable ESP", &cfg.enabled);
    ImGui::SameLine();
    if (ImGui::Button("Re-attach")) Attach();
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", g_status);
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Filters", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Team Check", &cfg.teamCheck);
        ImGui::SameLine();
        ImGui::TextDisabled("(LocalTeam=%d)", g_localTeam);
        ImGui::SliderFloat("Max Distance", &cfg.maxDistance, 0.0f, 2000.0f, "%.0f m");
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

    if (ImGui::CollapsingHeader("Tuning")) {
        ImGui::SliderFloat("Thickness", &cfg.thickness, 0.5f, 5.0f, "%.1f");
        ImGui::SliderFloat("Head Offset", &cfg.headOffset, 50.0f, 110.0f, "%.0f");
        ImGui::SliderFloat("Width Ratio", &cfg.widthRatio, 0.2f, 0.8f, "%.2f");
        ImGui::Combo("Matrix Source", &cfg.matrixChoice,
                     "Main (0x257B2C8)\0Alt (0xFC7B00)\0");
        ImGui::Checkbox("Entity List = Pointer Array", &cfg.pointerArray);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(
                "Default ON. If boxes do NOT appear, untick this.\n"
                "It controls whether 0x25B9390 holds a pointer to\n"
                "the entity array or is the entity array itself.");
    }

    if (ImGui::CollapsingHeader("Colors")) {
        ImGui::ColorEdit4("Enemy", (float*)&cfg.enemyColor);
        ImGui::ColorEdit4("Mate",  (float*)&cfg.mateColor);
        ImGui::ColorEdit4("Local", (float*)&cfg.localColor);
    }

    ImGui::End();
}

// ---------------- MAIN RENDER (called every frame) ----------------
void RenderESP() {
    RenderMenu();

    // Auto attach on first frame
    if (!g_hProc || !g_gameBase) {
        if (!Attach()) return;
    }

    UpdateLocal();

    if (!cfg.enabled) return;

    // Read view matrix
    Mat4 vm{};
    uintptr_t vmAddr = g_gameBase +
        (cfg.matrixChoice == 0 ? Off::VIEW_MATRIX : Off::VIEW_MATRIX_ALT);
    if (!RPM(vmAddr, vm)) return;

    // Read entity count
    int count = 0;
    if (!RPM(g_gameBase + Off::ENTITY_COUNT, count)) return;
    if (count <= 0 || count > 512) return;

    // Resolve list base
    uintptr_t listBase = 0;
    if (cfg.pointerArray) {
        if (!RPM(g_gameBase + Off::ENTITY_ARRAY_PTR, listBase)) return;
        if (listBase < 0x10000) return;
    } else {
        listBase = g_gameBase + Off::ENTITY_ARRAY_PTR;
    }

    ImGuiIO& io   = ImGui::GetIO();
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

        // distance from local player (if known), else from world origin
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
        if (isLocal)                          col = C(cfg.localColor);
        else if (g_localTeam != 0 && team == g_localTeam) col = C(cfg.mateColor);
        else                                  col = C(cfg.enemyColor);

        // ---- Box ----
        if (cfg.drawBox) {
            dl->AddRect(ImVec2(bx, by), ImVec2(bx + w, by + h),
                        col, 0.0f, 0, cfg.thickness);
        }
        if (cfg.drawCorners) {
            DrawCorners(dl, bx, by, w, h, col, cfg.thickness + 0.5f);
        }

        // ---- Health bar ----
        if (cfg.drawHealth) {
            float frac = (float)hp / 100.0f;
            if (frac < 0.0f) frac = 0.0f;
            if (frac > 1.0f) frac = 1.0f;
            float bw  = 3.5f;
            float bxx = bx - bw - 4.0f;
            dl->AddRectFilled(ImVec2(bxx, by), ImVec2(bxx + bw, by + h),
                              IM_COL32(0, 0, 0, 180));
            ImU32 hc = frac > 0.6f ? IM_COL32(60, 220, 60, 255)
                     : frac > 0.3f ? IM_COL32(240, 220, 40, 255)
                                   : IM_COL32(240, 60, 60, 255);
            float fill = h * frac;
            dl->AddRectFilled(ImVec2(bxx, by + h - fill),
                              ImVec2(bxx + bw, by + h), hc);
        }

        // ---- Distance label ----
        if (cfg.drawDistance) {
            char buf[48];
            sprintf_s(buf, "#%d  %.0f", i, dist);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(bx + w * 0.5f - ts.x * 0.5f, by + h + 2.0f),
                        IM_COL32(255, 255, 255, 230), buf);
        }
    }
}
