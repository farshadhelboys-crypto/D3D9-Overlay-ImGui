#pragma once
#include <string>

namespace License {
    // Cloudflare Worker (tested OK)
    constexpr const char* API_BASE = "https://farshad-esp-license.farshadhelboys.workers.dev";

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
    // One-time channel+license check at app start
    Status ActivateSaved();
    Status Heartbeat();

    void ReportDebug();

    bool DrawGateAndTick();

    bool IsLicensed();
}
