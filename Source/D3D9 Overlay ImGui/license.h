#pragma once
#include <string>

namespace License {
    // Cloudflare Worker (no trailing slash)
    constexpr const char* API_BASE = "https://farshad-esp-licesnse.farshadhelboys.workers.dev";

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

    Status Activate(const std::string& key, const std::string& tgId = "");
    Status Heartbeat();

    void ReportDebug();

    bool DrawGateAndTick();

    bool IsLicensed();
}
