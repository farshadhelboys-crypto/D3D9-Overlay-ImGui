// =====================================================================
//  ESP - VTable only + anti-blink + test aimbot + tabbed UI
//  No offsets visible in UI
// =====================================================================
#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdint>
#include <cfloat>
#include <vector>
#include <algorithm>
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
static const Loc L_Reattach     = { "Re-attach / Rescan",       "Re-attach / Rescan" };
static const Loc L_TeamCheck    = { "Team Check",               "Team Check" };
static const Loc L_MaxDist      = { "Max Distance",             "Max Distance" };
static const Loc L_Box          = { "Box",                      "Box" };
static const Loc L_Corners      = { "Corners",                  "Corners" };
static const Loc L_HealthBar    = { "Health Bar",               "Health Bar" };
static const Loc L_Distance     = { "Distance",                 "Distance" };
static const Loc L_Thickness    = { "Thickness",                "Thickness" };
static const Loc L_BoxHeight    = { "Box Height",               "Box Height" };
static const Loc L_WidthRatio   = { "Width Ratio",              "Width Ratio" };
static const Loc L_MaxHP        = { "Max HP",                   "Max HP" };
static const Loc L_Side         = { "Side",                     "Side" };
static const Loc L_AimEnable    = { "Enable Aimbot (test)",     "Enable Aimbot (test)" };
static const Loc L_AimSmooth    = { "Smooth",                   "Smooth" };
static const Loc L_AimFov       = { "FOV (px)",                 "FOV (px)" };
static const Loc L_Enemy        = { "Enemy",                    "Enemy" };
static const Loc L_Mate         = { "Mate",                     "Mate" };
static const Loc L_Telegram     = { "Telegram:",                "Telegram:" };
static const Loc L_OpenTG       = { "Open Telegram",            "Open Telegram" };
static const Loc L_CopyTG       = { "Copy @client_v34",         "Copy @client_v34" };

static const Loc L_TabMain      = { "Main",                     "اصلی" };
static const Loc L_TabVisuals   = { "Visuals",                  "نمایش" };
static const Loc L_TabAimbot    = { "Aimbot",                   "ایم‌بات" };
static const Loc L_TabColors    = { "Colors",                   "رنگ‌ها" };
static const Loc L_TabDebug     = { "Status",                   "وضعیت" };
static const Loc L_TabContact   = { "Contact",                  "ارتباط" };

namespace Off {
    constexpr uintptr_t MY_POS_X     = 0x242F510;
    constexpr uintptr_t MY_POS_Y     = 0x242F514;
    constexpr uintptr_t MY_POS_Z     = 0x242F518;
    constexpr uintptr_t MATRIX_RVA   = 0x27603D0;
    constexpr uintptr_t VTABLE_1     = 0x01DDAC3C;
    constexpr uintptr_t VTABLE_2     = 0x01DDA9C8;
    constexpr uintptr_t V_OBJ_ORIGIN = 0x538;
    constexpr uintptr_t OBJ_HP_OFF   = 0x11C;
    constexpr uintptr_t OBJ_TEAM_OFF = 0xD4;
}

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };

struct TrackedEntity {
    uintptr_t obj = 0;
    Vec3      pos = {0,0,0};
    int       hp = 0;
    int       team = 0;
    int       life = 0;
    bool      valid = false;
};

struct EspConfig {
    bool  enabled       = true;
    bool  teamCheck     = false;
    bool  drawBox       = true;
    bool  drawCorners   = false;
    bool  drawHealth    = false;
    bool  drawDistance  = true;
    bool  aimbot        = false;
    float aimSmooth     = 6.0f;
    float aimFov        = 180.0f;
    float headOffset    = 72.0f;
    float widthRatio    = 0.42f;
    float thickness     = 1.8f;
    float maxDistance   = 2000.0f;
    float healthMax     = 100.0f;
    int   healthSide    = 0;
    ImVec4 enemyColor   = ImVec4(1.00f, 0.25f, 0.25f, 1.00f);
    ImVec4 mateColor    = ImVec4(0.25f, 0.85f, 1.00f, 1.00f);
} cfg;

static HANDLE     g_hProc       = nullptr;
static uintptr_t  g_gameBase    = 0;
static DWORD      g_attachedPid = 0;
static HWND       g_lastTarget  = nullptr;
static Vec3       g_localPos    = {0, 0, 0};
static bool       g_haveLocal   = false;
static char       g_status[160] = "Not attached";
static int        g_dbgCount    = 0;
static int        g_dbgDrawn    = 0;
static bool       g_dbgVmOk     = false;

static std::vector<uintptr_t>     g_rawObjects;
static std::vector<TrackedEntity> g_tracked;
static bool g_scanned = false;
static const int kMaxLife = 8;

template<typename T>
static inline bool RPM(uintptr_t addr, T& out) {
    SIZE_T read = 0;
    return ReadProcessMemory(g_hProc, (LPCVOID)addr, &out, sizeof(T), &read) && read == sizeof(T);
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
    g_gameBase = 0; g_attachedPid = 0;
    g_haveLocal = false;
    g_dbgCount = 0; g_dbgDrawn = 0; g_dbgVmOk = false;
    g_rawObjects.clear();
    g_tracked.clear();
    g_scanned = false;
    strcpy_s(g_status, "Not attached");
}

static void ScanVTableObjects() {
    g_rawObjects.clear();
    g_tracked.clear();
    g_scanned = false;
    if (!g_hProc || !g_gameBase) return;

    const uint32_t targets[2] = {
        (uint32_t)(g_gameBase + Off::VTABLE_1),
        (uint32_t)(g_gameBase + Off::VTABLE_2)
    };

    const uintptr_t start = 0x10000000;
    const uintptr_t end   = 0x40000000;
    const size_t chunk = 0x100000;
    std::vector<uint8_t> buf(chunk);

    for (uintptr_t addr = start; addr < end; addr += chunk) {
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(g_hProc, (LPCVOID)addr, buf.data(), chunk, &bytesRead) || bytesRead < 16)
            continue;
        for (size_t i = 0; i + 4 <= bytesRead; i += 4) {
            uint32_t val = *reinterpret_cast<uint32_t*>(buf.data() + i);
            if (val == targets[0] || val == targets[1])
                g_rawObjects.push_back(addr + i);
        }
    }

    g_tracked.reserve(g_rawObjects.size());
    for (uintptr_t o : g_rawObjects) {
        TrackedEntity te;
        te.obj = o;
        g_tracked.push_back(te);
    }
    g_scanned = true;
    sprintf_s(g_status, "Attached | Players found: %zu", g_rawObjects.size());
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
        strcpy_s(g_status, "Game module not loaded yet");
        CloseHandle(g_hProc); g_hProc = nullptr; return false;
    }
    g_attachedPid = pid;
    strcpy_s(g_status, "Scanning players (wait)...");
    ScanVTableObjects();
    return true;
}

static void UpdateLocal() {
    g_haveLocal = false;
    if (!g_hProc || !g_gameBase) return;
    Vec3 pos{};
    if (RPM(g_gameBase + Off::MY_POS_X, pos.x) &&
        RPM(g_gameBase + Off::MY_POS_Y, pos.y) &&
        RPM(g_gameBase + Off::MY_POS_Z, pos.z)) {
        if (fabsf(pos.x) > 0.5f || fabsf(pos.y) > 0.5f || fabsf(pos.z) > 0.5f) {
            g_localPos = pos;
            g_haveLocal = true;
        }
    }
}

static bool WorldToScreen(const Vec3& pos, const float* mat, float sw, float sh, Vec2& out) {
    float cx = mat[0] * pos.x + mat[1] * pos.y + mat[2] * pos.z + mat[3];
    float cy = mat[4] * pos.x + mat[5] * pos.y + mat[6] * pos.z + mat[7];
    float cw = mat[12] * pos.x + mat[13] * pos.y + mat[14] * pos.z + mat[15];
    if (fabsf(cw) < 0.001f || cw < 0.15f) return false;
    float ndc_x = cx / cw;
    float ndc_y = cy / cw;
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

static void RunTestAimbot(float targetX, float targetY, float sw, float sh) {
    if (!cfg.aimbot) return;
    float cx = sw * 0.5f, cy = sh * 0.5f;
    float dx = targetX - cx, dy = targetY - cy;
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist < 2.0f || dist > cfg.aimFov) return;
    float smooth = cfg.aimSmooth < 1.0f ? 1.0f : cfg.aimSmooth;
    int mx = (int)(dx / smooth);
    int my = (int)(dy / smooth);
    if (mx != 0 || my != 0)
        mouse_event(MOUSEEVENTF_MOVE, mx, my, 0, 0);
}

void RenderESPMenu() {
    if (!UI::IsWindowTargeted()) return;

    ImGui::SetNextWindowSize(ImVec2(360, 420), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320, 280), ImVec2(500, 700));

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysVerticalScrollbar;
    if (!ImGui::Begin(L_Title(), nullptr, flags))
    {
        ImGui::End();
        return;
    }

    ImGui::TextColored(ImVec4(0.40f, 0.85f, 1.00f, 1.00f), "%s", L_Title());
    ImGui::SameLine();
    ImGui::TextDisabled("v2.2");
    ImGui::Separator();

    ImGui::Text("%s", L_Language());
    ImGui::SameLine();
    if (ImGui::RadioButton("EN", g_lang == Lang::EN)) g_lang = Lang::EN;
    ImGui::SameLine();
    if (ImGui::RadioButton("FA", g_lang == Lang::FA)) g_lang = Lang::FA;

    ImGui::Checkbox(L_EnableESP(), &cfg.enabled);
    ImGui::SameLine();
    if (ImGui::Button(L_Reattach())) AttachFromUI();
    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", g_status);
    ImGui::Separator();

    if (ImGui::BeginTabBar("EspTabs", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable))
    {
        if (ImGui::BeginTabItem(L_TabMain()))
        {
            ImGui::Checkbox(L_TeamCheck(), &cfg.teamCheck);
            ImGui::SliderFloat(L_MaxDist(), &cfg.maxDistance, 50.0f, 4000.0f, "%.0f");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(L_TabVisuals()))
        {
            ImGui::Checkbox(L_Box(), &cfg.drawBox);
            ImGui::SameLine();
            ImGui::Checkbox(L_Corners(), &cfg.drawCorners);
            ImGui::Checkbox(L_HealthBar(), &cfg.drawHealth);
            ImGui::SameLine();
            ImGui::Checkbox(L_Distance(), &cfg.drawDistance);

            ImGui::Separator();
            ImGui::SliderFloat(L_Thickness(), &cfg.thickness, 0.5f, 5.0f, "%.1f");
            ImGui::SliderFloat(L_BoxHeight(), &cfg.headOffset, 40.0f, 120.0f, "%.0f");
            ImGui::SliderFloat(L_WidthRatio(), &cfg.widthRatio, 0.2f, 0.8f, "%.2f");

            ImGui::Separator();
            ImGui::SliderFloat(L_MaxHP(), &cfg.healthMax, 50.0f, 500.0f, "%.0f");
            ImGui::Combo(L_Side(), &cfg.healthSide, "Left\0Right\0Top\0Bottom\0");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(L_TabAimbot()))
        {
            ImGui::Checkbox(L_AimEnable(), &cfg.aimbot);
            ImGui::SliderFloat(L_AimSmooth(), &cfg.aimSmooth, 1.0f, 20.0f, "%.1f");
            ImGui::SliderFloat(L_AimFov(), &cfg.aimFov, 30.0f, 500.0f, "%.0f");
            ImGui::TextDisabled("Aims at closest box center inside FOV");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(L_TabColors()))
        {
            ImGui::ColorEdit4(L_Enemy(), (float*)&cfg.enemyColor);
            ImGui::ColorEdit4(L_Mate(), (float*)&cfg.mateColor);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(L_TabDebug()))
        {
            ImGui::Text("Connected   : %s", g_hProc ? "Yes" : "No");
            ImGui::Text("Local Player: %s", g_haveLocal ? "Found" : "Not found");
            ImGui::Text("Camera      : %s", g_dbgVmOk ? "OK" : "Failed");
            ImGui::Text("Players     : %d", g_dbgCount);
            ImGui::Text("Boxes drawn : %d", g_dbgDrawn);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem(L_TabContact()))
        {
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
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
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
    if (!g_scanned) return;

    ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x;
    const float sh = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    float mat[16] = {};
    g_dbgVmOk = RPM(g_gameBase + Off::MATRIX_RVA, mat);
    if (!g_dbgVmOk) {
        dl->AddText(ImVec2(20, 70), IM_COL32(255, 80, 80, 255), "Camera read failed");
        return;
    }

    g_dbgDrawn = 0;
    g_dbgCount = 0;

    float bestAimDist = 1e9f;
    float bestAimX = 0, bestAimY = 0;
    bool  hasAimTarget = false;

    for (auto& te : g_tracked) {
        Vec3 origin{};
        bool ok = RPM(te.obj + Off::V_OBJ_ORIGIN, origin.x) &&
                  RPM(te.obj + Off::V_OBJ_ORIGIN + 4, origin.y) &&
                  RPM(te.obj + Off::V_OBJ_ORIGIN + 8, origin.z);

        if (ok) {
            if (fabsf(origin.x) > 30000.0f || fabsf(origin.y) > 30000.0f || fabsf(origin.z) > 30000.0f)
                ok = false;
            if (fabsf(origin.x) < 0.05f && fabsf(origin.y) < 0.05f)
                ok = false;
        }

        if (ok) {
            te.pos = origin;
            te.life = kMaxLife;
            te.valid = true;
            int hp = 0, team = 0;
            RPM(te.obj + Off::OBJ_HP_OFF, hp);
            RPM(te.obj + Off::OBJ_TEAM_OFF, team);
            te.hp = hp;
            te.team = team;
        } else {
            if (te.life > 0) te.life--;
            if (te.life <= 0) te.valid = false;
        }

        if (!te.valid) continue;

        float dist = 0.0f;
        if (g_haveLocal) {
            float dx = te.pos.x - g_localPos.x;
            float dy = te.pos.y - g_localPos.y;
            float dz = te.pos.z - g_localPos.z;
            dist = sqrtf(dx*dx + dy*dy + dz*dz);
        }
        if (dist < 40.0f || dist > cfg.maxDistance) continue;

        ++g_dbgCount;

        Vec3 head = te.pos;
        head.z += cfg.headOffset;

        Vec2 feet{}, headScr{};
        if (!WorldToScreen(te.pos, mat, sw, sh, feet)) continue;
        if (!WorldToScreen(head, mat, sw, sh, headScr)) continue;

        float h = feet.y - headScr.y;
        if (h < 6.0f || h > 1800.0f) continue;
        float w = h * cfg.widthRatio;
        float bx = headScr.x - w * 0.5f;
        float by = headScr.y;

        float boxCx = bx + w * 0.5f;
        float boxCy = by + h * 0.5f;

        ImU32 col = C(cfg.enemyColor);

        if (cfg.drawBox)
            dl->AddRect(ImVec2(bx, by), ImVec2(bx + w, by + h), col, 0.0f, 0, cfg.thickness);
        if (cfg.drawCorners)
            DrawCorners(dl, bx, by, w, h, col, cfg.thickness + 0.5f);
        if (cfg.drawHealth && te.hp > 0)
            DrawHealthBar(dl, bx, by, w, h, (float)te.hp / cfg.healthMax, cfg.healthSide);
        if (cfg.drawDistance) {
            char buf[32];
            sprintf_s(buf, "%.0fm", dist);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(bx + w * 0.5f - ts.x * 0.5f, by + h + 2.0f), IM_COL32(255, 255, 255, 230), buf);
        }

        ++g_dbgDrawn;

        if (cfg.aimbot) {
            float adx = boxCx - sw * 0.5f;
            float ady = boxCy - sh * 0.5f;
            float ad = sqrtf(adx*adx + ady*ady);
            if (ad < bestAimDist && ad < cfg.aimFov) {
                bestAimDist = ad;
                bestAimX = boxCx;
                bestAimY = boxCy;
                hasAimTarget = true;
            }
        }
    }

    if (hasAimTarget) {
        dl->AddLine(ImVec2(bestAimX - 6, bestAimY), ImVec2(bestAimX + 6, bestAimY), IM_COL32(0, 255, 0, 200), 1.5f);
        dl->AddLine(ImVec2(bestAimX, bestAimY - 6), ImVec2(bestAimX, bestAimY + 6), IM_COL32(0, 255, 0, 200), 1.5f);
        RunTestAimbot(bestAimX, bestAimY, sw, sh);
    }
}

void RenderESP() {
    RenderESPMenu();
    RenderESPBoxes();
}
