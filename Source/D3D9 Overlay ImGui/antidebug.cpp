#include "antidebug.h"
#include "license.h"
#include <Windows.h>
#include <TlHelp32.h>
#include <string>

namespace AntiDebug {
namespace {

typedef NTSTATUS (WINAPI* pNtQIP)(HANDLE, UINT, PVOID, ULONG, PULONG);

bool NtDebuggerPresent() {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return false;
    auto NtQueryInformationProcess = (pNtQIP)GetProcAddress(ntdll, "NtQueryInformationProcess");
    if (!NtQueryInformationProcess) return false;
    DWORD debugPort = 0;
    // ProcessDebugPort = 7
    NTSTATUS st = NtQueryInformationProcess(GetCurrentProcess(), 7, &debugPort, sizeof(debugPort), nullptr);
    if (st == 0 && debugPort != 0) return true;
    DWORD debugFlags = 0;
    // ProcessDebugFlags = 31
    st = NtQueryInformationProcess(GetCurrentProcess(), 31, &debugFlags, sizeof(debugFlags), nullptr);
    if (st == 0 && debugFlags == 0) return true;
    return false;
}

bool PebBeingDebugged() {
#ifdef _WIN64
    BYTE* peb = (BYTE*)__readgsqword(0x60);
#else
    BYTE* peb = (BYTE*)__readfsdword(0x30);
#endif
    if (!peb) return false;
    return peb[2] != 0; // BeingDebugged
}

bool FindBadProcess() {
    static const wchar_t* bad[] = {
        L"cheatengine", L"cheat engine", L"x64dbg", L"x32dbg", L"ollydbg",
        L"ida", L"ida64", L"idaq", L"windbg", L"processhacker",
        L"procmon", L"wireshark", L"fiddler", L"httpdebugger",
        L"dnspy", L"ghidra", L"scylla", L"petool", L"importrec",
        L"reclass", L"extreme injector", L"ksdumper", L"x96dbg",
        L"hxd", L"die.exe", L"detect it easy", L"apimonitor"
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
        L"x64dbg", L"x32dbg", L"titanhide", L"cheatengine"
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
    if (CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote) && remote)
        return true;

    if (PebBeingDebugged()) return true;
    if (NtDebuggerPresent()) return true;
    if (FindBadProcess()) return true;
    if (FindBadModule()) return true;
    return false;
}

void Guard() {
    // throttle heavy process scan ~ every 2s via tick count
    static DWORD lastHeavy = 0;
    DWORD now = GetTickCount();
    bool heavy = (now - lastHeavy) > 2000;

    bool hit = IsDebuggerPresent();
    if (!hit) {
        BOOL remote = FALSE;
        CheckRemoteDebuggerPresent(GetCurrentProcess(), &remote);
        hit = remote != FALSE;
    }
    if (!hit) hit = PebBeingDebugged();
    if (!hit) hit = NtDebuggerPresent();

    if (heavy) {
        lastHeavy = now;
        if (!hit) hit = FindBadProcess();
        if (!hit) hit = FindBadModule();
    }

    if (!hit) return;

    License::ReportDebug();
    MessageBoxW(nullptr,
        L"\x62f\x627\x62f\x627\x634 \x62f\x6cc\x628\x627\x6af \x646\x6a9\x646 \x645\x645\x646\x648\x646\x645",
        L"Farshad ESP",
        MB_OK | MB_ICONERROR | MB_TOPMOST);
    // Fallback ASCII message if unicode fails
    MessageBoxA(nullptr, "Dadash debug nakon mamnoonam", "Farshad ESP", MB_OK | MB_ICONERROR | MB_TOPMOST);
    ExitProcess(0);
}

} // namespace AntiDebug
