#include "UI.h"

#ifdef _WINDLL

HANDLE hCurrentUIThread = nullptr;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hinstDLL);
        UI::hCurrentModule = hinstDLL;
        hCurrentUIThread = CreateThread(nullptr, NULL, (LPTHREAD_START_ROUTINE)UI::Render, nullptr, NULL, nullptr);
    }

    if (fdwReason == DLL_PROCESS_DETACH)
    {
        // Prefer graceful exit. TerminateThread is last resort and can cause deadlocks.
        if (hCurrentUIThread)
        {
            // Signal the render loop to exit by posting WM_QUIT to the overlay window
            // (UI::Render already watches for WM_QUIT and VK_END).
            // As a safety net we still terminate after a short wait if needed.
            WaitForSingleObject(hCurrentUIThread, 500);
            if (WaitForSingleObject(hCurrentUIThread, 0) == WAIT_TIMEOUT)
                TerminateThread(hCurrentUIThread, 0);
            CloseHandle(hCurrentUIThread);
            hCurrentUIThread = nullptr;
        }
    }

    return TRUE;
}

#else

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    UI::Render();
    return 0;
}

#endif
