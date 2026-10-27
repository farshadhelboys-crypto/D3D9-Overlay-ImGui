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
    NTSTATUS st = NtQueryInformationProcess(GetCurrentProcess(), 7, &debugPort, sizeof(debugPort), nullptr);
    if (st == 0 && debugPort != 0) return true;
    DWORD debugFlags = 0;
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
    return peb[2] != 0;
}

// Detects tools already open BEFORE us and tools opened AFTER us (~every 1.5s)
bool FindBadProcess() {
    static const wchar_t* bad[] = {
        // Cheat Engine / memory editors
        L"cheatengine", L"cheat engine", L"ce-x",
        L"tutorial-i386", L"tutorial-x86_64",
        // Debuggers
        L"x64dbg", L"x32dbg", L"x96dbg", L"ollydbg", L"ollyice", L"windbg",
        L"devenv", L"msvsmon", L"vsjitdebugger", L"vshost", L"msdev",
        L"code.exe",
        L"ida64", L"idaq", L"idag", L"idaw", L"idau", L"ida.exe",
        L"ghidra", L"radare", L"r2.exe", L"binja",
        L"immunity", L"immdbg",
        L"httpdebugger", L"fiddler", L"charles", L"wireshark", L"tshark",
        L"processhacker", L"procmon", L"procexp",
        L"dnspy", L"ilspy", L"dotpeek",
        L"scylla", L"petool", L"importrec", L"lordpe", L"pe-bear", L"pestudio",
        L"reclass", L"extreme injector", L"extremeinjector",
        L"ksdumper", L"megadumper", L"apimonitor", L"rohitab",
        L"sandboxie", L"softice", L"syser",
        L"titanhide", L"scyllahide", L"sharpod",
        L"vehdebug", L"winhook", L"hookshark",
        L"exeinfope", L"peid",
        L"resourcehacker", L"reshacker",
        L"hollowshunter", L"pe-sieve",
        L"dbgview",
        L"xenos", L"dllinjector", L"processinjector"
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
        L"x64dbg", L"x32dbg", L"titanhide", L"cheatengine",
        L"speedhack", L"dbk32", L"dbk64", L"ced3d"
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

struct EnumCtx { bool hit; };
BOOL CALLBACK EnumWndProc(HWND hwnd, LPARAM lp) {
    auto* ctx = (EnumCtx*)lp;
    if (!IsWindowVisible(hwnd)) return TRUE;
    wchar_t title[256]{};
    GetWindowTextW(hwnd, title, 255);
    if (!title[0]) return TRUE;
    std::wstring t = title;
    for (auto& c : t) c = (wchar_t)towlower(c);
    static const wchar_t* badTitle[] = {
        L"cheat engine", L"x64dbg", L"x32dbg", L"ollydbg", L"windbg",
        L"ida -", L"ida pro", L"ghidra", L"process hacker", L"process monitor",
        L"dnspy", L"http debugger", L"fiddler", L"wireshark",
        L"reclass", L"extreme injector"
    };
    for (auto* b : badTitle) {
        if (t.find(b) != std::wstring::npos) { ctx->hit = true; return FALSE; }
    }
    return TRUE;
}

bool FindBadWindow() {
    EnumCtx ctx{ false };
    EnumWindows(EnumWndProc, (LPARAM)&ctx);
    return ctx.hit;
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
    if (FindBadWindow()) return true;
    return false;
}

void Guard() {
    static DWORD lastHeavy = 0;
    DWORD now = GetTickCount();
    bool heavy = (now - lastHeavy) > 1500;

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
        if (!hit) hit = FindBadWindow();
    }

    if (!hit) return;

    License::ReportDebug();
    MessageBoxW(nullptr, L"Dadash debug nakon mamnoonam", L"Farshad ESP", MB_OK | MB_ICONWARNING | MB_TOPMOST);
    ExitProcess(0);
}

} // namespace AntiDebug
