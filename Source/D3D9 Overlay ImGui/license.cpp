#include "license.h"
#include "ImGui/imgui.h"
#include <Windows.h>
#include <bcrypt.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>
#include <cstring>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

namespace License {
namespace {

std::string g_key;
std::string g_hwid;
bool g_licensed = false;
bool g_trial = false;
int g_seconds_left = -1;
double g_trial_start_imgui = 0;
Status g_last;
char g_inputKey[64] = {};
char g_inputTg[32] = {};
std::string g_uiMsg;
std::string g_offsets_cache;

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}

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

    HINTERNET hSession = WinHttpOpen(L"FarshadESP/2.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return {};
    WinHttpSetTimeouts(hSession, 5000, 5000, 10000, 10000);

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

bool PubkeyConfigured() {
    for (int i = 0; i < 32; i++)
        if (LICENSE_PUBKEY[i] != 0) return true;
    return false;
}

std::vector<unsigned char> B64Decode(const std::string& in) {
    static const int T[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
        -1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
    };
    std::vector<unsigned char> out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (c == '=' || c == '\n' || c == '\r') continue;
        if (T[c] == -1) continue;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back((unsigned char)((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

std::string NewNonce() {
    unsigned char buf[16];
    BCryptGenRandom(nullptr, buf, sizeof(buf), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    static const char* hex = "0123456789abcdef";
    std::string s;
    s.resize(32);
    for (int i = 0; i < 16; i++) {
        s[i * 2] = hex[buf[i] >> 4];
        s[i * 2 + 1] = hex[buf[i] & 0xf];
    }
    return s;
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

std::string ExtractPayloadString(const std::string& resp) {
    const std::string k = "\"payload\"";
    size_t p = resp.find(k);
    if (p == std::string::npos) return {};
    p = resp.find(':', p + k.size());
    if (p == std::string::npos) return {};
    p = resp.find('\"', p + 1);
    if (p == std::string::npos) return {};
    size_t i = p + 1;
    std::string out;
    while (i < resp.size()) {
        char c = resp[i++];
        if (c == '\\' && i < resp.size()) {
            char n = resp[i++];
            if (n == '\"' || n == '\\' || n == '/') out.push_back(n);
            else if (n == 'n') out.push_back('\n');
            else if (n == 'r') out.push_back('\r');
            else if (n == 't') out.push_back('\t');
            else out.push_back(n);
        } else if (c == '\"') break;
        else out.push_back(c);
    }
    return out;
}

bool AcceptSigned(const std::string& resp, const std::string& nonce, const std::string& hwid,
                  const char* kind, std::string& payloadOut) {
    std::string payload = ExtractPayloadString(resp);
    std::string sig = JsonStr(resp, "sig");

    if (payload.empty()) {
        if (PubkeyConfigured()) return false;
        payloadOut = resp;
        return JsonBool(resp, "ok");
    }

    if (!JsonBool(payload, "ok")) return false;
    if (JsonStr(payload, "nonce") != nonce) return false;
    if (JsonStr(payload, "hwid") != hwid) return false;
    if (JsonStr(payload, "kind") != kind) return false;

    if (PubkeyConfigured()) {
        auto raw = B64Decode(sig);
        if (raw.size() != 64) return false;
    }

    payloadOut = payload;
    return true;
}

std::string MapError(const Status& st) {
    if (!st.message.empty()) return st.message;
    if (st.error == "hwid_mismatch") return "License locked to another PC";
    if (st.error == "expired") return "License expired";
    if (st.error == "disabled") return "License disabled";
    if (st.error == "invalid_key") return "Invalid key";
    if (st.error == "join_channel") return "Join Telegram channel + enter numeric ID";
    if (st.error == "network") return "Cannot reach server";
    if (st.error == "trial_expired") return "Trial finished - buy a license";
    if (st.error == "trial_disabled") return "Trial disabled";
    if (st.error == "trial_unavailable") return "Daily trial capacity full";
    if (st.error == "server_misconfigured" || st.error == "internal_error")
        return "Server error - try later";
    if (st.error.empty()) return "Activation failed";
    return "Error: " + st.error;
}

} // anon

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

Status Activate(const std::string& key, const std::string& tgId) {
    Status st;
    std::string hwid = GetHWID();
    std::string nonce = NewNonce();

    std::string body = "{\"key\":\"" + key + "\",\"hwid\":\"" + hwid + "\",\"nonce\":\"" + nonce + "\"";
    if (!tgId.empty()) body += ",\"tg_id\":\"" + tgId + "\"";
    body += "}";

    std::string resp = HttpPost("/api/activate", body);
    if (resp.empty()) {
        st.error = "network";
        st.message = "Cannot reach license server";
        g_last = st;
        return st;
    }

    std::string payload;
    if (!AcceptSigned(resp, nonce, hwid, "activate", payload)) {
        st.ok = false;
        st.error = JsonStr(resp, "error");
        st.message = JsonStr(resp, "message");
        if (st.error.empty()) st.error = "bad_signature";
        if (st.error == "join_channel") st.need_channel = true;
        g_licensed = false;
        g_last = st;
        return st;
    }

    st.ok = true;
    st.trial = JsonBool(payload, "trial");
    st.days_left = (int)JsonNum(payload, "days_left");
    st.seconds_left = (int)JsonNum(payload, "seconds_left");
    st.expires_at = JsonNum(payload, "expires_at");
    st.server_time = JsonNum(payload, "server_time");
    if (st.seconds_left <= 0 && st.days_left > 0)
        st.seconds_left = st.days_left * 86400;

    g_licensed = true;
    g_trial = st.trial;
    g_seconds_left = st.seconds_left;
    g_trial_start_imgui = ImGui::GetTime();
    SaveKey(key);
    if (!tgId.empty()) SaveTg(tgId);
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

Status Heartbeat() {
    Status st;
    st.ok = g_licensed;
    st.error = g_licensed ? "" : "not_licensed";
    return st;
}

bool FetchOffsets(std::string& outJson) {
    if (!g_licensed) return false;
    if (!g_offsets_cache.empty()) {
        outJson = g_offsets_cache;
        return true;
    }
    std::string hwid = GetHWID();
    std::string nonce = NewNonce();
    std::string key = g_key.empty() ? LoadSavedKey() : g_key;
    std::string body = "{\"key\":\"" + key + "\",\"hwid\":\"" + hwid + "\",\"nonce\":\"" + nonce + "\"}";
    std::string resp = HttpPost("/api/offsets", body);
    if (resp.empty()) return false;
    std::string payload;
    if (!AcceptSigned(resp, nonce, hwid, "offsets", payload)) return false;
    std::string off = JsonStr(payload, "offsets");
    if (off.empty()) return false;
    g_offsets_cache = off;
    outJson = off;
    return true;
}

void ReportDebug() {
    g_licensed = false;
}

bool IsLicensed() {
    if (!g_licensed) return false;
    if (g_trial && g_seconds_left > 0) {
        double elapsed = ImGui::GetTime() - g_trial_start_imgui;
        if (elapsed >= (double)g_seconds_left) {
            g_licensed = false;
            g_uiMsg = "Trial finished - buy a license";
            return false;
        }
    }
    return true;
}

bool IsTrial() { return g_trial; }

int TrialSecondsLeft() {
    if (!g_trial || g_seconds_left <= 0) return -1;
    double elapsed = ImGui::GetTime() - g_trial_start_imgui;
    int left = g_seconds_left - (int)elapsed;
    return left > 0 ? left : 0;
}

static void ExitApp() {
    PostQuitMessage(0);
    ExitProcess(0);
}

bool DrawGateAndTick() {
    if (GetAsyncKeyState(VK_END) & 1)
        ExitApp();

    if (IsLicensed())
        return true;

    bool open = true;
    ImGui::SetNextWindowSize(ImVec2(440, 300), ImGuiCond_FirstUseEver);
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
    ImGui::TextWrapped("One request on Activate. Signed license server.");
    ImGui::TextDisabled("Press END to exit.");
    ImGui::Spacing();
    ImGui::InputText("License / Trial key", g_inputKey, sizeof(g_inputKey));
    if (g_inputTg[0] == 0) LoadTg();
    ImGui::InputText("Telegram ID", g_inputTg, sizeof(g_inputTg));

    if (ImGui::Button("Activate", ImVec2(140, 0))) {
        auto st = Activate(g_inputKey, g_inputTg);
        if (!st.ok)
            g_uiMsg = MapError(st);
        else if (st.trial)
            g_uiMsg = "Trial OK - hours left: " + std::to_string(st.seconds_left > 0 ? (st.seconds_left + 3599) / 3600 : 0);
        else
            g_uiMsg = st.days_left >= 0 ? ("OK - days left: " + std::to_string(st.days_left)) : "OK";
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
