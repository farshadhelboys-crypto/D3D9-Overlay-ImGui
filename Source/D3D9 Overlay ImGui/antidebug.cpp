#include "antidebug.h"
#include "license.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <string>

namespace AntiDebug {
namespace {

bool NameMatch(const wchar_t* mod, const wchar_t* needle) {
    return mod && needle && wcsstr(mod, needle) != nullptr;
}

bool FindBadProcess() {
    static const wchar_t* bad[] = {
        L"cheatengine", L"cheat engine", L"x64dbg", L"x32dbg", L"ollydbg",
        L"ida", L"ida64", L"idaq", L"windbg", L"processhacker",
        L"procmon", L"wireshark", L"fiddler", L"httpdebugger",
        L"dnspy", L"ghidra", L"scylla", L"petool", L"importrec",
        L"reclass", L"extreme injector", L"ksdumper"
    };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{ sizeof(pe) };
    bool found = false;
    if (Process32FirstW(snap, &pe)) {
        do {
            std::wstring n = pe.szExeFile;
            for (auto& c : n) c = (wchar_t)towlower(c);
            for (auto* b : bad) {
                if (n.find(b) != std::wstring::npos) { found = true; break; }
            }
            if (found) break;
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

bool FindBadModule() {
    static const wchar_t* bad[] = {
        L"vehdebug", L"winhook", L"sharpod", L"scyllahide",
        L"x64dbg", L"x32dbg", L"titanhide"
    };
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
    if (snap == INVALID_HANDLE_VALUE) return false;
    MODULEENTRY32W me{ sizeof(me) };
    bool found = false;
    if (Module32FirstW(snap, &me)) {
        do {
            std::wstring n = me.szModule;
            for (auto& c : n) c = (wchar_t)towlower(c);
            for (auto* b : bad) {
                if (n.find(b) != std::wstring::npos) { found = true; break; }
            }
            if (found) break;
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

} // anon

bool Detected() {
    if (IsDebuggerPresent()) return true;
    BOOL remote = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote);
    if (remote) return true;

    // PEB BeingDebugged
#ifdef _WIN64
    // skip PEB on x64 simple path
#else
    __try {
        auto peb = (BYTE*)__readfsdword(0x30);
        if (peb && peb[2]) return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
#endif

    if (FindBadProcess()) return true;
    if (FindBadModule()) return true;
    return false;
}

void Guard() {
    if (!Detected()) return;
    License::ReportDebug();
    MessageBoxW(nullptr,
        L"داداش دیباگ نکن ممنونم \uD83D\uDE4F",
        L"Farshad ESP",
        MB_OK | MB_ICONERROR | MB_TOPMOST);
    ExitProcess(0);
}

} // namespace AntiDebug
