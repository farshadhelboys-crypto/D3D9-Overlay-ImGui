#pragma once
#include <string>

namespace License {
    // Set your Cloudflare Worker URL (no trailing slash)
    constexpr const char* API_BASE = "https://YOUR_WORKER.workers.dev";

    struct Status {
        bool ok = false;
        bool need_channel = false;
        std::string error;
        std::string message;
        int days_left = -1;
        long long expires_at = 0;
        long long server_time = 0;
    };

    std::string GetHWID();
    std::string LoadSavedKey();
    void SaveKey(const std::string& key);

    // Activate / heartbeat against Cloudflare Worker
    Status Activate(const std::string& key, const std::string& tgId = "");
    Status Heartbeat();

    // Report debugger → server disables key
    void ReportDebug();

    // Show license gate UI (ImGui). Returns true when licensed.
    bool DrawGateAndTick();

    bool IsLicensed();
}
