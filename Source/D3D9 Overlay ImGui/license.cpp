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
double g_lastBeat = 0;
Status g_last;
char g_inputKey[64] = {};
char g_inputTg[32] = {};
std::string g_uiMsg;

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

std::string HttpPost(const std::string& path, const std::string& jsonBody) {
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
    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(),
        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return {}; }
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return {}; }

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
            WinHttpReadData(hRequest, buf.data(), dwSize, &dwRead);
            result.append(buf.data(), dwRead);
        } while (dwSize > 0);
    }
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return result;
}

std::string JsonStr(const std::string& j, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return {};
    p = j.find(':', p);
    if (p == std::string::npos) return {};
    p = j.find('"', p);
    if (p == std::string::npos) return {};
    size_t e = j.find('"', p + 1);
    if (e == std::string::npos) return {};
    return j.substr(p + 1, e - p - 1);
}
bool JsonBool(const std::string& j, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return false;
    p = j.find(':', p);
    if (p == std::string::npos) return false;
    while (p < j.size() && (j[p] == ':' || j[p] == ' ')) p++;
    return j.compare(p, 4, "true") == 0;
}
long long JsonNum(const std::string& j, const char* key) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = j.find(k);
    if (p == std::string::npos) return 0;
    p = j.find(':', p);
    if (p == std::string::npos) return 0;
    while (p < j.size() && (j[p] == ':' || j[p] == ' ')) p++;
    return atoll(j.c_str() + p);
}

std::string KeyPath() {
    char path[MAX_PATH]{};
    GetTempPathA(MAX_PATH, path);
    strcat_s(path, "farshad_esp.key");
    return path;
}

} // anon

std::string GetHWID() {
    if (!g_hwid.empty()) return g_hwid;
    DWORD serial = 0;
    GetVolumeInformationA("C:\\", nullptr, 0, &serial, nullptr, nullptr, nullptr, 0);
    char comp[MAX_COMPUTERNAME_LENGTH + 1]{};
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
    while (!k.empty() && (k.back() == '\r' || k.back() == '\n' || k.back() == ' ')) k.pop_back();
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

Status Activate(const std::string& key, const std::string& tgId) {
    Status st;
    std::string body = "{\"key\":\"" + key + "\",\"hwid\":\"" + GetHWID() + "\"";
    if (!tgId.empty()) body += ",\"tg_id\":\"" + tgId + "\"";
    body += "}";
    std::string resp = HttpPost("/api/activate", body);
    if (resp.empty()) {
        st.error = "network";
        st.message = "No response from license server";
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
    if (st.ok) SaveKey(key);
    g_last = st;
    return st;
}

Status Heartbeat() {
    if (g_key.empty()) g_key = LoadSavedKey();
    Status st;
    if (g_key.empty()) { st.error = "no_key"; return st; }
    std::string body = "{\"key\":\"" + g_key + "\",\"hwid\":\"" + GetHWID() + "\"}";
    std::string resp = HttpPost("/api/heartbeat", body);
    if (resp.empty()) { st.error = "network"; g_last = st; return st; }
    st.ok = JsonBool(resp, "ok");
    st.error = JsonStr(resp, "error");
    st.message = JsonStr(resp, "message");
    st.days_left = (int)JsonNum(resp, "days_left");
    st.expires_at = JsonNum(resp, "expires_at");
    st.server_time = JsonNum(resp, "server_time");
    g_licensed = st.ok;
    g_last = st;
    if (resp.find("\"body\"") != std::string::npos) {
        std::string b = JsonStr(resp, "body");
        if (!b.empty()) g_uiMsg = b;
    }
    return st;
}

void ReportDebug() {
    if (g_key.empty()) g_key = LoadSavedKey();
    if (g_key.empty()) return;
    std::string body = "{\"key\":\"" + g_key + "\",\"hwid\":\"" + GetHWID() + "\"}";
    HttpPost("/api/report_debug", body);
    g_licensed = false;
}

bool IsLicensed() { return g_licensed; }

static void ExitApp() {
    PostQuitMessage(0);
    ExitProcess(0);
}

bool DrawGateAndTick() {
    double now = ImGui::GetTime();
    if (g_licensed && (now - g_lastBeat) > 300.0) {
        g_lastBeat = now;
        Heartbeat();
    }

    // Always allow exit keys
    if (GetAsyncKeyState(VK_ESCAPE) & 1)
        ExitApp();
    if (GetAsyncKeyState(VK_END) & 1)
        ExitApp();

    if (g_licensed) {
        if (!g_uiMsg.empty())
            ImGui::OpenPopup("FarshadMsg");
        if (ImGui::BeginPopupModal("FarshadMsg", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", g_uiMsg.c_str());
            if (ImGui::Button("OK", ImVec2(120, 0))) {
                g_uiMsg.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        return true;
    }

    // License gate — closable, with Exit
    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(420, 300), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
        ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    if (!ImGui::Begin("Farshad ESP - License", &open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return false;
    }
    if (!open) {
        ImGui::End();
        ExitApp();
        return false;
    }

    ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.f, 1.f), "Farshad ESP");
    ImGui::Separator();
    ImGui::TextWrapped("Please enter your license key.");
    ImGui::TextDisabled("Press ESC or END to exit.");
    ImGui::Spacing();
    ImGui::InputText("License", g_inputKey, sizeof(g_inputKey));
    ImGui::InputText("Telegram ID (optional)", g_inputTg, sizeof(g_inputTg));

    if (ImGui::Button("Activate", ImVec2(140, 0))) {
        auto st = Activate(g_inputKey, g_inputTg);
        if (!st.ok) {
            if (st.need_channel)
                g_uiMsg = st.message.empty() ? "Please join the Telegram channel" : st.message;
            else if (st.error == "hwid_mismatch")
                g_uiMsg = "This license is locked to another PC";
            else if (st.error == "expired")
                g_uiMsg = "License expired";
            else if (st.error == "disabled")
                g_uiMsg = "License disabled";
            else if (st.error == "network")
                g_uiMsg = "Cannot reach license server";
            else
                g_uiMsg = st.message.empty() ? ("Error: " + st.error) : st.message;
        }
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
