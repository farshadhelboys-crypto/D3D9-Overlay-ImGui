// =====================================================================
//  ESP - Integrated with UI / Drawing framework (EN / FA bilingual)
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
static const Loc L_PtrArray     = { "Entity Mode: List A",       "Entity Mode: List A" };
static const Loc L_Colors       = { "Colors",                   "Colors" };
static const Loc L_Enemy        = { "Enemy",                    "Enemy" };
static const Loc L_Mate         = { "Mate",                     "Mate" };
static const Loc L_Local        = { "Local",                    "Local" };
static const Loc L_Debug        = { "Debug",                    "Debug" };
static const Loc L_Contact      = { "Contact",                  "Contact" };
static const Loc L_Telegram     = { "Telegram:",                "Telegram:" };
static const Loc L_OpenTG       = { "Open Telegram",            "Open Telegram" };
static const Loc L_CopyTG       = { "Copy @client_v34",         "Copy @client_v34" };

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

template<typename T>
static inline bool RPM(uintptr_t addr, T& out) {
    return ReadProcessMemory(g_hProc, (LPCVOID)addr, &out, sizeof(T), nullptr);
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
    strcpy_s(g_status, "Not attached");
}

static bool AttachFromUI() {
    Detach();
    if (!UI::IsWindowTargeted()) { strcpy_s(g_status, "No target window"); return false; }
    HWND hwnd = UI::GetTargetWindow();
    DWORD pid = UI::GetTargetPID();
    if (!pid && hwnd) GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) { strcpy_s(g_status, "Target PID invalid"); return false; }
    g_hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!g_hProc) { strcpy_s(g_status, "OpenProcess failed (run as admin)"); return false; }
    g_gameBase = GetModuleBase(pid, L"clientmod_game.dll");
    if (!g_gameBase) {
        strcpy_s(g_status, "clientmod_game.dll not loaded yet");
        CloseHandle(g_hProc); g_hProc = nullptr; return false;
    }
    g_attachedPid = pid;
    sprintf_s(g_status, "Attached - PID: %lu", (unsigned long)pid);
    return true;
}

static void UpdateLocal() {
    g_localTeam = 0; g_localPtr = 0; g_haveLocal = false;
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
    ImGui::SetNextWindowSize(ImVec2(380, 780), ImGuiCond_FirstUseEver);
    ImGui::Begin(L_Title(), nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::TextColored(ImVec4(0.40f, 0.85f, 1.00f, 1.00f), "%s", L_Title());
    ImGui::SameLine(); ImGui::TextDisabled("v1.0");
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
        const char* matrixItems = "Option 1 (Default)\0Option 2 (Backup)\0";
        ImGui::Combo(L_MatrixSrc(), &cfg.matrixChoice, matrixItems);
        ImGui::Checkbox("Entity Mode: List A", &cfg.pointerArray);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("If boxes don't appear, untick this (switches to List B)");
    }

    if (ImGui::CollapsingHeader(L_Colors())) {
        ImGui::ColorEdit4(L_Enemy(), (float*)&cfg.enemyColor);
        ImGui::ColorEdit4(L_Mate(), (float*)&cfg.mateColor);
        ImGui::ColorEdit4(L_Local(), (float*)&cfg.localColor);
    }
    if (ImGui::CollapsingHeader(L_Debug())) {
        ImGui::Text("PID      : %lu", (unsigned long)g_attachedPid);
        ImGui::Text("Entity   : %d", g_dbgCount);
        ImGui::Text("Drawn    : %d", g_dbgDrawn);
        ImGui::Text("Matrix OK: %s", g_dbgVmOk ? "yes" : "no");
        ImGui::Text("LocalTeam: %d", g_localTeam);
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

    Mat4 vm{};
    uintptr_t vmAddr = g_gameBase + (cfg.matrixChoice == 0 ? Off::VIEW_MATRIX : Off::VIEW_MATRIX_ALT);
    g_dbgVmOk = RPM(vmAddr, vm);
    if (!g_dbgVmOk) return;

    int count = 0;
    if (!RPM(g_gameBase + Off::ENTITY_COUNT, count)) return;
    g_dbgCount = count; g_dbgDrawn = 0;
    if (count <= 0 || count > 512) return;

    uintptr_t listBase = 0;
    if (cfg.pointerArray) {
        if (!RPM(g_gameBase + Off::ENTITY_ARRAY_PTR, listBase)) return;
        if (listBase < 0x10000) return;
    } else {
        listBase = g_gameBase + Off::ENTITY_ARRAY_PTR;
    }

    ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x, sh = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    for (int i = 0; i < count; ++i) {
        uintptr_t ent = listBase + (uintptr_t)i * Off::ENTITY_STRIDE;
        int hp = 0, team = 0; Vec3 origin{};
        if (!RPM(ent + Off::OBJ_HP, hp)) continue;
        if (!RPM(ent + Off::OBJ_TEAM, team)) continue;
        if (!RPM(ent + Off::OBJ_ORIGIN, origin)) continue;
        if (hp <= 0 || hp > 1000) continue;
        if (origin.x == 0.0f && origin.y == 0.0f && origin.z == 0.0f) continue;
        const bool isLocal = (g_localPtr != 0 && ent == g_localPtr);
        if (isLocal && !cfg.drawLocal) continue;
        if (!isLocal && cfg.teamCheck && g_localTeam != 0 && team == g_localTeam) continue;
        Vec3 head = origin; head.z += cfg.headOffset;
        Vec2 sf{}, shd{};
        if (!WorldToScreen(origin, sf, vm, sw, sh)) continue;
        if (!WorldToScreen(head, shd, vm, sw, sh)) continue;
        float h = sf.y - shd.y;
        if (h < 3.0f || h > 3000.0f) continue;
        float w = h * cfg.widthRatio;
        float bx = shd.x - w * 0.5f, by = shd.y;
        if (bx + w < 0 || bx > sw || by + h < 0 || by > sh) continue;
        float dist;
        if (g_haveLocal) {
            float dx = origin.x - g_localPos.x, dy = origin.y - g_localPos.y, dz = origin.z - g_localPos.z;
            dist = sqrtf(dx*dx + dy*dy + dz*dz);
        } else dist = sqrtf(origin.x*origin.x + origin.y*origin.y + origin.z*origin.z);
        if (cfg.maxDistance > 0.0f && dist > cfg.maxDistance) continue;
        ImU32 col = isLocal ? C(cfg.localColor) : (g_localTeam != 0 && team == g_localTeam) ? C(cfg.mateColor) : C(cfg.enemyColor);
        if (cfg.drawBox) dl->AddRect(ImVec2(bx, by), ImVec2(bx + w, by + h), col, 0.0f, 0, cfg.thickness);
        if (cfg.drawCorners) DrawCorners(dl, bx, by, w, h, col, cfg.thickness + 0.5f);
        if (cfg.drawHealth) DrawHealthBar(dl, bx, by, w, h, (float)hp / cfg.healthMax, cfg.healthSide);
        if (cfg.drawDistance) {
            char buf[48]; sprintf_s(buf, "#%d  %.0f", i, dist);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(bx + w * 0.5f - ts.x * 0.5f, by + h + 2.0f), IM_COL32(255, 255, 255, 230), buf);
        }
        ++g_dbgDrawn;
    }
}

void RenderESP() {
    RenderESPMenu();
    RenderESPBoxes();
}
