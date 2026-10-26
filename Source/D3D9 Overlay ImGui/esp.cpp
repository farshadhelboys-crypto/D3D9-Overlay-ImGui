// =====================================================================
//  ESP v2.5 — English UI
//  Offsets are NOT hardcoded for production use: loaded from GitHub
//  https://raw.githubusercontent.com/farshadhelboys-crypto/D3D9-Overlay-ImGui/main/offsets/offsets.json
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

// ---------- Remote offsets (filled at runtime) ----------
struct Offsets {
    uintptr_t MY_POS_PTR_RVA  = 0;
    uintptr_t MY_TEAM_PTR_RVA = 0;
    uintptr_t MY_TEAM_OFFSET  = 0;
    uintptr_t HP_PTR_RVA      = 0;
    uintptr_t HP_OFFSET1      = 0;
    uintptr_t HP_OFFSET2      = 0;
    uintptr_t MY_POS_X        = 0;
    uintptr_t MY_POS_Y        = 0;
    uintptr_t MY_POS_Z        = 0;
    uintptr_t MATRIX_RVA      = 0;
    uintptr_t VTABLE_1        = 0;
    uintptr_t VTABLE_2        = 0;
    uintptr_t V_OBJ_ORIGIN    = 0;
    uintptr_t OBJ_HP_OFF      = 0;
    uintptr_t OBJ_TEAM_OFF    = 0;
    bool loaded = false;
    char status[96] = "Offsets: not loaded";
} Off;

static const char* OFFSETS_HOST = "raw.githubusercontent.com";
static const char* OFFSETS_PATH =
    "/farshadhelboys-crypto/D3D9-Overlay-ImGui/main/offsets/offsets.json";

static std::string HttpGetHttps(const wchar_t* host, const wchar_t* path) {
    std::string result;
    HINTERNET hSession = WinHttpOpen(L"FarshadESP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return {};
    HINTERNET hConnect = WinHttpConnect(hSession, host, INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return {}; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path, nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return {}; }
    BOOL ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);
    if (ok) {
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (!dwSize) break;
            std::vector<char> buf(dwSize + 1, 0);
            DWORD dwRead = 0;
            WinHttpReadData(hRequest, buf.data(), dwSize, &dwRead);
            result.append(buf.data(), dwRead);
        } while (dwSize > 0);
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

static uintptr_t JsonHex(const std::string& j, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return 0;
    p = j.find(':', p);
    if (p == std::string::npos) return 0;
    // skip to value
    while (p < j.size() && (j[p] == ':' || j[p] == ' ' || j[p] == '\"')) p++;
    // read until quote or comma
    size_t e = p;
    while (e < j.size() && j[e] != '\"' && j[e] != ',' && j[e] != '}') e++;
    std::string v = j.substr(p, e - p);
    // trim
    while (!v.empty() && (v.back() == ' ' || v.back() == '\"')) v.pop_back();
    if (v.empty()) return 0;
    return (uintptr_t)strtoull(v.c_str(), nullptr, 0);
}

static bool LoadOffsetsFromGitHub() {
    std::wstring host = L"raw.githubusercontent.com";
    std::wstring path = L"/farshadhelboys-crypto/D3D9-Overlay-ImGui/main/offsets/offsets.json";
    std::string j = HttpGetHttps(host.c_str(), path.c_str());
    if (j.empty() || j.find("matrix_rva") == std::string::npos) {
        strcpy_s(Off.status, "Offsets: download failed");
        Off.loaded = false;
        return false;
    }
    Off.MY_POS_PTR_RVA  = JsonHex(j, "my_pos_ptr_rva");
    Off.MY_TEAM_PTR_RVA = JsonHex(j, "my_team_ptr_rva");
    Off.MY_TEAM_OFFSET  = JsonHex(j, "my_team_offset");
    Off.HP_PTR_RVA      = JsonHex(j, "hp_ptr_rva");
    Off.HP_OFFSET1      = JsonHex(j, "hp_offset1");
    Off.HP_OFFSET2      = JsonHex(j, "hp_offset2");
    Off.MY_POS_X        = JsonHex(j, "my_pos_x");
    Off.MY_POS_Y        = JsonHex(j, "my_pos_y");
    Off.MY_POS_Z        = JsonHex(j, "my_pos_z");
    Off.MATRIX_RVA      = JsonHex(j, "matrix_rva");
    Off.VTABLE_1        = JsonHex(j, "vtable_1");
    Off.VTABLE_2        = JsonHex(j, "vtable_2");
    Off.V_OBJ_ORIGIN    = JsonHex(j, "v_obj_origin");
    Off.OBJ_HP_OFF      = JsonHex(j, "obj_hp_off");
    Off.OBJ_TEAM_OFF    = JsonHex(j, "obj_team_off");

    if (!Off.MATRIX_RVA || !Off.VTABLE_1 || !Off.V_OBJ_ORIGIN) {
        strcpy_s(Off.status, "Offsets: incomplete JSON");
        Off.loaded = false;
        return false;
    }
    Off.loaded = true;
    strcpy_s(Off.status, "Offsets: loaded from GitHub");
    return true;
}

// ---------- UI strings (English only) ----------
struct Loc {
    const char* en;
    const char* operator()() const { return en; }
};
static const Loc L_Title = { "farshad pm Esp" };
static const Loc L_EnableESP = { "Enable ESP" };
static const Loc L_Reattach = { "Re-attach / Rescan" };
static const Loc L_TeamCheck = { "Team Check" };
static const Loc L_MaxDist = { "Max Distance" };
static const Loc L_Box = { "Box" };
static const Loc L_Corners = { "Corners" };
static const Loc L_HealthBar = { "Health Bar" };
static const Loc L_Distance = { "Distance" };
static const Loc L_Thickness = { "Thickness" };
static const Loc L_BoxHeight = { "Box Height" };
static const Loc L_WidthRatio = { "Width Ratio" };
static const Loc L_MaxHP = { "Max HP" };
static const Loc L_Side = { "Side" };
static const Loc L_AimEnable = { "Enable Aim Assist" };
static const Loc L_AimSmooth = { "Smooth" };
static const Loc L_AimFov = { "FOV (px)" };
static const Loc L_AimHint = { "Hold RMB or always-on; aims at upper body" };
static const Loc L_Enemy = { "Enemy" };
static const Loc L_Mate = { "Mate" };
static const Loc L_Telegram = { "Telegram:" };
static const Loc L_OpenTG = { "Open Telegram" };
static const Loc L_CopyTG = { "Copy @client_v34" };
static const Loc L_TabMain = { "Main" };
static const Loc L_TabVisuals = { "Visuals" };
static const Loc L_TabAimbot = { "Aimbot" };
static const Loc L_TabColors = { "Colors" };
static const Loc L_TabDebug = { "Status" };
static const Loc L_TabContact = { "Contact" };
static const Loc L_Connected = { "Connected" };
static const Loc L_LocalPlayer = { "Local Player" };
static const Loc L_Camera = { "Camera" };
static const Loc L_Players = { "Players" };
static const Loc L_BoxesDrawn = { "Boxes drawn" };
static const Loc L_Yes = { "Yes" };
static const Loc L_No = { "No" };
static const Loc L_Found = { "Found" };
static const Loc L_NotFound = { "Not found" };
static const Loc L_OK = { "OK" };
static const Loc L_Failed = { "Failed" };

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };
struct TrackedEntity {
    uintptr_t obj = 0;
    Vec3 pos = {0,0,0};
    int hp = 0, team = 0, life = 0;
    bool valid = false;
};

struct EspConfig {
    bool  enabled = true, teamCheck = false, drawBox = true, drawCorners = false;
    bool  drawHealth = false, drawDistance = true, aimbot = false, aimOnlyRMB = true;
    float aimSmooth = 4.0f, aimFov = 220.0f, headOffset = 72.0f, widthRatio = 0.42f;
    float thickness = 1.8f, maxDistance = 2000.0f, healthMax = 100.0f;
    int   healthSide = 0;
    ImVec4 enemyColor = ImVec4(1.f, 0.25f, 0.25f, 1.f);
    ImVec4 mateColor  = ImVec4(0.25f, 0.85f, 1.f, 1.f);
} cfg;

static HANDLE g_hProc = nullptr;
static uintptr_t g_gameBase = 0;
static DWORD g_attachedPid = 0;
static HWND g_lastTarget = nullptr;
static Vec3 g_localPos = {0,0,0};
static bool g_haveLocal = false;
static int g_localTeam = 0, g_localHP = 0;
static char g_status[160] = "Not attached";
static int g_dbgCount = 0, g_dbgDrawn = 0;
static bool g_dbgVmOk = false;
static std::vector<uintptr_t> g_rawObjects;
static std::vector<TrackedEntity> g_tracked;
static bool g_scanned = false;
static bool g_offsetsTried = false;
static const int kMaxLife = 8;

template<typename T>
static inline bool RPM(uintptr_t addr, T& out) {
    SIZE_T read = 0;
    return ReadProcessMemory(g_hProc, (LPCVOID)addr, &out, sizeof(T), &read) && read == sizeof(T);
}
static inline bool ReadPtr32(uintptr_t addr, uintptr_t& out) {
    uint32_t tmp = 0;
    if (!RPM(addr, tmp)) return false;
    out = (uintptr_t)tmp;
    return out > 0x10000;
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
static void EnsureOffsets() {
    if (Off.loaded) return;
    if (g_offsetsTried) return;
    g_offsetsTried = true;
    LoadOffsetsFromGitHub();
}
static void Detach() {
    if (g_hProc) { CloseHandle(g_hProc); g_hProc = nullptr; }
    g_gameBase = 0; g_attachedPid = 0;
    g_haveLocal = false; g_localTeam = 0; g_localHP = 0;
    g_dbgCount = 0; g_dbgDrawn = 0; g_dbgVmOk = false;
    g_rawObjects.clear(); g_tracked.clear(); g_scanned = false;
    strcpy_s(g_status, "Not attached");
}
static void ScanVTableObjects() {
    g_rawObjects.clear(); g_tracked.clear(); g_scanned = false;
    if (!g_hProc || !g_gameBase || !Off.loaded) return;
    const uint32_t targets[2] = {
        (uint32_t)(g_gameBase + Off.VTABLE_1),
        (uint32_t)(g_gameBase + Off.VTABLE_2)
    };
    const uintptr_t start = 0x10000000, end = 0x40000000;
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
        TrackedEntity te; te.obj = o; g_tracked.push_back(te);
    }
    g_scanned = true;
    sprintf_s(g_status, "Attached | Players found: %zu", g_rawObjects.size());
}
static bool AttachFromUI() {
    Detach();
    EnsureOffsets();
    if (!Off.loaded) {
        strcpy_s(g_status, Off.status);
        return false;
    }
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
    g_haveLocal = false; g_localTeam = 0; g_localHP = 0;
    if (!g_hProc || !g_gameBase || !Off.loaded) return;
    uintptr_t posPtr = 0;
    if (Off.MY_POS_PTR_RVA && ReadPtr32(g_gameBase + Off.MY_POS_PTR_RVA, posPtr)) {
        Vec3 pos{};
        if (RPM(posPtr + 0x0, pos.x) && RPM(posPtr + 0x4, pos.y) && RPM(posPtr + 0x8, pos.z)) {
            if (fabsf(pos.x) > 0.5f || fabsf(pos.y) > 0.5f || fabsf(pos.z) > 0.5f) {
                g_localPos = pos; g_haveLocal = true;
            }
        }
    }
    if (!g_haveLocal && Off.MY_POS_X) {
        Vec3 pos{};
        if (RPM(g_gameBase + Off.MY_POS_X, pos.x) &&
            RPM(g_gameBase + Off.MY_POS_Y, pos.y) &&
            RPM(g_gameBase + Off.MY_POS_Z, pos.z)) {
            if (fabsf(pos.x) > 0.5f || fabsf(pos.y) > 0.5f || fabsf(pos.z) > 0.5f) {
                g_localPos = pos; g_haveLocal = true;
            }
        }
    }
    uintptr_t teamBase = 0;
    if (Off.MY_TEAM_PTR_RVA && ReadPtr32(g_gameBase + Off.MY_TEAM_PTR_RVA, teamBase)) {
        int team = 0;
        if (RPM(teamBase + Off.MY_TEAM_OFFSET, team)) g_localTeam = team;
    }
    uintptr_t hp1 = 0, hp2 = 0;
    if (Off.HP_PTR_RVA && ReadPtr32(g_gameBase + Off.HP_PTR_RVA, hp1) &&
        ReadPtr32(hp1 + Off.HP_OFFSET1, hp2)) {
        int hp = 0;
        if (RPM(hp2 + Off.HP_OFFSET2, hp)) g_localHP = hp;
    }
}
static bool WorldToScreen(const Vec3& pos, const float* mat, float sw, float sh, Vec2& out) {
    float cx = mat[0]*pos.x + mat[1]*pos.y + mat[2]*pos.z + mat[3];
    float cy = mat[4]*pos.x + mat[5]*pos.y + mat[6]*pos.z + mat[7];
    float cw = mat[12]*pos.x + mat[13]*pos.y + mat[14]*pos.z + mat[15];
    if (fabsf(cw) < 0.001f || cw < 0.15f) return false;
    float ndc_x = cx / cw, ndc_y = cy / cw;
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
    if (frac < 0) frac = 0; if (frac > 1) frac = 1;
    ImU32 hc = frac > 0.6f ? IM_COL32(60,220,60,255) : frac > 0.3f ? IM_COL32(240,220,40,255) : IM_COL32(240,60,60,255);
    ImU32 bg = IM_COL32(0,0,0,180);
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
static int g_RotateStartIdx = -1;
static void ImRotateStart(ImDrawList* dl) { g_RotateStartIdx = dl->VtxBuffer.Size; }
static void ImRotateEnd(ImDrawList* dl, float rad, ImVec2 center) {
    float s = sinf(rad), c = cosf(rad);
    for (int i = g_RotateStartIdx; i < dl->VtxBuffer.Size; i++) {
        ImDrawVert& v = dl->VtxBuffer.Data[i];
        float px = v.pos.x - center.x, py = v.pos.y - center.y;
        v.pos.x = px * c - py * s + center.x;
        v.pos.y = px * s + py * c + center.y;
    }
}
static void DrawRotatingLogo() {
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;
    const float posX = 28.f, posY = 28.f, radius = 22.f;
    const float angle = (float)ImGui::GetTime() * 2.1f;
    dl->AddCircleFilled(ImVec2(posX, posY), radius + 4.f, IM_COL32(0,0,0,120), 32);
    for (int i = 0; i < 48; ++i) {
        float a0 = angle + (float)i / 48 * 2.f * (float)M_PI;
        float a1 = angle + (float)(i + 1) / 48 * 2.f * (float)M_PI;
        float alpha = 80.f + 120.f * (0.5f + 0.5f * sinf(a0 * 2.f));
        dl->AddLine(ImVec2(posX + cosf(a0)*radius, posY + sinf(a0)*radius),
                    ImVec2(posX + cosf(a1)*radius, posY + sinf(a1)*radius),
                    IM_COL32(80,200,255,(int)alpha), 2.f);
    }
    const char* text = "F ESP";
    ImFont* font = ImGui::GetFont();
    ImRotateStart(dl);
    ImVec2 ts = font->CalcTextSizeA(14.f, FLT_MAX, 0.f, text);
    ImVec2 tp(posX - ts.x*0.5f, posY - ts.y*0.5f);
    dl->AddText(font, 14.f, ImVec2(tp.x+1, tp.y+1), IM_COL32(0,0,0,200), text);
    dl->AddText(font, 14.f, tp, IM_COL32(100,220,255,255), text);
    ImRotateEnd(dl, angle, ImVec2(posX, posY));
}
static void MoveMouseRel(int dx, int dy) {
    if (!dx && !dy) return;
    INPUT in{}; in.type = INPUT_MOUSE; in.mi.dx = dx; in.mi.dy = dy; in.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &in, sizeof(INPUT));
}
static void RunAimAssist(float targetX, float targetY, float sw, float sh) {
    if (!cfg.aimbot) return;
    if (cfg.aimOnlyRMB && !(GetAsyncKeyState(VK_RBUTTON) & 0x8000)) return;
    float cx = sw*0.5f, cy = sh*0.5f;
    float dx = targetX - cx, dy = targetY - cy;
    float dist = sqrtf(dx*dx + dy*dy);
    if (dist < 1.5f || dist > cfg.aimFov) return;
    float smooth = cfg.aimSmooth < 1.f ? 1.f : cfg.aimSmooth;
    float stepX = dx / smooth, stepY = dy / smooth;
    const float maxStep = 40.f;
    if (stepX > maxStep) stepX = maxStep; if (stepX < -maxStep) stepX = -maxStep;
    if (stepY > maxStep) stepY = maxStep; if (stepY < -maxStep) stepY = -maxStep;
    int mx = (int)(stepX >= 0 ? stepX + 0.5f : stepX - 0.5f);
    int my = (int)(stepY >= 0 ? stepY + 0.5f : stepY - 0.5f);
    if (!mx && fabsf(dx) > 2.f) mx = dx > 0 ? 1 : -1;
    if (!my && fabsf(dy) > 2.f) my = dy > 0 ? 1 : -1;
    MoveMouseRel(mx, my);
}

void RenderESPMenu() {
    if (!UI::IsWindowTargeted()) return;
    EnsureOffsets();
    ImGui::SetNextWindowSize(ImVec2(360, 440), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320, 280), ImVec2(500, 700));
    if (!ImGui::Begin(L_Title(), nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
        ImGui::End(); return;
    }
    ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.f, 1.f), "%s", L_Title());
    ImGui::SameLine(); ImGui::TextDisabled("remote-off");
    ImGui::Separator();
    ImGui::Checkbox(L_EnableESP(), &cfg.enabled);
    ImGui::SameLine();
    if (ImGui::Button(L_Reattach())) AttachFromUI();
    if (ImGui::Button("Reload offsets")) { g_offsetsTried = false; Off.loaded = false; EnsureOffsets(); }
    ImGui::TextColored(ImVec4(0.5f,1.f,0.5f,1.f), "%s", g_status);
    ImGui::TextDisabled("%s", Off.status);
    ImGui::Separator();
    if (ImGui::BeginTabBar("EspTabs", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_Reorderable)) {
        if (ImGui::BeginTabItem(L_TabMain())) {
            ImGui::Checkbox(L_TeamCheck(), &cfg.teamCheck);
            if (g_localTeam) ImGui::TextDisabled("My team: %d", g_localTeam);
            ImGui::SliderFloat(L_MaxDist(), &cfg.maxDistance, 50.f, 4000.f, "%.0f m");
            ImGui::TextDisabled("Boxes farther than this are not drawn");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabVisuals())) {
            ImGui::Checkbox(L_Box(), &cfg.drawBox); ImGui::SameLine();
            ImGui::Checkbox(L_Corners(), &cfg.drawCorners);
            ImGui::Checkbox(L_HealthBar(), &cfg.drawHealth); ImGui::SameLine();
            ImGui::Checkbox(L_Distance(), &cfg.drawDistance);
            ImGui::SliderFloat(L_Thickness(), &cfg.thickness, 0.5f, 6.f, "%.1f");
            ImGui::SliderFloat(L_BoxHeight(), &cfg.headOffset, 30.f, 140.f, "%.0f");
            ImGui::SliderFloat(L_WidthRatio(), &cfg.widthRatio, 0.15f, 0.9f, "%.2f");
            ImGui::SliderFloat(L_MaxHP(), &cfg.healthMax, 50.f, 500.f, "%.0f");
            const char* sides = "Left\0Right\0Top\0Bottom\0";
            ImGui::Combo(L_Side(), &cfg.healthSide, sides);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabAimbot())) {
            ImGui::Checkbox(L_AimEnable(), &cfg.aimbot);
            ImGui::Checkbox("Only while RMB held", &cfg.aimOnlyRMB);
            ImGui::SliderFloat(L_AimSmooth(), &cfg.aimSmooth, 1.f, 15.f, "%.1f");
            ImGui::SliderFloat(L_AimFov(), &cfg.aimFov, 40.f, 600.f, "%.0f");
            ImGui::TextDisabled("%s", L_AimHint());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabColors())) {
            ImGui::ColorEdit4(L_Enemy(), (float*)&cfg.enemyColor);
            ImGui::ColorEdit4(L_Mate(), (float*)&cfg.mateColor);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabDebug())) {
            ImGui::Text("%s : %s", L_Connected(), g_hProc ? L_Yes() : L_No());
            ImGui::Text("%s : %s", L_LocalPlayer(), g_haveLocal ? L_Found() : L_NotFound());
            ImGui::Text("%s : %s", L_Camera(), g_dbgVmOk ? L_OK() : L_Failed());
            ImGui::Text("%s : %d", L_Players(), g_dbgCount);
            ImGui::Text("%s : %d", L_BoxesDrawn(), g_dbgDrawn);
            ImGui::TextDisabled("%s", Off.status);
            // Never print raw hex offsets in UI
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(L_TabContact())) {
            ImGui::Text("%s", L_Telegram());
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f,0.7f,1.f,1.f), "@client_v34");
            if (ImGui::Button(L_OpenTG(), ImVec2(-1,0)))
                ShellExecuteA(nullptr, "open", "https://t.me/client_v34", nullptr, nullptr, SW_SHOWNORMAL);
            if (ImGui::Button(L_CopyTG(), ImVec2(-1,0))) {
                if (OpenClipboard(nullptr)) {
                    EmptyClipboard();
                    const char* text = "@client_v34";
                    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, strlen(text)+1);
                    if (hMem) {
                        memcpy(GlobalLock(hMem), text, strlen(text)+1);
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
    EnsureOffsets();
    if (!Off.loaded) return;

    if (UI::GetTargetWindow() != g_lastTarget) {
        g_lastTarget = UI::GetTargetWindow();
        if (g_lastTarget) AttachFromUI(); else Detach();
    }
    if ((!g_hProc || !g_gameBase) && UI::IsWindowTargeted()) AttachFromUI();
    if (!g_hProc || !g_gameBase) return;
    UpdateLocal();
    if (!cfg.enabled || !g_scanned) return;

    ImGuiIO& io = ImGui::GetIO();
    const float sw = io.DisplaySize.x, sh = io.DisplaySize.y;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    float mat[16] = {};
    g_dbgVmOk = RPM(g_gameBase + Off.MATRIX_RVA, mat);
    if (!g_dbgVmOk) {
        dl->AddText(ImVec2(20, 70), IM_COL32(255,80,80,255), "Camera read failed");
        return;
    }

    g_dbgDrawn = 0; g_dbgCount = 0;
    float bestAimDist = 1e9f, bestAimX = 0, bestAimY = 0;
    bool hasAimTarget = false;

    for (auto& te : g_tracked) {
        Vec3 origin{};
        bool ok = RPM(te.obj + Off.V_OBJ_ORIGIN, origin.x) &&
                  RPM(te.obj + Off.V_OBJ_ORIGIN + 4, origin.y) &&
                  RPM(te.obj + Off.V_OBJ_ORIGIN + 8, origin.z);
        if (ok) {
            if (fabsf(origin.x) > 30000.f || fabsf(origin.y) > 30000.f || fabsf(origin.z) > 30000.f) ok = false;
            if (fabsf(origin.x) < 0.05f && fabsf(origin.y) < 0.05f) ok = false;
        }
        if (ok) {
            te.pos = origin; te.life = kMaxLife; te.valid = true;
            int hp = 0, team = 0;
            RPM(te.obj + Off.OBJ_HP_OFF, hp);
            RPM(te.obj + Off.OBJ_TEAM_OFF, team);
            te.hp = hp; te.team = team;
        } else {
            if (te.life > 0) te.life--;
            if (te.life <= 0) te.valid = false;
        }
        if (!te.valid) continue;
        if (cfg.teamCheck && g_localTeam && te.team == g_localTeam) continue;

        float dist = -1.f;
        if (g_haveLocal) {
            float dx = te.pos.x - g_localPos.x, dy = te.pos.y - g_localPos.y, dz = te.pos.z - g_localPos.z;
            dist = sqrtf(dx*dx + dy*dy + dz*dz);
            if (dist < 15.f || dist > cfg.maxDistance) continue;
        }
        ++g_dbgCount;

        Vec3 head = te.pos; head.z += cfg.headOffset;
        Vec2 feetScr{}, headScr{};
        if (!WorldToScreen(te.pos, mat, sw, sh, feetScr)) continue;
        if (!WorldToScreen(head, mat, sw, sh, headScr)) continue;
        float h = feetScr.y - headScr.y;
        if (h < 6.f || h > 2000.f) continue;
        float w = h * cfg.widthRatio; if (w < 2.f) w = 2.f;
        float bx = headScr.x - w * 0.5f, by = headScr.y;
        float aimX = headScr.x, aimY = headScr.y + h * 0.25f;
        ImU32 col = C(cfg.enemyColor);
        if (cfg.drawBox) dl->AddRect(ImVec2(bx, by), ImVec2(bx + w, by + h), col, 0.f, 0, cfg.thickness);
        if (cfg.drawCorners) DrawCorners(dl, bx, by, w, h, col, cfg.thickness + 0.5f);
        if (cfg.drawHealth && te.hp > 0) DrawHealthBar(dl, bx, by, w, h, (float)te.hp / cfg.healthMax, cfg.healthSide);
        if (cfg.drawDistance && dist >= 0.f) {
            char buf[32]; sprintf_s(buf, "%.0fm", dist);
            ImVec2 ts = ImGui::CalcTextSize(buf);
            dl->AddText(ImVec2(bx + w*0.5f - ts.x*0.5f, by + h + 2.f), IM_COL32(255,255,255,230), buf);
        }
        ++g_dbgDrawn;
        if (cfg.aimbot) {
            float adx = aimX - sw*0.5f, ady = aimY - sh*0.5f;
            float ad = sqrtf(adx*adx + ady*ady);
            if (ad < bestAimDist && ad <= cfg.aimFov) {
                bestAimDist = ad; bestAimX = aimX; bestAimY = aimY; hasAimTarget = true;
            }
        }
    }
    if (cfg.aimbot)
        dl->AddCircle(ImVec2(sw*0.5f, sh*0.5f), cfg.aimFov, IM_COL32(255,255,255,40), 64, 1.f);
    if (hasAimTarget) {
        dl->AddLine(ImVec2(bestAimX-8, bestAimY), ImVec2(bestAimX+8, bestAimY), IM_COL32(0,255,0,220), 1.5f);
        dl->AddLine(ImVec2(bestAimX, bestAimY-8), ImVec2(bestAimX, bestAimY+8), IM_COL32(0,255,0,220), 1.5f);
        RunAimAssist(bestAimX, bestAimY, sw, sh);
    }
}

void RenderESP() {
    RenderESPMenu();
    RenderESPBoxes();
}
