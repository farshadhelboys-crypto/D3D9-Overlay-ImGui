// =====================================================================
//  farshad pm Esp  — ClientMod (CMLauncher)
//  Entity List path from offsets_cm.hpp dump
//  v3.0  |  EN / FA (FA strings fixed for LTR ImGui)
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

// ---------------------------------------------------------------------------
// Offsets from ClientMod dumper (offsets_cm.hpp)
// Module: clientmod_game.dll  (unless noted)
// ---------------------------------------------------------------------------
namespace Off {
    // Entity array
    constexpr uintptr_t ENTITY_ARRAY   = 0x25B9390; // pointer / base
    constexpr uintptr_t ENTITY_COUNT   = 0x25B939C; // max / count
    constexpr uintptr_t ENTITY_STRIDE  = 0x274;

    // Alternate list pointer (IClientEntityList*)
    constexpr uintptr_t CLIENT_ENTITY_LIST = 0x247B7D0;

    // Matrices
    constexpr uintptr_t VIEW_MATRIX     = 0x257B2C8;
    constexpr uintptr_t WORLD_TO_SCREEN = 0x257B250;
    constexpr uintptr_t MATRIX_BUFFER   = 0x257B280;
    constexpr uintptr_t VIEW_MATRIX_ALT = 0xFC7B00;

    // Netvars on entity object
    constexpr uintptr_t m_iHealth       = 0x11C;
    constexpr uintptr_t m_lifeState     = 0x120; // 0 = alive
    constexpr uintptr_t m_iTeamNum      = 0x1FC; // 2=T  3=CT
    constexpr uintptr_t m_vecOrigin     = 0x318;
    constexpr uintptr_t m_vecAbsOrigin  = 0x290;
    constexpr uintptr_t m_vecViewOffset = 0x334;
    constexpr uintptr_t m_bDormant      = 0x596;

    // Local helpers (still useful)
    constexpr uintptr_t MY_POS_X        = 0x242F510;
    constexpr uintptr_t MY_POS_Y        = 0x242F514;
    constexpr uintptr_t MY_POS_Z        = 0x242F518;
    constexpr uintptr_t MY_POS_PTR_RVA  = 0x00E1F664;
    constexpr uintptr_t MY_TEAM_PTR_RVA = 0x02E1616C;
    constexpr uintptr_t MY_TEAM_OFFSET  = 0x1D8;
}

enum class Lang { EN = 0, FA = 1 };
static Lang g_lang = Lang::EN;

// Reverse UTF-8 string so Persian displays correctly in LTR ImGui
static std::string FaFix(const char* s) {
    if (!s || !*s) return {};
    std::vector<std::string> chars;
    const unsigned char* p = (const unsigned char*)s;
    while (*p) {
        int n = 1;
        if ((*p & 0x80) == 0) n = 1;
        else if ((*p & 0xE0) == 0xC0) n = 2;
        else if ((*p & 0xF0) == 0xE0) n = 3;
        else if ((*p & 0xF8) == 0xF0) n = 4;
        chars.emplace_back(reinterpret_cast<const char*>(p), n);
        p += n;
    }
    std::string out;
    for (int i = (int)chars.size() - 1; i >= 0; --i)
        out += chars[i];
    return out;
}

struct Loc {
    const char* en;
    const char* fa; // stored logical (correct reading order); displayed reversed when FA
    std::string buf;
    const char* operator()() {
        if (g_lang != Lang::FA) return en;
        buf = FaFix(fa);
        return buf.c_str();
    }
};

static Loc L_Title      = { "farshad pm Esp",     "farshad pm Esp" };
static Loc L_Language   = { "Language",           "زبان" };
static Loc L_EnableESP  = { "Enable ESP",         "فعال کردن ای‌اس‌پی" };
static Loc L_Reattach   = { "Re-attach",          "اتصال مجدد" };
static Loc L_TeamCheck  = { "Team Check",         "چک تیم" };
static Loc L_MaxDist    = { "Max Distance",       "حداکثر فاصله" };
static Loc L_Box        = { "Box",                "باکس" };
static Loc L_Corners    = { "Corners",            "گوشه" };
static Loc L_HealthBar  = { "Health Bar",         "نوار جان" };
static Loc L_Distance   = { "Distance",           "فاصله" };
static Loc L_Thickness  = { "Thickness",          "ضخامت" };
static Loc L_BoxHeight  = { "Box Height",         "ارتفاع باکس" };
static Loc L_WidthRatio = { "Width Ratio",        "نسبت عرض" };
static Loc L_MaxHP      = { "Max HP",             "حداکثر جان" };
static Loc L_Side       = { "Side",               "سمت" };
static Loc L_AimEnable  = { "Enable Aim Assist",  "فعال کردن ایم اسیست" };
static Loc L_AimSmooth  = { "Smooth",             "نرمی" };
static Loc L_AimFov     = { "FOV",                "میدان دید" };
static Loc L_AimRMB     = { "Only while RMB",     "فقط با کلیک راست" };
static Loc L_Enemy      = { "Enemy",              "دشمن" };
static Loc L_Mate       = { "Mate",               "همتیمی" };
static Loc L_Telegram   = { "Telegram:",          "تلگرام:" };
static Loc L_OpenTG     = { "Open Telegram",      "باز کردن تلگرام" };
static Loc L_CopyTG     = { "Copy @client_v34",   "کپی شناسه" };

static Loc L_TabMain    = { "Main",     "اصلی" };
static Loc L_TabVisuals = { "Visuals",  "نمایش" };
static Loc L_TabAim     = { "Aim",      "ایم" };
static Loc L_TabColors  = { "Colors",   "رنگ" };
static Loc L_TabStatus  = { "Status",   "وضعیت" };
static Loc L_TabContact = { "Contact",  "ارتباط" };

static Loc L_Connected  = { "Connected",     "اتصال" };
static Loc L_LocalP     = { "Local Player",  "بازیکن خودی" };
static Loc L_Camera     = { "Camera",        "دوربین" };
static Loc L_Players    = { "Players",       "بازیکنان" };
static Loc L_Drawn      = { "Boxes drawn",   "باکس رسم شده" };
static Loc L_Yes        = { "Yes", "بله" };
static Loc L_No         = { "No",  "خیر" };
static Loc L_OK         = { "OK",  "موفق" };
static Loc L_Fail       = { "Fail","ناموفق" };
static Loc L_Found      = { "Found", "پیدا شد" };
static Loc L_NotFound   = { "Not found", "پیدا نشد" };

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };

struct EspConfig {
    bool  enabled      = true;
    bool  teamCheck    = true;
    bool  drawBox      = true;
    bool  drawCorners  = false;
    bool  drawHealth   = true;
    bool  drawDistance = true;
    bool  aimbot       = false;
    bool  aimOnlyRMB   = true;
    float aimSmooth    = 4.0f;
    float aimFov       = 220.0f;
    float headOffset   = 64.0f;   // matches typical view offset
    float widthRatio   = 0.45f;
    float thickness    = 1.8f;
    float maxDistance  = 2000.0f;
    float healthMax    = 100.0f;
    int   healthSide   = 0;
    ImVec4 enemyColor  = ImVec4(1.f, 0.25f, 0.25f, 1.f);
    ImVec4 mateColor   = ImVec4(0.25f, 0.85f, 1.f, 1.f);
} cfg;

static HANDLE    g_hProc = nullptr;
static uintptr_t g_gameBase = 0;
static DWORD     g_pid = 0;
static HWND      g_lastTarget = nullptr;
static Vec3      g_localPos{};
static bool      g_haveLocal = false;
static int       g_localTeam = 0;
static char      g_status[160] = "Not attached";
static int       g_count = 0, g_drawn = 0;
static bool      g_matOk = false;

template<typename T>
static bool RPM(uintptr_t a, T& o) {
    SIZE_T n = 0;
    return ReadProcessMemory(g_hProc, (LPCVOID)a, &o, sizeof(T), &n) && n == sizeof(T);
}
static bool ReadPtr32(uintptr_t a, uintptr_t& o) {
    uint32_t t = 0;
    if (!RPM(a, t) || t < 0x10000) return false;
    o = t;
    return true;
}

static uintptr_t GetModuleBase(DWORD pid, const wchar_t* name) {
    HANDLE s = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (s == INVALID_HANDLE_VALUE) return 0;
    MODULEENTRY32W me{ sizeof(me) };
    uintptr_t base = 0;
    if (Module32FirstW(s, &me)) {
        do {
            if (_wcsicmp(me.szModule, name) == 0) { base = (uintptr_t)me.modBaseAddr; break; }
        } while (Module32NextW(s, &me));
    }
    CloseHandle(s);
    return base;
}

static void Detach() {
    if (g_hProc) { CloseHandle(g_hProc); g_hProc = nullptr; }
    g_gameBase = 0; g_pid = 0;
    g_haveLocal = false; g_localTeam = 0;
    g_count = g_drawn = 0; g_matOk = false;
    strcpy_s(g_status, g_lang == Lang::FA ? FaFix("متصل نیست").c_str() : "Not attached");
}

static bool AttachFromUI() {
    Detach();
    if (!UI::IsWindowTargeted()) {
        strcpy_s(g_status, g_lang == Lang::FA ? FaFix("پنجره هدف نیست").c_str() : "No target window");
        return false;
    }
    HWND hwnd = UI::GetTargetWindow();
    DWORD pid = UI::GetTargetPID();
    if (!pid && hwnd) GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) {
        strcpy_s(g_status, "PID invalid");
        return false;
    }
    g_hProc = OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!g_hProc) {
        strcpy_s(g_status, g_lang == Lang::FA ? FaFix("خطا — ادمین اجرا کن").c_str() : "OpenProcess failed (admin)");
        return false;
    }
    g_gameBase = GetModuleBase(pid, L"clientmod_game.dll");
    if (!g_gameBase) {
        strcpy_s(g_status, g_lang == Lang::FA ? FaFix("ماژول لود نشده").c_str() : "clientmod_game.dll not loaded");
        CloseHandle(g_hProc); g_hProc = nullptr;
        return false;
    }
    g_pid = pid;
    sprintf_s(g_status, g_lang == Lang::FA ? "متصل" : "Attached");
    return true;
}

static void UpdateLocal() {
    g_haveLocal = false;
    g_localTeam = 0;
    if (!g_hProc || !g_gameBase) return;

    // CE pointer chain first
    uintptr_t posPtr = 0;
    if (ReadPtr32(g_gameBase + Off::MY_POS_PTR_RVA, posPtr)) {
        Vec3 p{};
        if (RPM(posPtr, p.x) && RPM(posPtr + 4, p.y) && RPM(posPtr + 8, p.z)) {
            if (fabsf(p.x) > 0.5f || fabsf(p.y) > 0.5f) {
                g_localPos = p;
                g_haveLocal = true;
            }
        }
    }
    if (!g_haveLocal) {
        Vec3 p{};
        if (RPM(g_gameBase + Off::MY_POS_X, p.x) &&
            RPM(g_gameBase + Off::MY_POS_Y, p.y) &&
            RPM(g_gameBase + Off::MY_POS_Z, p.z)) {
            if (fabsf(p.x) > 0.5f || fabsf(p.y) > 0.5f) {
                g_localPos = p;
                g_haveLocal = true;
            }
        }
    }

    uintptr_t teamBase = 0;
    if (ReadPtr32(g_gameBase + Off::MY_TEAM_PTR_RVA, teamBase)) {
        int t = 0;
        if (RPM(teamBase + Off::MY_TEAM_OFFSET, t))
            g_localTeam = t;
    }
}

static bool WorldToScreen(const Vec3& pos, const float* m, float sw, float sh, Vec2& out) {
    // row layout matching working dumps
    float cx = m[0]*pos.x + m[1]*pos.y + m[2]*pos.z + m[3];
    float cy = m[4]*pos.x + m[5]*pos.y + m[6]*pos.z + m[7];
    float cw = m[12]*pos.x + m[13]*pos.y + m[14]*pos.z + m[15];
    if (fabsf(cw) < 0.001f || cw < 0.1f) return false;
    float ndx = cx / cw, ndy = cy / cw;
    out.x = sw * 0.5f * (1.f + ndx);
    out.y = sh * 0.5f * (1.f - ndy);
    return true;
}

static bool ReadMatrix(float* out16) {
    // try several known matrix RVAs
    const uintptr_t cands[] = {
        Off::VIEW_MATRIX,
        Off::WORLD_TO_SCREEN,
        Off::MATRIX_BUFFER,
        Off::VIEW_MATRIX_ALT,
        0x27603D0, // previous working
    };
    for (uintptr_t rva : cands) {
        float m[16]{};
        uintptr_t addr = g_gameBase + rva;
        // some are pointers to matrix
        uintptr_t maybePtr = 0;
        if (ReadPtr32(addr, maybePtr) && maybePtr > 0x10000) {
            if (RPM(maybePtr, m)) {
                if (fabsf(m[15] - 1.f) < 0.5f || fabsf(m[0]) > 0.01f) {
                    memcpy(out16, m, 64);
                    return true;
                }
            }
        }
        if (RPM(addr, m)) {
            if (fabsf(m[15] - 1.f) < 0.5f || fabsf(m[0]) > 0.01f) {
                memcpy(out16, m, 64);
                return true;
            }
        }
    }
    return false;
}

static inline ImU32 Col(ImVec4 v) { return ImGui::ColorConvertFloat4ToU32(v); }

static void DrawCorners(ImDrawList* dl, float x, float y, float w, float h, ImU32 c, float t) {
    float l = (w < h ? w : h) * 0.28f; if (l < 4.f) l = 4.f;
    dl->AddLine(ImVec2(x,y), ImVec2(x+l,y), c, t);
    dl->AddLine(ImVec2(x,y), ImVec2(x,y+l), c, t);
    dl->AddLine(ImVec2(x+w,y), ImVec2(x+w-l,y), c, t);
    dl->AddLine(ImVec2(x+w,y), ImVec2(x+w,y+l), c, t);
    dl->AddLine(ImVec2(x,y+h), ImVec2(x+l,y+h), c, t);
    dl->AddLine(ImVec2(x,y+h), ImVec2(x,y+h-l), c, t);
    dl->AddLine(ImVec2(x+w,y+h), ImVec2(x+w-l,y+h), c, t);
    dl->AddLine(ImVec2(x+w,y+h), ImVec2(x+w,y+h-l), c, t);
}

static void DrawHealthBar(ImDrawList* dl, float bx, float by, float w, float h, float frac, int side) {
    frac = frac < 0 ? 0 : (frac > 1 ? 1 : frac);
    ImU32 hc = frac > 0.6f ? IM_COL32(60,220,60,255) : frac > 0.3f ? IM_COL32(240,220,40,255) : IM_COL32(240,60,60,255);
    ImU32 bg = IM_COL32(0,0,0,180);
    if (side <= 1) {
        float bw = 3.5f, gap = 4.f;
        float bx2 = side == 0 ? bx - bw - gap : bx + w + gap;
        dl->AddRectFilled(ImVec2(bx2, by), ImVec2(bx2 + bw, by + h), bg);
        float fill = h * frac;
        dl->AddRectFilled(ImVec2(bx2, by + h - fill), ImVec2(bx2 + bw, by + h), hc);
    } else {
        float bh = 3.5f, gap = 4.f;
        float by2 = side == 2 ? by - bh - gap : by + h + gap;
        dl->AddRectFilled(ImVec2(bx, by2), ImVec2(bx + w, by2 + bh), bg);
        dl->AddRectFilled(ImVec2(bx, by2), ImVec2(bx + w * frac, by2 + bh), hc);
    }
}

static void MoveMouseRel(int dx, int dy) {
    if (!dx && !dy) return;
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = dx; in.mi.dy = dy;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &in, sizeof(INPUT));
}

static void RunAim(float tx, float ty, float sw, float sh) {
    if (!cfg.aimbot) return;
    if (cfg.aimOnlyRMB && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) return;
    float dx = tx - sw * 0.5f, dy = ty - sh * 0.5f;
    float d = sqrtf(dx*dx + dy*dy);
    if (d < 1.5f || d > cfg.aimFov) return;
    float s = cfg.aimSmooth < 1.f ? 1.f : cfg.aimSmooth;
    float sx = dx / s, sy = dy / s;
    if (sx > 40) sx = 40; if (sx < -40) sx = -40;
    if (sy > 40) sy = 40; if (sy < -40) sy = -40;
    int mx = (int)(sx >= 0 ? sx + 0.5f : sx - 0.5f);
    int my = (int)(sy >= 0 ? sy + 0.5f : sy - 0.5f);
    if (!mx && fabsf(dx) > 2) mx = dx > 0 ? 1 : -1;
    if (!my && fabsf(dy) > 2) my = dy > 0 ? 1 : -1;
    MoveMouseRel(mx, my);
}

// rotating logo (same as before)
static int g_rotIdx = -1;
static void RotStart(ImDrawList* dl) { g_rotIdx = dl->VtxBuffer.Size; }
static void RotEnd(ImDrawList* dl, float rad, ImVec2 c) {
    float s = sinf(rad), co = cosf(rad);
    for (int i = g_rotIdx; i < dl->VtxBuffer.Size; i++) {
        auto& v = dl->VtxBuffer.Data[i];
        float px = v.pos.x - c.x, py = v.pos.y - c.y;
        v.pos.x = px * co - py * s + c.x;
        v.pos.y = px * s + py * co + c.y;
    }
}
static void DrawLogo() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    float px = 28, py = 28, r = 22;
    float a = (float)ImGui::GetTime() * 2.1f;
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
    const char* t = "F ESP";
    ImFont* f = ImGui::GetFont();
    RotStart(dl);
    ImVec2 ts = f->CalcTextSizeA(14.f, FLT_MAX, 0, t);
    ImVec2 tp(px - ts.x * 0.5f, py - ts.y * 0.5f);
    dl->AddText(f, 14.f, ImVec2(tp.x+1, tp.y+1), IM_COL32(0,0,0,200), t);
    dl->AddText(f, 14.f, tp, IM_COL32(100,220,255,255), t);
    RotEnd(dl, a, ImVec2(px, py));
}

// ===================== MENU =====================
void RenderESPMenu() {
    if (!UI::IsWindowTargeted()) return;

    ImGui::SetNextWindowSize(ImVec2(360, 440), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320, 280), ImVec2(520, 720));
    if (!ImGui::Begin(L_Title(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        ImGui::End(); return;
    }

    ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.f, 1.f), "%s", L_Title());
    ImGui::SameLine(); ImGui::TextDisabled("v3.0");
    ImGui::Separator();

    ImGui::Text("%s", L_Language());
    ImGui::SameLine();
    if (ImGui::RadioButton("EN", g_lang == Lang::EN)) g_lang = Lang::EN;
    ImGui::SameLine();
    if (ImGui::RadioButton("FA", g_lang == Lang::FA)) g_lang = Lang::FA;

    ImGui::Checkbox(L_EnableESP(), &cfg.enabled);
    ImGui::SameLine();
    if (ImGui::Button(L_Reattach())) AttachFromUI();
    ImGui::TextColored(ImVec4(0.5f, 1.f, 0.5f, 1.f), "%s", g_status);
    ImGui::Separator();

    if (ImGui::BeginTabBar("tabs", ImGuiTabBarFlags_FittingPolicyScroll)) {
        if (ImGui::BeginTabItem(L_TabMain())) {
            ImGui::Checkbox(L_TeamCheck(), &cfg.teamCheck);
            if (g_localTeam) ImGui::TextDisabled("Team: %d", g_localTeam);
            ImGui::SliderFloat(L_MaxDist(), &cfg.maxDistance, 50.f, 4000.f, "%.0f");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabVisuals())) {
            ImGui::Checkbox(L_Box(), &cfg.drawBox); ImGui::SameLine();
            ImGui::Checkbox(L_Corners(), &cfg.drawCorners);
            ImGui::Checkbox(L_HealthBar(), &cfg.drawHealth); ImGui::SameLine();
            ImGui::Checkbox(L_Distance(), &cfg.drawDistance);
            ImGui::Separator();
            ImGui::SliderFloat(L_Thickness(), &cfg.thickness, 0.5f, 6.f, "%.1f");
            ImGui::SliderFloat(L_BoxHeight(), &cfg.headOffset, 30.f, 120.f, "%.0f");
            ImGui::SliderFloat(L_WidthRatio(), &cfg.widthRatio, 0.15f, 0.9f, "%.2f");
            ImGui::SliderFloat(L_MaxHP(), &cfg.healthMax, 50.f, 200.f, "%.0f");
            const char* sides = g_lang == Lang::FA ? "چپ\0راست\0بالا\0پایین\0" : "Left\0Right\0Top\0Bottom\0";
            // FA combo items also need reverse — keep simple EN for combo values
            ImGui::Combo(L_Side(), &cfg.healthSide, "Left\0Right\0Top\0Bottom\0");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabAim())) {
            ImGui::Checkbox(L_AimEnable(), &cfg.aimbot);
            ImGui::Checkbox(L_AimRMB(), &cfg.aimOnlyRMB);
            ImGui::SliderFloat(L_AimSmooth(), &cfg.aimSmooth, 1.f, 15.f, "%.1f");
            ImGui::SliderFloat(L_AimFov(), &cfg.aimFov, 40.f, 600.f, "%.0f");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabColors())) {
            ImGui::ColorEdit4(L_Enemy(), (float*)&cfg.enemyColor);
            ImGui::ColorEdit4(L_Mate(), (float*)&cfg.mateColor);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabStatus())) {
            ImGui::Text("%s : %s", L_Connected(), g_hProc ? L_Yes() : L_No());
            ImGui::Text("%s : %s", L_LocalP(), g_haveLocal ? L_Found() : L_NotFound());
            ImGui::Text("%s : %s", L_Camera(), g_matOk ? L_OK() : L_Fail());
            ImGui::Text("%s : %d", L_Players(), g_count);
            ImGui::Text("%s : %d", L_Drawn(), g_drawn);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabContact())) {
            ImGui::Text("%s", L_Telegram());
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f, 0.7f, 1.f, 1.f), "@client_v34");
            if (ImGui::Button(L_OpenTG(), ImVec2(-1, 0)))
                ShellExecuteA(nullptr, "open", "https://t.me/client_v34", nullptr, nullptr, SW_SHOWNORMAL);
            if (ImGui::Button(L_CopyTG(), ImVec2(-1, 0))) {
                if (OpenClipboard(nullptr)) {
                    EmptyClipboard();
                    const char* t = "@client_v34";
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, strlen(t) + 1);
                    if (h) {
                        memcpy(GlobalLock(h), t, strlen(t) + 1);
                        GlobalUnlock(h);
                        SetClipboardData(CF_TEXT, h);
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

// ===================== DRAW =====================
void RenderESPBoxes() {
    if (UI::IsWindowTargeted()) DrawLogo();

    if (UI::GetTargetWindow() != g_lastTarget) {
        g_lastTarget = UI::GetTargetWindow();
        if (g_lastTarget) AttachFromUI(); else Detach();
    }
    if ((!g_hProc || !g_gameBase) && UI::IsWindowTargeted()) AttachFromUI();
    if (!g_hProc || !g_gameBase) return;

    UpdateLocal();
    if (!cfg.enabled) return;

    ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x, sh = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    float mat[16]{};
    g_matOk = ReadMatrix(mat);
    if (!g_matOk) {
        dl->AddText(ImVec2(20, 70), IM_COL32(255,80,80,255), "Matrix fail");
        return;
    }

    g_count = g_drawn = 0;
    float bestD = 1e9f, bestX = 0, bestY = 0;
    bool hasAim = false;

    // ---- Entity array path ----
    // ENTITY_ARRAY may be a pointer to first element or the array base itself
    uintptr_t arrayBase = 0;
    if (!ReadPtr32(g_gameBase + Off::ENTITY_ARRAY, arrayBase))
        arrayBase = g_gameBase + Off::ENTITY_ARRAY;

    int maxEnt = 64;
    int cnt = 0;
    if (RPM(g_gameBase + Off::ENTITY_COUNT, cnt) && cnt > 0 && cnt < 256)
        maxEnt = cnt;

    for (int i = 0; i < maxEnt; ++i) {
        uintptr_t slot = arrayBase + (uintptr_t)i * Off::ENTITY_STRIDE;

        // slot may be entity ptr or embedded entity
        uintptr_t ent = 0;
        if (!ReadPtr32(slot, ent) || ent < 0x10000)
            ent = slot; // try as embedded

        // dormant
        uint8_t dormant = 0;
        RPM(ent + Off::m_bDormant, dormant);
        if (dormant) continue;

        // alive
        uint8_t life = 0;
        RPM(ent + Off::m_lifeState, life);
        if (life != 0) continue;

        int hp = 0;
        RPM(ent + Off::m_iHealth, hp);
        if (hp <= 0 || hp > 500) continue;

        int team = 0;
        RPM(ent + Off::m_iTeamNum, team);

        // origin — prefer abs, fallback origin
        Vec3 origin{};
        bool gotPos = RPM(ent + Off::m_vecAbsOrigin, origin.x) &&
                      RPM(ent + Off::m_vecAbsOrigin + 4, origin.y) &&
                      RPM(ent + Off::m_vecAbsOrigin + 8, origin.z);
        if (!gotPos || (fabsf(origin.x) < 0.1f && fabsf(origin.y) < 0.1f)) {
            gotPos = RPM(ent + Off::m_vecOrigin, origin.x) &&
                     RPM(ent + Off::m_vecOrigin + 4, origin.y) &&
                     RPM(ent + Off::m_vecOrigin + 8, origin.z);
        }
        if (!gotPos) continue;
        if (fabsf(origin.x) > 50000.f || fabsf(origin.y) > 50000.f) continue;

        float dist = -1.f;
        if (g_haveLocal) {
            float dx = origin.x - g_localPos.x;
            float dy = origin.y - g_localPos.y;
            float dz = origin.z - g_localPos.z;
            dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist < 10.f) continue;           // self
            if (dist > cfg.maxDistance) continue;
        }

        if (cfg.teamCheck && g_localTeam != 0 && team == g_localTeam)
            continue;

        ++g_count;

        // head height from view offset if available
        float headZ = cfg.headOffset;
        float vo = 0.f;
        if (RPM(ent + Off::m_vecViewOffset + 8, vo) && vo > 20.f && vo < 100.f)
            headZ = vo;

        Vec3 head = origin;
        head.z += headZ;

        Vec2 feet{}, headS{};
        if (!WorldToScreen(origin, mat, sw, sh, feet)) continue;
        if (!WorldToScreen(head, mat, sw, sh, headS)) continue;

        float h = feet.y - headS.y;
        if (h < 6.f || h > 2000.f) continue;
        float w = h * cfg.widthRatio;
        if (w < 2.f) w = 2.f;
        float bx = headS.x - w * 0.5f;
        float by = headS.y;

        ImU32 col = (g_localTeam && team == g_localTeam) ? Col(cfg.mateColor) : Col(cfg.enemyColor);

        if (cfg.drawBox)
            dl->AddRect(ImVec2(bx, by), ImVec2(bx + w, by + h), col, 0.f, 0, cfg.thickness);
        if (cfg.drawCorners)
            DrawCorners(dl, bx, by, w, h, col, cfg.thickness + 0.5f);
        if (cfg.drawHealth)
            DrawHealthBar(dl, bx, by, w, h, (float)hp / cfg.healthMax, cfg.healthSide);
        if (cfg.drawDistance && dist >= 0.f) {
            char buf[24];
            sprintf_s(buf, "%.0fm", dist);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(bx + w * 0.5f - ts.x * 0.5f, by + h + 2), IM_COL32(255,255,255,230), buf);
        }

        ++g_drawn;

        if (cfg.aimbot) {
            float ax = headS.x;
            float ay = headS.y + h * 0.25f;
            float adx = ax - sw * 0.5f, ady = ay - sh * 0.5f;
            float ad = sqrtf(adx*adx + ady*ady);
            if (ad < bestD && ad <= cfg.aimFov) {
                bestD = ad; bestX = ax; bestY = ay; hasAim = true;
            }
        }
    }

    if (cfg.aimbot)
        dl->AddCircle(ImVec2(sw * 0.5f, sh * 0.5f), cfg.aimFov, IM_COL32(255,255,255,35), 64, 1.f);

    if (hasAim) {
        dl->AddLine(ImVec2(bestX - 8, bestY), ImVec2(bestX + 8, bestY), IM_COL32(0,255,0,220), 1.5f);
        dl->AddLine(ImVec2(bestX, bestY - 8), ImVec2(bestX, bestY + 8), IM_COL32(0,255,0,220), 1.5f);
        RunAim(bestX, bestY, sw, sh);
    }
}

void RenderESP() {
    RenderESPMenu();
    RenderESPBoxes();
}
