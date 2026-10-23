// =====================================================================
//  ESP - Integrated with UI / Drawing framework (EN / FA bilingual)
//  Updated with verified offsets discovered 2026-10-23
// =====================================================================
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cfloat>
#include "ImGui/imgui.h"
#include "UI.h"
#include "esp.h"

#pragma comment(lib, "shell32.lib")

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

enum class Lang { EN = 0, FA = 1 };
static Lang g_lang = Lang::EN;

struct Loc {
    const char* en;
    const char* fa;
    const char* operator()() const { return g_lang == Lang::FA ? fa : en; }
};

static const Loc L_Title        = { "farshad pm Esp",           "farshad pm Esp" };
static const Loc L_Language     = { "Language",                 "Language" };
static const Loc L_EnableESP    = { "Enable ESP",               "Enable ESP" };
static const Loc L_Reattach     = { "Re-attach",                "Re-attach" };
static const Loc L_Filters      = { "Filters",                  "Filters" };
static const Loc L_TeamCheck    = { "Team Check",               "Team Check" };
static const Loc L_MaxDist      = { "Max Distance",             "Max Distance" };
static const Loc L_Visuals      = { "Visuals",                  "Visuals" };
static const Loc L_Box          = { "Box",                      "Box" };
static const Loc L_Corners      = { "Corners",                  "Corners" };
static const Loc L_HealthBar    = { "Health Bar",               "Health Bar" };
static const Loc L_Distance     = { "Distance",                 "Distance" };
static const Loc L_DrawLocal    = { "Draw Local Player",        "Draw Local Player" };
static const Loc L_BoxTuning    = { "Box Tuning",               "Box Tuning" };
static const Loc L_Thickness    = { "Thickness",                "Thickness" };
static const Loc L_HeadOffset   = { "Head Offset",              "Head Offset" };
static const Loc L_WidthRatio   = { "Width Ratio",              "Width Ratio" };
static const Loc L_Health       = { "Health Bar",               "Health Bar" };
static const Loc L_MaxHP        = { "Max HP",                   "Max HP" };
static const Loc L_Side         = { "Side",                     "Side" };
static const Loc L_MatrixEntity = { "Advanced",                 "Advanced" };
static const Loc L_MatrixSrc    = { "Camera Mode",              "Camera Mode" };
static const Loc L_Colors       = { "Colors",                   "Colors" };
static const Loc L_Enemy        = { "Enemy",                    "Enemy" };
static const Loc L_Mate         = { "Mate",                     "Mate" };
static const Loc L_Local        = { "Local",                    "Local" };
static const Loc L_Debug        = { "Debug",                    "Debug" };
static const Loc L_Contact      = { "Contact",                  "Contact" };
static const Loc L_Telegram     = { "Telegram:",                "Telegram:" };
static const Loc L_OpenTG       = { "Open Telegram",            "Open Telegram" };
static const Loc L_CopyTG       = { "Copy @client_v34",         "Copy @client_v34" };

// ============================================================
// VERIFIED OFFSETS (Discovered by Farshad via Cheat Engine)
// Date: 2026-10-23
// ============================================================
namespace Off {
    // Self Position: [clientmod_game.dll + 0x00E1F664] -> Ptr -> X/Y/Z (float)
    constexpr uintptr_t MY_POS_PTR_RVA   = 0x00E1F664;

    // Self Team: [clientmod_game.dll + 0x02E1616C] -> Ptr + 0x1D8 (Int32)
    // 2 = Terrorist, 3 = CT
    constexpr uintptr_t MY_TEAM_PTR_RVA  = 0x02E1616C;
    constexpr uintptr_t MY_TEAM_OFFSET   = 0x1D8;

    // HP chain: [clientmod_game.dll + 0x029BD00C] -> Ptr1 + 0x38 -> Ptr2 + 0x48
    constexpr uintptr_t HP_PTR_RVA       = 0x029BD00C;
    constexpr uintptr_t HP_OFFSET1       = 0x38;
    constexpr uintptr_t HP_OFFSET2       = 0x48;

    // Entity list (still experimental / unverified)
    constexpr uintptr_t ENTITY_LIST_RVA  = 0x25B9390;
    constexpr uintptr_t ENTITY_BIAS_RVA  = 0x25B8FA0;
    constexpr uintptr_t PLAYER_STRIDE    = 0x400;

    // Old entity fields (kept for experimental entity loop)
    constexpr uintptr_t OBJ_HP           = 0x11C;
    constexpr uintptr_t OBJ_TEAM         = 0x1FC;
    constexpr uintptr_t OBJ_ORIGIN       = 0x538;
}

// Absolute heap matrix candidates (change every game restart)
static const uintptr_t MATRIX_CANDIDATES[] = {
    0x2BE62A70,
    0x2BE62AC0,
    0x25474A30,
    0x317FDDF0,
    0x317FDE00,
    0x257B2C8,   // old module-relative fallback (will be + base later if needed)
    0xFC7B00
};
static const int MATRIX_CANDIDATE_COUNT = sizeof(MATRIX_CANDIDATES) / sizeof(MATRIX_CANDIDATES[0]);

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };
struct Mat4 { float m[16]; };  // row-major flat for easy W2S

struct EspConfig {
    bool  enabled       = true;
    bool  teamCheck     = false;
    bool  drawBox       = true;
    bool  drawCorners   = false;
    bool  drawHealth    = true;
    bool  drawDistance  = true;
    bool  drawLocal     = true;   // default ON so user can verify W2S
    bool  tryEntityLoop = false;  // experimental, off by default
    int   matrixChoice  = 0;      // 0 = Auto-scan
    float headOffset    = 72.0f;
    float widthRatio    = 0.45f;
    float thickness     = 1.6f;
    float maxDistance   = 800.0f;
    float healthMax     = 100.0f;
    int   healthSide    = 0;
    ImVec4 enemyColor   = ImVec4(1.00f, 0.25f, 0.25f, 1.00f);
    ImVec4 mateColor    = ImVec4(0.25f, 0.85f, 1.00f, 1.00f);
    ImVec4 localColor   = ImVec4(0.20f, 1.00f, 0.30f, 1.00f);
} cfg;

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
static int        g_bestMatrixIdx = -1;
static float      g_bestMatrixDist = 999999.f;

template<typename T>
static inline bool RPM(uintptr_t addr, T& out) {
    SIZE_T read = 0;
    return ReadProcessMemory(g_hProc, (LPCVOID)addr, &out, sizeof(T), &read) && read == sizeof(T);
}

// Safe pointer read (game appears to be 32-bit)
static inline bool ReadPtr(uintptr_t addr, uintptr_t& out) {
    uint32_t tmp = 0;
    if (!RPM(addr, tmp)) return false;
    out = (uintptr_t)tmp;
    return true;
}

static uintptr_t GetModuleBase(DWORD pid, const wchar_t* mod) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (s == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me{ sizeof(me) };
    uintptr_t base = 0;
    if (Module32FirstW(s, &me)) {
        do {
            if (_wcsicmp(me.szModule, mod) == 0) { base = (uintptr_t)me.modBaseAddr; break; }
        } while (Module32NextW(s, &me));
    }
    CloseHandle(s);
    return base;
}

static void Detach() {
    if (g_hProc) { CloseHandle(g_hProc); g_hProc = nullptr; }
    g_gameBase = 0; g_attachedPid = 0; g_localTeam = 0; g_localPtr = 0; g_haveLocal = false;
    g_dbgCount = 0; g_dbgDrawn = 0; g_dbgVmOk = false;
    g_bestMatrixIdx = -1; g_bestMatrixDist = 999999.f;
    strcpy_s(g_status, "Not attached");
}

static bool AttachFromUI() {
    Detach();
    if (!UI::IsWindowTargeted()) { strcpy_s(g_status, "No target window"); return false; }
    HWND hwnd = UI::GetTargetWindow();
    DWORD pid = UI::GetTargetPID();
    if (!pid && hwnd) GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) { strcpy_s(g_status, "Target PID invalid"); return false; }
    g_hProc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!g_hProc) { strcpy_s(g_status, "OpenProcess failed (run as admin)"); return false; }
    g_gameBase = GetModuleBase(pid, L"clientmod_game.dll");
    if (!g_gameBase) {
        strcpy_s(g_status, "clientmod_game.dll not loaded yet");
        CloseHandle(g_hProc); g_hProc = nullptr; return false;
    }
    g_attachedPid = pid;
    sprintf_s(g_status, "Attached - PID: %lu  Base: 0x%llX", (unsigned long)pid, (unsigned long long)g_gameBase);
    return true;
}

// -----------------------------------------------------------------
// Update local player using NEW verified chains
// -----------------------------------------------------------------
static void UpdateLocal() {
    g_localTeam = 0;
    g_localPtr  = 0;
    g_haveLocal = false;
    if (!g_hProc || !g_gameBase) return;

    // 1. Self Position
    uintptr_t posPtr = 0;
    if (ReadPtr(g_gameBase + Off::MY_POS_PTR_RVA, posPtr) && posPtr > 0x10000) {
        Vec3 pos{};
        if (RPM(posPtr + 0x0, pos.x) && RPM(posPtr + 0x4, pos.y) && RPM(posPtr + 0x8, pos.z)) {
            g_localPos  = pos;
            g_localPtr  = posPtr;   // we use the pos pointer as identity for now
            g_haveLocal = true;
        }
    }

    // 2. Self Team
    uintptr_t teamBase = 0;
    if (ReadPtr(g_gameBase + Off::MY_TEAM_PTR_RVA, teamBase) && teamBase > 0x10000) {
        int team = 0;
        if (RPM(teamBase + Off::MY_TEAM_OFFSET, team)) {
            g_localTeam = team;
        }
    }
}

// -----------------------------------------------------------------
// WorldToScreen (same math as the working Python)
// -----------------------------------------------------------------
static bool WorldToScreen(const Vec3& pos, const float* mat, float sw, float sh, Vec2& out) {
    float cx = mat[0] * pos.x + mat[4] * pos.y + mat[8]  * pos.z + mat[12];
    float cy = mat[1] * pos.x + mat[5] * pos.y + mat[9]  * pos.z + mat[13];
    float cw = mat[3] * pos.x + mat[7] * pos.y + mat[11] * pos.z + mat[15];

    if (fabsf(cw) < 0.001f) return false;

    out.x = (sw * 0.5f) * (1.0f + cx / cw);
    out.y = (sh * 0.5f) * (1.0f - cy / cw);
    return true;
}

static bool IsValidMatrix(const float* m) {
    // m[15] should be close to 1.0
    if (fabsf(m[15] - 1.0f) > 0.05f) return false;
    // translation components should be reasonably large
    if (fabsf(m[3]) < 5.0f && fabsf(m[7]) < 5.0f && fabsf(m[11]) < 5.0f) return false;
    // rotation values roughly in [-1.5 .. 1.5]
    for (int i : {0,1,2, 4,5,6, 8,9,10}) {
        if (fabsf(m[i]) > 1.6f) return false;
    }
    return true;
}

// Find the best matrix among candidates (same logic as Python)
static bool FindBestMatrix(float* outMat, float sw, float sh) {
    g_bestMatrixIdx = -1;
    g_bestMatrixDist = 999999.f;

    if (!g_haveLocal) return false;

    float bestMat[16] = {};
    bool  found = false;

    for (int i = 0; i < MATRIX_CANDIDATE_COUNT; ++i) {
        uintptr_t addr = MATRIX_CANDIDATES[i];

        // If address looks like a small RVA, try module + RVA
        if (addr < 0x1000000) {
            addr = g_gameBase + addr;
        }

        float mat[16] = {};
        if (!RPM(addr, mat)) continue;
        if (!IsValidMatrix(mat)) continue;

        Vec2 screen{};
        if (!WorldToScreen(g_localPos, mat, sw, sh, screen)) continue;

        float dx = screen.x - sw * 0.5f;
        float dy = screen.y - sh * 0.5f;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist < g_bestMatrixDist) {
            g_bestMatrixDist = dist;
            g_bestMatrixIdx  = i;
            memcpy(bestMat, mat, sizeof(bestMat));
            found = true;
        }
    }

    if (found) {
        memcpy(outMat, bestMat, sizeof(float) * 16);
        g_dbgVmOk = true;
        return true;
    }

    g_dbgVmOk = false;
    return false;
}

static inline ImU32 C(ImVec4 v) { return ImGui::ColorConvertFloat4ToU32(v); }

static void DrawCorners(ImDrawList* dl, float x, float y, float w, float h, ImU32 c, float t) {
    float l = (w < h ? w : h) * 0.28f; if (l < 4.0f) l = 4.0f;
    dl->AddLine(ImVec2(x, y), ImVec2(x + l, y), c, t);
    dl->AddLine(ImVec2(x, y), ImVec2(x, y + l), c, t);
    dl->AddLine(ImVec2(x + w, y), ImVec2(x + w - l, y), c, t);
    dl->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + l), c, t);
    dl->AddLine(ImVec2(x, y + h), ImVec2(x + l, y + h), c, t);
    dl->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - l), c, t);
    dl->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - l, y + h), c, t);
    dl->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - l), c, t);
}

static void DrawHealthBar(ImDrawList* dl, float bx, float by, float w, float h, float frac, int side) {
    if (frac < 0.0f) frac = 0.0f; if (frac > 1.0f) frac = 1.0f;
    ImU32 hc = frac > 0.6f ? IM_COL32(60, 220, 60, 255) : frac > 0.3f ? IM_COL32(240, 220, 40, 255) : IM_COL32(240, 60, 60, 255);
    ImU32 bg = IM_COL32(0, 0, 0, 180);
    if (side == 0 || side == 1) {
        float bw = 3.5f, gap = 4.0f;
        float bxx = (side == 0) ? (bx - bw - gap) : (bx + w + gap);
        dl->AddRectFilled(ImVec2(bxx, by), ImVec2(bxx + bw, by + h), bg);
        float fill = h * frac;
        dl->AddRectFilled(ImVec2(bxx, by + h - fill), ImVec2(bxx + bw, by + h), hc);
    } else {
        float bh = 3.5f, gap = 4.0f;
        float byy = (side == 2) ? (by - bh - gap) : (by + h + gap);
        dl->AddRectFilled(ImVec2(bx, byy), ImVec2(bx + w, byy + bh), bg);
        float fill = w * frac;
        dl->AddRectFilled(ImVec2(bx, byy), ImVec2(bx + fill, byy + bh), hc);
    }
}

static inline ImVec2 RotatePoint(float x, float y, float s, float c) {
    return ImVec2(x * c - y * s, x * s + y * c);
}
static int g_RotateStartIdx = -1;
static void ImRotateStart(ImDrawList* dl) { g_RotateStartIdx = dl->VtxBuffer.Size; }
static void ImRotateEnd(ImDrawList* dl, float rad, ImVec2 center) {
    float s = sinf(rad), c = cosf(rad);
    for (int i = g_RotateStartIdx; i < dl->VtxBuffer.Size; i++) {
        ImDrawVert& v = dl->VtxBuffer.Data[i];
        float px = v.pos.x - center.x;
        float py = v.pos.y - center.y;
        ImVec2 r = RotatePoint(px, py, s, c);
        v.pos.x = r.x + center.x;
        v.pos.y = r.y + center.y;
    }
}

static void DrawRotatingLogo() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    const float posX = 28.0f, posY = 28.0f, radius = 22.0f;
    const float angle = (float)ImGui::GetTime() * 2.1f;
    dl->AddCircleFilled(ImVec2(posX, posY), radius + 4.0f, IM_COL32(0, 0, 0, 120), 32);
    dl->AddCircle(ImVec2(posX, posY), radius + 2.0f, IM_COL32(60, 180, 255, 180), 32, 1.5f);
    for (int i = 0; i < 48; ++i) {
        float a0 = angle + (float)i / 48 * 2.0f * (float)M_PI;
        float a1 = angle + (float)(i + 1) / 48 * 2.0f * (float)M_PI;
        float alpha = 80.0f + 120.0f * (0.5f + 0.5f * sinf(a0 * 2.0f));
        dl->AddLine(ImVec2(posX + cosf(a0) * radius, posY + sinf(a0) * radius),
                    ImVec2(posX + cosf(a1) * radius, posY + sinf(a1) * radius),
                    IM_COL32(80, 200, 255, (int)alpha), 2.0f);
    }
    const char* text = "F ESP";
    ImFont* font = ImGui::GetFont();
    const float fontSize = 14.0f;
    ImRotateStart(dl);
    ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
    ImVec2 textPos(posX - textSize.x * 0.5f, posY - textSize.y * 0.5f);
    dl->AddText(font, fontSize, ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 200), text);
    dl->AddText(font, fontSize, textPos, IM_COL32(100, 220, 255, 255), text);
    ImRotateEnd(dl, angle, ImVec2(posX, posY));
}

void RenderESPMenu() {
    if (!UI::IsWindowTargeted()) return;
    ImGui::SetNextWindowSize(ImVec2(380, 820), ImGuiCond_FirstUseEver);
    ImGui::Begin(L_Title(), nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::TextColored(ImVec4(0.40f, 0.85f, 1.00f, 1.00f), "%s", L_Title());
    ImGui::SameLine(); ImGui::TextDisabled("v1.1 (CE offsets)");
    ImGui::Separator();

    ImGui::Text("%s", L_Language());
    ImGui::SameLine();
    if (ImGui::RadioButton("English", g_lang == Lang::EN)) g_lang = Lang::EN;
    ImGui::SameLine();
    if (ImGui::RadioButton("Farsi", g_lang == Lang::FA)) g_lang = Lang::FA;
    ImGui::Separator();

    ImGui::Checkbox(L_EnableESP(), &cfg.enabled);
    ImGui::SameLine();
    if (ImGui::Button(L_Reattach())) AttachFromUI();
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", g_status);
    ImGui::Separator();

    if (ImGui::CollapsingHeader(L_Filters(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox(L_TeamCheck(), &cfg.teamCheck);
        ImGui::SameLine(); ImGui::TextDisabled("(LocalTeam=%d)", g_localTeam);
        ImGui::SliderFloat(L_MaxDist(), &cfg.maxDistance, 0.0f, 3000.0f, "%.0f");
    }
    if (ImGui::CollapsingHeader(L_Visuals(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox(L_Box(), &cfg.drawBox); ImGui::SameLine();
        ImGui::Checkbox(L_Corners(), &cfg.drawCorners);
        ImGui::Checkbox(L_HealthBar(), &cfg.drawHealth); ImGui::SameLine();
        ImGui::Checkbox(L_Distance(), &cfg.drawDistance);
        ImGui::Checkbox(L_DrawLocal(), &cfg.drawLocal);
        ImGui::Checkbox("Try Entity Loop (experimental)", &cfg.tryEntityLoop);
    }
    if (ImGui::CollapsingHeader(L_BoxTuning(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat(L_Thickness(), &cfg.thickness, 0.5f, 5.0f, "%.1f");
        ImGui::SliderFloat(L_HeadOffset(), &cfg.headOffset, 50.0f, 110.0f, "%.0f");
        ImGui::SliderFloat(L_WidthRatio(), &cfg.widthRatio, 0.2f, 0.8f, "%.2f");
    }
    if (ImGui::CollapsingHeader(L_Health())) {
        ImGui::SliderFloat(L_MaxHP(), &cfg.healthMax, 50.0f, 500.0f, "%.0f");
        ImGui::Combo(L_Side(), &cfg.healthSide, "Left\0Right\0Top\0Bottom\0");
    }

    if (ImGui::CollapsingHeader(L_MatrixEntity(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::TextDisabled("Matrix is auto-scanned from CE candidates");
        ImGui::Text("Best Matrix Idx : %d", g_bestMatrixIdx);
        ImGui::Text("Dist to center  : %.1f", g_bestMatrixDist);
    }

    if (ImGui::CollapsingHeader(L_Colors())) {
        ImGui::ColorEdit4(L_Enemy(), (float*)&cfg.enemyColor);
        ImGui::ColorEdit4(L_Mate(), (float*)&cfg.mateColor);
        ImGui::ColorEdit4(L_Local(), (float*)&cfg.localColor);
    }
    if (ImGui::CollapsingHeader(L_Debug(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("PID         : %lu", (unsigned long)g_attachedPid);
        ImGui::Text("Base        : 0x%llX", (unsigned long long)g_gameBase);
        ImGui::Text("Have Local  : %s", g_haveLocal ? "YES" : "NO");
        ImGui::Text("Local Pos   : %.1f  %.1f  %.1f", g_localPos.x, g_localPos.y, g_localPos.z);
        ImGui::Text("Local Team  : %d", g_localTeam);
        ImGui::Text("Matrix OK   : %s", g_dbgVmOk ? "yes" : "no");
        ImGui::Text("Entities    : %d", g_dbgCount);
        ImGui::Text("Drawn       : %d", g_dbgDrawn);
    }

    ImGui::Separator();
    if (ImGui::CollapsingHeader(L_Contact(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("%s", L_Telegram());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.30f, 0.70f, 1.00f, 1.00f), "@client_v34");
        if (ImGui::Button(L_OpenTG(), ImVec2(-1, 0)))
            ShellExecuteA(nullptr, "open", "https://t.me/client_v34", nullptr, nullptr, SW_SHOWNORMAL);
        if (ImGui::Button(L_CopyTG(), ImVec2(-1, 0))) {
            if (OpenClipboard(nullptr)) {
                EmptyClipboard();
                const char* text = "@client_v34";
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, strlen(text) + 1);
                if (hMem) {
                    memcpy(GlobalLock(hMem), text, strlen(text) + 1);
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_TEXT, hMem);
                }
                CloseClipboard();
            }
        }
    }
    ImGui::End();
}

void RenderESPBoxes() {
    if (UI::IsWindowTargeted()) DrawRotatingLogo();

    if (UI::GetTargetWindow() != g_lastTarget) {
        g_lastTarget = UI::GetTargetWindow();
        if (g_lastTarget) AttachFromUI(); else Detach();
    }
    if ((!g_hProc || !g_gameBase) && UI::IsWindowTargeted()) AttachFromUI();
    if (!g_hProc || !g_gameBase) return;

    UpdateLocal();
    if (!cfg.enabled) return;

    ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x;
    const float sh = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    // ---------- Find best View Matrix ----------
    float vm[16] = {};
    if (!FindBestMatrix(vm, sw, sh)) {
        // Still draw status text so user knows something is happening
        dl->AddText(ImVec2(20, 70), IM_COL32(255, 80, 80, 255), "Matrix not found - move camera / restart game");
        return;
    }

    g_dbgDrawn = 0;

    // ---------- Draw Local Player (always useful for verification) ----------
    if (g_haveLocal && cfg.drawLocal) {
        Vec2 screen{};
        if (WorldToScreen(g_localPos, vm, sw, sh, screen)) {
            // Green filled circle + label (same as Python proof)
            dl->AddCircleFilled(ImVec2(screen.x, screen.y), 6.0f, IM_COL32(50, 255, 50, 220), 16);
            dl->AddCircle(ImVec2(screen.x, screen.y), 8.0f, IM_COL32(0, 0, 0, 200), 16, 2.0f);
            dl->AddText(ImVec2(screen.x + 12, screen.y - 8), IM_COL32(50, 255, 50, 255), "YOU");

            // Optional small box around local
            if (cfg.drawBox) {
                Vec3 head = g_localPos;
                head.z += cfg.headOffset;
                Vec2 headScr{};
                if (WorldToScreen(head, vm, sw, sh, headScr)) {
                    float h = screen.y - headScr.y;
                    if (h > 5.0f && h < 800.0f) {
                        float w = h * cfg.widthRatio;
                        float bx = headScr.x - w * 0.5f;
                        float by = headScr.y;
                        dl->AddRect(ImVec2(bx, by), ImVec2(bx + w, by + h), C(cfg.localColor), 0.0f, 0, cfg.thickness);
                    }
                }
            }
            ++g_dbgDrawn;
        }
    }

    // ---------- Experimental Entity Loop (still unreliable) ----------
    if (cfg.tryEntityLoop) {
        uintptr_t listBase = 0;
        if (ReadPtr(g_gameBase + Off::ENTITY_LIST_RVA, listBase) && listBase > 0x10000) {
            // try a reasonable number of slots
            const int MAX_ENT = 64;
            g_dbgCount = 0;

            for (int i = 0; i < MAX_ENT; ++i) {
                uintptr_t ent = listBase + (uintptr_t)i * Off::PLAYER_STRIDE;

                int hp = 0, team = 0;
                Vec3 origin{};
                if (!RPM(ent + Off::OBJ_HP, hp)) continue;
                if (!RPM(ent + Off::OBJ_TEAM, team)) continue;
                if (!RPM(ent + Off::OBJ_ORIGIN, origin)) continue;

                if (hp <= 0 || hp > 1000) continue;
                if (origin.x == 0.f && origin.y == 0.f && origin.z == 0.f) continue;

                // skip local roughly
                float dx = origin.x - g_localPos.x;
                float dy = origin.y - g_localPos.y;
                float dz = origin.z - g_localPos.z;
                float dist = sqrtf(dx*dx + dy*dy + dz*dz);
                if (dist < 5.0f) continue; // probably ourselves

                if (cfg.teamCheck && g_localTeam != 0 && team == g_localTeam) continue;
                if (cfg.maxDistance > 0.f && dist > cfg.maxDistance) continue;

                Vec3 head = origin; head.z += cfg.headOffset;
                Vec2 feet{}, headScr{};
                if (!WorldToScreen(origin, vm, sw, sh, feet)) continue;
                if (!WorldToScreen(head, vm, sw, sh, headScr)) continue;

                float h = feet.y - headScr.y;
                if (h < 4.0f || h > 2000.0f) continue;
                float w = h * cfg.widthRatio;
                float bx = headScr.x - w * 0.5f;
                float by = headScr.y;

                ImU32 col = (g_localTeam != 0 && team == g_localTeam) ? C(cfg.mateColor) : C(cfg.enemyColor);

                if (cfg.drawBox)     dl->AddRect(ImVec2(bx, by), ImVec2(bx + w, by + h), col, 0.0f, 0, cfg.thickness);
                if (cfg.drawCorners) DrawCorners(dl, bx, by, w, h, col, cfg.thickness + 0.5f);
                if (cfg.drawHealth)  DrawHealthBar(dl, bx, by, w, h, (float)hp / cfg.healthMax, cfg.healthSide);
                if (cfg.drawDistance) {
                    char buf[48];
                    sprintf_s(buf, "%.0f", dist);
                    ImVec2 ts = ImGui::CalcTextSize(buf);
                    dl->AddText(ImVec2(bx + w * 0.5f - ts.x * 0.5f, by + h + 2.0f), IM_COL32(255, 255, 255, 230), buf);
                }
                ++g_dbgDrawn;
                ++g_dbgCount;
            }
        }
    }
}

void RenderESP() {
    RenderESPMenu();
    RenderESPBoxes();
}
