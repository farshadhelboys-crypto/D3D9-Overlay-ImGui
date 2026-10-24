// =====================================================================
//  ESP - Integrated with UI / Drawing framework (EN / FA bilingual)
//  Updated with WORKING offsets from verified Python ESP (2026-10-24)
// =====================================================================
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cfloat>
#include <vector>
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
// WORKING OFFSETS (from verified Python ESP - 2026-10-24)
// ============================================================
namespace Off {
    // Local player direct floats (module relative)
    constexpr uintptr_t MY_POS_X     = 0x242F510;
    constexpr uintptr_t MY_POS_Y     = 0x242F514;
    constexpr uintptr_t MY_POS_Z     = 0x242F518;
    constexpr uintptr_t MY_YAW       = 0x25982CC;
    constexpr uintptr_t MY_PITCH     = 0x25982D0;

    // View Matrix (module relative) - verified working
    constexpr uintptr_t MATRIX_RVA   = 0x27603D0;

    // Offline Entity Array (V8 style)
    constexpr uintptr_t PLAYER_ARRAY_RVA = 0x26B2364;
    constexpr uintptr_t ARR_STRIDE       = 0x14;
    constexpr uintptr_t ARR_OBJ_OFF      = 0x0C;
    constexpr uintptr_t OBJ_POS_OFF      = 0x290;
    constexpr uintptr_t OBJ_HP_OFF       = 0x11C;
    constexpr uintptr_t OBJ_TEAM_OFF     = 0xD4;

    // Online VTable signatures (module relative)
    constexpr uintptr_t VTABLE_1     = 0x01DDAC3C;
    constexpr uintptr_t VTABLE_2     = 0x01DDA9C8;
    constexpr uintptr_t V_OBJ_ORIGIN = 0x538;

    // Optional resource table
    constexpr uintptr_t PLAYER_RES_RVA = 0x2478218;
    constexpr uintptr_t RES_HP         = 0x1270;
    constexpr uintptr_t RES_TEAM       = 0x1128;
}

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };

struct EspConfig {
    bool  enabled       = true;
    bool  teamCheck     = false;
    bool  drawBox       = true;
    bool  drawCorners   = false;
    bool  drawHealth    = true;
    bool  drawDistance  = true;
    bool  drawLocal     = true;
    bool  useOffline    = true;   // V8 array
    bool  useOnline     = false;  // VTable objects (heavier)
    float headOffset    = 72.0f;
    float widthRatio    = 0.45f;
    float thickness     = 1.8f;
    float maxDistance   = 1500.0f;
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
static Vec3       g_localPos    = {0, 0, 0};
static bool       g_haveLocal   = false;
static char       g_status[160] = "Not attached";
static int        g_dbgCount    = 0;
static int        g_dbgDrawn    = 0;
static bool       g_dbgVmOk     = false;

// Cached online objects (scanned once on attach / re-attach)
static std::vector<uintptr_t> g_vObjects;
static bool g_vObjectsScanned = false;

template<typename T>
static inline bool RPM(uintptr_t addr, T& out) {
    SIZE_T read = 0;
    return ReadProcessMemory(g_hProc, (LPCVOID)addr, &out, sizeof(T), &read) && read == sizeof(T);
}

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
    g_gameBase = 0; g_attachedPid = 0; g_localTeam = 0;
    g_haveLocal = false;
    g_dbgCount = 0; g_dbgDrawn = 0; g_dbgVmOk = false;
    g_vObjects.clear();
    g_vObjectsScanned = false;
    strcpy_s(g_status, "Not attached");
}

// Lightweight VTable object scan (only called on attach)
static void ScanVTableObjects() {
    g_vObjects.clear();
    g_vObjectsScanned = false;
    if (!g_hProc || !g_gameBase) return;

    const uint32_t targets[2] = {
        (uint32_t)(g_gameBase + Off::VTABLE_1),
        (uint32_t)(g_gameBase + Off::VTABLE_2)
    };

    // Scan a reasonable heap range (same idea as Python but limited for speed)
    // Full 0x10000000-0x40000000 is too slow for realtime; we do a lighter pass.
    const uintptr_t start = 0x10000000;
    const uintptr_t end   = 0x30000000;
    const size_t chunk = 0x100000; // 1 MB

    std::vector<uint8_t> buf(chunk);

    for (uintptr_t addr = start; addr < end; addr += chunk) {
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(g_hProc, (LPCVOID)addr, buf.data(), chunk, &bytesRead) || bytesRead < 16)
            continue;

        for (size_t i = 0; i + 4 <= bytesRead; i += 4) {
            uint32_t val = *reinterpret_cast<uint32_t*>(buf.data() + i);
            if (val == targets[0] || val == targets[1]) {
                g_vObjects.push_back(addr + i);
            }
        }
    }
    g_vObjectsScanned = true;
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

    // Optional: scan online objects (can take 1-3 seconds)
    if (cfg.useOnline) {
        strcpy_s(g_status, "Scanning VTable objects...");
        ScanVTableObjects();
        sprintf_s(g_status, "Attached - PID %lu | VObj %zu", (unsigned long)pid, g_vObjects.size());
    } else {
        sprintf_s(g_status, "Attached - PID %lu | Base 0x%llX", (unsigned long)pid, (unsigned long long)g_gameBase);
    }
    return true;
}

// -----------------------------------------------------------------
// Local player (direct floats - verified working)
// -----------------------------------------------------------------
static void UpdateLocal() {
    g_haveLocal = false;
    g_localTeam = 0;
    if (!g_hProc || !g_gameBase) return;

    Vec3 pos{};
    if (RPM(g_gameBase + Off::MY_POS_X, pos.x) &&
        RPM(g_gameBase + Off::MY_POS_Y, pos.y) &&
        RPM(g_gameBase + Off::MY_POS_Z, pos.z)) {
        if (fabsf(pos.x) > 1.0f || fabsf(pos.y) > 1.0f || fabsf(pos.z) > 1.0f) {
            g_localPos = pos;
            g_haveLocal = true;
        }
    }
}

// -----------------------------------------------------------------
// WorldToScreen matching the working Python layout
// mat layout used by the verified script:
// cx = m[0]*x + m[1]*y + m[2]*z + m[3]
// cy = m[4]*x + m[5]*y + m[6]*z + m[7]
// cw = m[12]*x + m[13]*y + m[14]*z + m[15]
// -----------------------------------------------------------------
static bool WorldToScreen(const Vec3& pos, const float* mat, float sw, float sh, Vec2& out) {
    float cx = mat[0] * pos.x + mat[1] * pos.y + mat[2] * pos.z + mat[3];
    float cy = mat[4] * pos.x + mat[5] * pos.y + mat[6] * pos.z + mat[7];
    float cw = mat[12] * pos.x + mat[13] * pos.y + mat[14] * pos.z + mat[15];

    if (fabsf(cw) < 0.001f) return false;
    if (cw < 0.2f) return false; // behind camera-ish

    float ndc_x = cx / cw;
    float ndc_y = cy / cw;

    // Standard conversion (more stable than the Python variant)
    out.x = (sw * 0.5f) * (1.0f + ndc_x);
    out.y = (sh * 0.5f) * (1.0f - ndc_y);
    return true;
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

// ---------- Rotating logo ----------
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

// -----------------------------------------------------------------
// Menu
// -----------------------------------------------------------------
void RenderESPMenu() {
    if (!UI::IsWindowTargeted()) return;
    ImGui::SetNextWindowSize(ImVec2(380, 780), ImGuiCond_FirstUseEver);
    ImGui::Begin(L_Title(), nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::TextColored(ImVec4(0.40f, 0.85f, 1.00f, 1.00f), "%s", L_Title());
    ImGui::SameLine(); ImGui::TextDisabled("v2.0 (Working offsets)");
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
        ImGui::Checkbox("Offline Array (V8)", &cfg.useOffline);
        ImGui::Checkbox("Online VTable (slow scan)", &cfg.useOnline);
    }
    if (ImGui::CollapsingHeader(L_BoxTuning(), ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat(L_Thickness(), &cfg.thickness, 0.5f, 5.0f, "%.1f");
        ImGui::SliderFloat(L_HeadOffset(), &cfg.headOffset, 40.0f, 120.0f, "%.0f");
        ImGui::SliderFloat(L_WidthRatio(), &cfg.widthRatio, 0.2f, 0.8f, "%.2f");
    }
    if (ImGui::CollapsingHeader(L_Health())) {
        ImGui::SliderFloat(L_MaxHP(), &cfg.healthMax, 50.0f, 500.0f, "%.0f");
        ImGui::Combo(L_Side(), &cfg.healthSide, "Left\0Right\0Top\0Bottom\0");
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
        ImGui::Text("Matrix OK   : %s", g_dbgVmOk ? "yes" : "no");
        ImGui::Text("Entities    : %d", g_dbgCount);
        ImGui::Text("Drawn       : %d", g_dbgDrawn);
        ImGui::Text("VTable objs : %zu", g_vObjects.size());
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

// -----------------------------------------------------------------
// Main draw
// -----------------------------------------------------------------
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

    // ---- Read View Matrix (direct RVA that works in Python) ----
    float mat[16] = {};
    g_dbgVmOk = RPM(g_gameBase + Off::MATRIX_RVA, mat);
    if (!g_dbgVmOk) {
        dl->AddText(ImVec2(20, 70), IM_COL32(255, 80, 80, 255), "Matrix read failed");
        return;
    }

    g_dbgDrawn = 0;
    g_dbgCount = 0;

    // ---- Draw Local Player marker ----
    if (g_haveLocal && cfg.drawLocal) {
        Vec2 screen{};
        if (WorldToScreen(g_localPos, mat, sw, sh, screen)) {
            dl->AddCircleFilled(ImVec2(screen.x, screen.y), 5.0f, IM_COL32(50, 255, 50, 220), 12);
            dl->AddCircle(ImVec2(screen.x, screen.y), 7.0f, IM_COL32(0, 0, 0, 200), 12, 2.0f);
            dl->AddText(ImVec2(screen.x + 10, screen.y - 8), IM_COL32(50, 255, 50, 255), "YOU");
            ++g_dbgDrawn;
        }
    }

    // ============================================================
    // OFFLINE ARRAY (V8 style) - main reliable path
    // ============================================================
    if (cfg.useOffline) {
        for (int i = 0; i < 64; ++i) {
            uintptr_t slot = g_gameBase + Off::PLAYER_ARRAY_RVA + (uintptr_t)i * Off::ARR_STRIDE;
            uintptr_t obj = 0;
            if (!ReadPtr(slot + Off::ARR_OBJ_OFF, obj) || obj < 0x10000) continue;

            Vec3 origin{};
            if (!RPM(obj + Off::OBJ_POS_OFF, origin.x)) continue;
            if (!RPM(obj + Off::OBJ_POS_OFF + 4, origin.y)) continue;
            if (!RPM(obj + Off::OBJ_POS_OFF + 8, origin.z)) continue;

            if (fabsf(origin.x) < 1.0f || fabsf(origin.x) > 20000.0f) continue;

            int hp = 0, team = 0;
            RPM(obj + Off::OBJ_HP_OFF, hp);
            RPM(obj + Off::OBJ_TEAM_OFF, team);

            float dist = 0.0f;
            if (g_haveLocal) {
                float dx = origin.x - g_localPos.x;
                float dy = origin.y - g_localPos.y;
                float dz = origin.z - g_localPos.z;
                dist = sqrtf(dx*dx + dy*dy + dz*dz);
            }
            if (dist < 3.0f || dist > cfg.maxDistance) continue;

            if (cfg.teamCheck && g_localTeam != 0 && team == g_localTeam) continue;

            Vec3 head = origin;
            head.z += cfg.headOffset;

            Vec2 feet{}, headScr{};
            if (!WorldToScreen(origin, mat, sw, sh, feet)) continue;
            if (!WorldToScreen(head, mat, sw, sh, headScr)) continue;

            float h = feet.y - headScr.y;
            if (h < 4.0f || h > 2000.0f) continue;
            float w = h * cfg.widthRatio;
            float bx = headScr.x - w * 0.5f;
            float by = headScr.y;

            ImU32 col = (g_localTeam != 0 && team == g_localTeam) ? C(cfg.mateColor) : C(cfg.enemyColor);

            if (cfg.drawBox)
                dl->AddRect(ImVec2(bx, by), ImVec2(bx + w, by + h), col, 0.0f, 0, cfg.thickness);
            if (cfg.drawCorners)
                DrawCorners(dl, bx, by, w, h, col, cfg.thickness + 0.5f);
            if (cfg.drawHealth && hp > 0)
                DrawHealthBar(dl, bx, by, w, h, (float)hp / cfg.healthMax, cfg.healthSide);
            if (cfg.drawDistance) {
                char buf[48];
                sprintf_s(buf, "%.0fm", dist);
                ImVec2 ts = ImGui::CalcTextSize(buf);
                dl->AddText(ImVec2(bx + w * 0.5f - ts.x * 0.5f, by + h + 2.0f), IM_COL32(255, 255, 255, 230), buf);
            }
            ++g_dbgDrawn;
            ++g_dbgCount;
        }
    }

    // ============================================================
    // ONLINE VTable objects (optional, scanned on attach)
    // ============================================================
    if (cfg.useOnline && g_vObjectsScanned) {
        for (uintptr_t obj : g_vObjects) {
            Vec3 origin{};
            if (!RPM(obj + Off::V_OBJ_ORIGIN, origin.x)) continue;
            if (!RPM(obj + Off::V_OBJ_ORIGIN + 4, origin.y)) continue;
            if (!RPM(obj + Off::V_OBJ_ORIGIN + 8, origin.z)) continue;

            if (fabsf(origin.x) > 20000.0f || fabsf(origin.y) > 20000.0f) continue;
            if (fabsf(origin.x) < 0.1f && fabsf(origin.y) < 0.1f) continue;

            float dist = 0.0f;
            if (g_haveLocal) {
                float dx = origin.x - g_localPos.x;
                float dy = origin.y - g_localPos.y;
                float dz = origin.z - g_localPos.z;
                dist = sqrtf(dx*dx + dy*dy + dz*dz);
            }
            if (dist < 50.0f || dist > cfg.maxDistance) continue;

            Vec3 head = origin;
            head.z += cfg.headOffset;

            Vec2 feet{}, headScr{};
            if (!WorldToScreen(origin, mat, sw, sh, feet)) continue;
            if (!WorldToScreen(head, mat, sw, sh, headScr)) continue;

            float h = feet.y - headScr.y;
            if (h < 4.0f || h > 2000.0f) continue;
            float w = h * cfg.widthRatio;
            float bx = headScr.x - w * 0.5f;
            float by = headScr.y;

            ImU32 col = C(cfg.enemyColor);
            if (cfg.drawBox)
                dl->AddRect(ImVec2(bx, by), ImVec2(bx + w, by + h), col, 0.0f, 0, cfg.thickness);
            if (cfg.drawDistance) {
                char buf[32];
                sprintf_s(buf, "ON %.0f", dist);
                dl->AddText(ImVec2(bx, by - 14), IM_COL32(255, 100, 100, 230), buf);
            }
            ++g_dbgDrawn;
            ++g_dbgCount;
        }
    }
}

void RenderESP() {
    RenderESPMenu();
    RenderESPBoxes();
}
