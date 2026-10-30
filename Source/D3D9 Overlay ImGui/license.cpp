#include "license.h"
#include "ImGui/imgui.h"
#include <Windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "winhttp.lib")

namespace License {
namespace {

std::string g_key;
std::string g_hwid;
bool g_licensed = false;
Status g_last;
char g_inputKey[64] =nchar g_inputTg[32] = {};
std::string g_uiMsg;
int g_daysLeft = -1;

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

// Single HTTP POST — one soft retry only if response empty
std::string HttpPost(const std::string& path, const std::string& jsonBody, bool allowRetry = true) {
    std::string result;
    std::string base = API_BASE;
    bool https = true;
    size_t pos = 0;
    if (base.rfind("https://", 0) == 0) { pos = 8; https = true; }
    else if (base.rfind("http://", 0) == 0) { pos = 7; https = false; }
    size_t slash = base.find('/', pos);
    std::string hostA = slash == std::string::npos ? base.substr(pos) : base.substr(pos, slash - pos);
    std::string pathA = (slash == std::string::npos ? "" : base.substr(slash)) + path;
    std::wstring host = Utf8ToWide(hostA);
    std::wstring urlPath = Utf8ToWide(pathA);

    HINTERNET hSession = WinHttpOpen(L"FarshadESP/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return {};
    WinHttpSetTimeouts(hSession, 5000, 5000, 8000, 8000);

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return {}; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    std::wstring headers = L"Content-Type: application/json\r\n";
    BOOL ok = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1L,
        (LPVOID)jsonBody.data(), (DWORD)jsonBody.size(), (DWORD)jsonBody.size(), 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);
    if (ok) {
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            WinHttpQueryDataAvailable(hRequest, &dwSize);
            if (!dwSize) break;
            std::vector<char> buf(dwSize + 1, 0);
            DWORD dwRead = 0;
            if (!WinHttpReadData(hRequest, buf.data(), dwSize, &dwRead)) break;
            result.append(buf.data(), dwRead);
        } while (dwSize > 0);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (result.empty() && allowRetry)
        return HttpPost(path, jsonBody, false);
    return result;
}

std::string JsonStr(const std::string& j, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return {};
    p = j.find(':', p + k.size());
    if (p == std::string::npos) return {};
    p = j.find('\"', p + 1);
    if (p == std::string::npos) return {};
    size_t e = j.find('\"', p + 1);
    if (e == std::string::npos) return {};
    return j.substr(p + 1, e - p - 1);
}

bool JsonBool(const std::string& j, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return false;
    p = j.find(':', p + k.size());
    if (p == std::string::npos) return false;
    while (p + 1 < j.size() && (j[p + 1] == ' ' || j[p + 1] == '\t')) ++p;
    return j.compare(p + 1, 4, "true") == 0;
}

long long JsonNum(const std::string& j, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return 0;
    p = j.find(':', p + k.size());
    if (p == std::string::npos) return 0;
    return _atoi64(j.c_str() + p + 1);
}

std::string KeyPath() {
    char path[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, path);
    strcat_s(path, "farshad_esp.key");
    return path;
}

std::string TgPath() {
    char path[MAX_PATH] = {};
    GetTempPathA(MAX_PATH, path);
    strcat_s(path, "farshad_esp.tg");
    return path;
}

std::string LoadTg() {
    std::ifstream f(TgPath());
    std::string t;
    if (f) std::getline(f, t);
    while (!t.empty() && (t.back()=='\r'||t.back()=='\n'||t.back()==' ')) t.pop_back();
    if (!t.empty() && t.size() < sizeof(g_inputTg))
        strcpy_s(g_inputTg, t.c_str());
    return t;
}

void SaveTg(const std::string& tg) {
    std::ofstream f(TgPath(), std::ios::trunc);
    if (f) f << tg;
}

std::string GetHWID() {
    if (!g_hwid.empty()) return g_hwid;
    DWORD serial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
    char comp[MAX_COMPUTERNAME_LENGTH + 1] = {};
    DWORD n = MAX_COMPUTERNAME_LENGTH + 1;
    GetComputerNameA(comp, &n);
    char buf[128];
    sprintf_s(buf, "%08X-%s", serial, comp);
    g_hwid = buf;
    return g_hwid;
}

std::string LoadSavedKey() {
    std::ifstream f(KeyPath());
    std::string k;
    if (f) std::getline(f, k);
    while (!k.empty() && (k.back()=='\r'||k.back()=='\n'||k.back()==' ')) k.pop_back();
    g_key = k;
    if (!k.empty() && k.size() < sizeof(g_inputKey))
        strcpy_s(g_inputKey, k.c_str());
    return k;
}

void SaveKey(const std::string& key) {
    g_key = key;
    std::ofstream f(KeyPath(), std::ios::trunc);
    if (f) f << key;
}

std::string MapError(const Status& st) {
    if (!st.message.empty()) return st.message;
    if (st.error == "hwid_mismatch")
        return "This license is locked to another PC. Ask admin to Reset HWID.";
    if (st.error == "expired") return "License expired";
    if (st.error == "disabled") return "License disabled";
    if (st.error == "invalid_key") return "Invalid license key";
    if (st.error == "missing_fields") return "Key / HWID missing";
    if (st.error == "join_channel")
        return "Please join the Telegram channel and enter your numeric ID";
    if (st.error == "network") return "Cannot reach license server";
    if (st.error == "server_misconfigured" || st.error == "internal_error" || st.error == "internal")
        return "License server error (KV limit or misconfig — try later)";
    if (st.error.find("429") != std::string::npos)
        return "Server rate limit — try again later";
    if (st.error.empty()) return "Activation failed";
    return "Error: " + st.error;
}

} // anon

Status Activate(const std::string& key, const std::string& tgId) {
    Status st;
    std::string body = "{\"key\":\"" + key + "\",\"hwid\":\"" + GetHWID() + "\"";
    if (!tgId.empty()) body += ",\"tg_id\":\"" + tgId + "\"";
    body += "}";

    std::string resp = HttpPost("/api/activate", body);
    if (resp.empty()) {
        st.error = "network";
        st.message = "Cannot reach license server";
        g_last = st;
        return st;
    }

    st.ok = JsonBool(resp, "ok");
    st.error = JsonStr(resp, "error");
    st.message = JsonStr(resp, "message");
    st.days_left = (int)JsonNum(resp, "days_left");
    st.expires_at = JsonNum(resp, "expires_at");
    st.server_time = JsonNum(resp, "server_time");
    if (st.error == "join_channel") st.need_channel = true;

    g_licensed = st.ok;
    g_daysLeft = st.days_left;
    if (st.ok) {
        SaveKey(key);
        if (!tgId.empty()) SaveTg(tgId);
    }
    g_last = st;
    return st;
}

Status ActivateSaved() {
    std::string k = LoadSavedKey();
    if (k.empty()) {
        Status st;
        st.error = "no_key";
        st.message = "No saved key";
        return st;
    }
    return Activate(k, LoadTg());
}

// Heartbeat removed — was burning free-tier KV puts every 5 min.
Status Heartbeat() {
    Status st;
    st.ok = g_licensed;
    st.error = g_licensed ? "" : "not_licensed";
    return st;
}

void ReportDebug() {
    // Do not hit server (costs KV put + auto-disables key).
    g_licensed = false;
}

bool IsLicensed() { return g_licensed; }

static void ExitApp() {
    PostQuitMessage(0);
    ExitProcess(0);
}

bool DrawGateAndTick() {
    // NO periodic network after activate.
    if (GetAsyncKeyState(VK_END) & 1)
        ExitApp();

    if (g_licensed)
        return true;

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(420, 280), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    if (!ImGui::Begin("Farshad ESP - License", &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        if (!open) ExitApp();
        return false;
    }
    if (!open) {
        ImGui::End();
        ExitApp();
        return false;
    }

    ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.f, 1.f), "Farshad ESP");
    ImGui::Separator();
    ImGui::TextWrapped("Enter license key once. Server is contacted only on Activate.");
    ImGui::TextDisabled("Press END to exit.");
    ImGui::Spacing();
    ImGui::InputText("License", g_inputKey, sizeof(g_inputKey));
    if (g_inputTg[0] == 0) LoadTg();
    ImGui::InputText("Telegram ID", g_inputTg, sizeof(g_inputTg));

    if (ImGui::Button("Activate", ImVec2(140, 0))) {
        auto st = Activate(g_inputKey, g_inputTg);
        if (!st.ok)
            g_uiMsg = MapError(st);
        else
            g_uiMsg = st.days_left >= 0
                ? ("OK — days left: " + std::to_string(st.days_left))
                : "OK";
    }
    ImGui::SameLine();
    if (ImGui::Button("Support", ImVec2(120, 0)))
        ShellExecuteA(nullptr, "open", "https://t.me/client_v34", nullptr, nullptr, SW_SHOWNORMAL);
    ImGui::SameLine();
    if (ImGui::Button("Exit", ImVec2(100, 0)))
        ExitApp();

    if (!g_uiMsg.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "%s", g_uiMsg.c_str());
    }

    ImGui::Separator();
    ImGui::TextDisabled("Support: @client_v34");
    ImGui::End();
    return false;
}

} // namespace License
