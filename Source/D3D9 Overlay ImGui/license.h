#pragma once
#include <string>

namespace License {
    constexpr const char* API_BASE = "https://farshad-esp-license.farshadhelboys.workers.dev";

    // Paste public key from gen_keys.py. All-zero = structural checks only (dev).
    static const unsigned char LICENSE_PUBKEY[32] = {
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0
    };

    struct Status {
        bool ok = false;
        bool need_channel = false;
        bool trial = false;
        std::string error;
        std::string message;
        int days_left = -1;
        int seconds_left = -1;
        long long expires_at = 0;
        long long server_time = 0;
    };

    std::string GetHWID();
    std::string LoadSavedKey();
    void SaveKey(const std::string& key);

    Status Activate(const std::string& key, const std::string& tgId = "");
    Status ActivateSaved();
    Status Heartbeat();

    bool FetchOffsets(std::string& outJson);

    void ReportDebug();
    bool DrawGateAndTick();
    bool IsLicensed();
    bool IsTrial();
    int TrialSecondsLeft();
}
