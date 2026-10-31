#pragma once
#include <string>

namespace License {
    constexpr const char* API_BASE = "https://farshad-esp-license.farshadhelboys.workers.dev";

    // Paste public key from gen_keys.py. All-zero = structural checks only (dev).
    static const unsigned char LICENSE_PUBKEY[32] = { 0x76, 0xE6, 0x60, 0x23, 0x97, 0x51, 0xDD, 0x3A, 0x12, 0x59, 0x56, 0xAF, 0x46, 0xF8, 0x14, 0xCB, 0x63, 0xFE, 0xFB, 0xB2, 0xA3, 0xF0, 0x94, 0x82, 0xC4, 0x46, 0xBF, 0x6D, 0xC0, 0xF2, 0xD6, 0x07

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
