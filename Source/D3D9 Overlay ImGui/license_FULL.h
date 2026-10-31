#pragma once
#include <string>

namespace License {
    // Cloudflare Worker URL
    constexpr const char* API_BASE = "https://farshad-esp-license.farshadhelboys.workers.dev";

    // Ed25519 public key (32 bytes). Run gen_keys.py and paste C array here.
    // If all zero, signature verify is skipped (dev only — set real key for production).
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
    Status Heartbeat(); // no-op (no periodic network)

    // POST /api/offsets — requires prior activate. Returns raw offsets JSON string.
    bool FetchOffsets(std::string& outJson);

    void ReportDebug();
    bool DrawGateAndTick();
    bool IsLicensed();
    bool IsTrial();
    int TrialSecondsLeft();
}
