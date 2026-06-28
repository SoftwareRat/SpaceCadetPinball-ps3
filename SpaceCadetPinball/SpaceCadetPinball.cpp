// SpaceCadetPinball.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "winmain.h"

// PS3/PSL1GHT spezifische Includes
#include <sys/process.h>
#include <sysutil/sysutil.h>
#include <sys/tty.h>
#include <string.h>
#include <stdio.h>

// Da wir WHBLog via Makros in der pch.h umgeleitet haben,
// fangen wir die Wii U Initialisierungen hier einfach ab.
#define WHBProcInit()           (void)0
#define WHBLogCafeInit()        (void)0
#define WHBLogUdpInit()         (void)0
#define KPADInit()              (void)0
#define WPADEnableURCC(x)       (void)0
#define WPADEnableWiiRemote(x)  (void)0
#define KPADShutdown()          (void)0
#define WHBLogUdpDeinit()       (void)0
#define WHBLogCafeDeinit()      (void)0
#define WHBProcShutdown()       (void)0

// PS3 Sysutil Callback für das XMB-Menü (Home-Button / Quit Event)
static void sysutil_callback(uint64_t status, uint64_t param, void* usrdata)
{
    if (status == SYSUTIL_EXIT_GAME)
    {
        // Signalisiert SDL, dass das Event-Loop beendet werden soll
        SDL_Event event;
        event.type = SDL_QUIT;
        SDL_PushEvent(&event);
    }
}

int MainActual(LPCSTR lpCmdLine)
{
    const char* dbg_msg;
    
    // PS3 TTY DEBUG: Game started
    dbg_msg = "[DBG] SpaceCadetPinball MainActual START\n";
    sysTtyWrite(0, dbg_msg, strlen(dbg_msg), NULL);
    
    // PS3 DEBUG: Quick delay after entering MainActual (distinct from main() delay)
    {
        volatile u64 delay = 0;
        for (volatile u64 i = 0; i < 200000000; i++) { delay += i; }
    }
    
    WHBProcInit();
    WHBLogCafeInit();
    WHBLogUdpInit();
    KPADInit();
    WPADEnableURCC(true);
    WPADEnableWiiRemote(true);

    // PS3 Sysutil registrieren, um Home-Button-Events abzufangen
    dbg_msg = "[DBG] Registering sysutil callback...\n";
    sysTtyWrite(0, dbg_msg, strlen(dbg_msg), NULL);
    sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, sysutil_callback, NULL);
    dbg_msg = "[DBG] sysutil callback OK\n";
    sysTtyWrite(0, dbg_msg, strlen(dbg_msg), NULL);

    // SDL init
    dbg_msg = "[DBG] SDL_SetMainReady...\n";
    sysTtyWrite(0, dbg_msg, strlen(dbg_msg), NULL);
    SDL_SetMainReady();
    dbg_msg = "[DBG] SDL_Init(VIDEO|AUDIO|TIMER|EVENTS)...\n";
    sysTtyWrite(0, dbg_msg, strlen(dbg_msg), NULL);
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0)
    {
        // Auf der PS3 werfen wir den Fehler ins TTY/Netzwerk-Log, falls Videomodi fehlschlagen
        char err_buf[256];
        snprintf(err_buf, sizeof(err_buf), "[ERROR] Could not initialize SDL2: %s\n", SDL_GetError());
        sysTtyWrite(0, err_buf, strlen(err_buf), NULL);
        printf("[ERROR] Could not initialize SDL2: %s\n", SDL_GetError());
        return 1;
    }
    dbg_msg = "[DBG] SDL_Init OK\n";
    sysTtyWrite(0, dbg_msg, strlen(dbg_msg), NULL);
    
    // PS3 DEBUG: Delay after SDL_Init success
    {
        volatile u64 delay = 0;
        for (volatile u64 i = 0; i < 200000000; i++) { delay += i; }
    }

    int returnCode;
    char16_t errorMessage[2048];
    do
    {
        returnCode = winmain::WinMain(lpCmdLine, errorMessage);
    }
    while (winmain::RestartRequested());

    SDL_VideoQuit();
    SDL_Quit();

    // Wii U spezifischer Error-Viewer Code (`nn::erreula`) komplett entfernt.
    // Falls ein Fehler auftritt, wird er auf der PS3 im Terminal/Debugger geloggt.
    if (returnCode != 0)
    {
        printf("[FATAL] SpaceCadetPinball exited with code: %d\n", returnCode);
    }

    sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);

    KPADShutdown();
    WHBLogUdpDeinit();
    WHBLogCafeDeinit();
    WHBProcShutdown();

    char end_buf[64];
    snprintf(end_buf, sizeof(end_buf), "[DBG] Exiting with code: %d\n", returnCode);
    sysTtyWrite(0, end_buf, strlen(end_buf), NULL);
    
    // Return to ps3loadx if loaded via ps3load
    sysProcessExitSpawn2("/dev_hdd0/game/PSL145310/RELOAD.SELF",
        NULL, NULL, NULL, 0, 1001, SYS_PROCESS_SPAWN_STACK_SIZE_1M);

    return returnCode;
}

int main(int argc, char* argv[])
{
    // PS3 DEBUG: Visual timing test - delay BEFORE anything
    // If you see 5s black screen before game exits, code is running
    // If instant exit, crash is before main() (SPRX issue)
    {
        volatile u64 delay = 0;
        for (volatile u64 i = 0; i < 500000000; i++) { delay += i; }
    }
    
    std::string cmdLine;
    for (int i = 1; i < argc; i++)
    {
        cmdLine += argv[i];
        if (i < argc - 1) cmdLine += " ";
    }

    return MainActual(cmdLine.c_str());
}

#if _WIN32
#include <windows.h>

// Windows subsystem main
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    return MainActual(lpCmdLine);
}

// fopen to _wfopen adapter, for UTF-8 paths
FILE* fopenu(const char* path, const char* opt)
{
    wchar_t* wideArgs[2]{};
    for (auto& arg : wideArgs)
    {
        auto src = wideArgs[0] ? opt : path;
        auto length = MultiByteToWideChar(CP_UTF8, 0, src, -1, nullptr, 0);
        arg = new wchar_t[length];
        MultiByteToWideChar(CP_UTF8, 0, src, -1, arg, length);
    }

    auto fileHandle = _wfopen(wideArgs[0], wideArgs[1]);
    for (auto arg : wideArgs)
        delete[] arg;

    return fileHandle;
}
#endif