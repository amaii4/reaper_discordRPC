#include "stdafx.h"
#include "reaper_plugin.h"
#include "reaper_plugin_functions.h"
#include "discord_rpc.h"
#include <string>
#include <shlobj.h>
#pragma comment(lib, "comctl32.lib")

#define rpcenable 1001
#define rppname 1002
#define activeplaystate 1003

bool isrpcenable = true;
bool isrppname = true;
bool isactiveplaystate = true;

HWND hCheck_rpcenable = NULL;
HWND hCheck_rppname = NULL;
HWND hCheck_activeplaystate = NULL;

bool discordInitialized = false;

std::string GetConfigIniPath()
{
    char path[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, path);

    std::string fullPath = std::string(path) + "\\REAPER\\UserPlugins\\reaper_discordrpc.ini";
    return fullPath;
}

void SaveSettings()
{
    std::string iniPath = GetConfigIniPath();
    WritePrivateProfileStringA("Settings", "isrpcenable", isrpcenable ? "1" : "0", iniPath.c_str());
    WritePrivateProfileStringA("Settings", "isrppname", isrppname ? "1" : "0", iniPath.c_str());
    WritePrivateProfileStringA("Settings", "isactiveplaystate", isactiveplaystate ? "1" : "0", iniPath.c_str());
}

void LoadSettings()
{
    std::string iniPath = GetConfigIniPath();
    char value[8];
    GetPrivateProfileStringA("Settings", "isrpcenable", "1", value, sizeof(value), iniPath.c_str());
    isrpcenable = (strcmp(value, "1") == 0);

    GetPrivateProfileStringA("Settings", "isrppname", "1", value, sizeof(value), iniPath.c_str());
    isrppname = (strcmp(value, "1") == 0);

    GetPrivateProfileStringA("Settings", "isactiveplaystate", "1", value, sizeof(value), iniPath.c_str());
    isactiveplaystate = (strcmp(value, "1") == 0);
}

void InitDiscord() 
{
    if (discordInitialized) {
        return;
    }
    DiscordEventHandlers handlers;
    memset(&handlers, 0, sizeof(handlers));
    Discord_Initialize("959699230241460226", &handlers, 1, NULL);
    discordInitialized = true;
}

void UpdateDiscordPresence(const char* state, const char* details) {
    DiscordRichPresence presence;
    memset(&presence, 0, sizeof(presence));
    presence.state = state;
    presence.details = details;
    presence.largeImageKey = "reaper";
    presence.largeImageText = "REAPER";
    Discord_UpdatePresence(&presence);
}

std::string GetCurrentProjectName() {
    char path[MAX_PATH] = { 0 };
    EnumProjects(-1, path, MAX_PATH);

    const char* fileName = strrchr(path, '\\');
    if (fileName) return std::string(fileName + 1);
    return std::string("");
}

void TimerCallback()
{
    if (isrpcenable == false)
    {
        return;
    }
    InitDiscord();
    int playstate = GetPlayState();

    std::string currentState;
    std::string projectname;
    if(isrppname == true)
    {
        projectname = GetCurrentProjectName();
    }
    if (isactiveplaystate == true)
    {
        if (playstate == 1)
            currentState = u8"再生中";
        else if (playstate == 2)
            currentState = u8"停止中";
        else if (playstate == 4)
            currentState = u8"録音中";
        else
            currentState = u8"アイドル状態";
    }
    UpdateDiscordPresence(currentState.c_str(), projectname.c_str());
}

LRESULT CALLBACK PrefWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case rpcenable:
        {
            if (isrpcenable == false) {
                Discord_Shutdown();
                discordInitialized = false;
            }
            isrpcenable = !isrpcenable;
            SaveSettings();
            break;
        }
        case rppname:
        {
            isrppname = !isrppname;
            SaveSettings();
            break;
        }
        case activeplaystate:
        {
            isactiveplaystate = !isactiveplaystate;
            SaveSettings();
            break;
        }
        }
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

HWND OnCreatePrefWnd(HWND hWndParent)
{
    static bool registered = false;
    const wchar_t* className = L"MyPrefWindowClass";
    if (!registered)
    {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = PrefWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = className;
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        RegisterClassW(&wc);
        registered = true;
    }

    HWND hWnd = CreateWindowExW(
        0, className, NULL,
        WS_CHILD | WS_VISIBLE,
        0, 0, 400, 300,
        hWndParent, NULL, GetModuleHandle(NULL), NULL
    );

    hCheck_rpcenable = CreateWindowExW( // RPC
        0, L"BUTTON", L"RPC",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, 20, 200, 25,
        hWnd, (HMENU)rpcenable, GetModuleHandle(NULL), NULL
    );
    SendMessage(hCheck_rpcenable, BM_SETCHECK, isrpcenable ? BST_CHECKED : BST_UNCHECKED, 0);
    hCheck_rppname  = CreateWindowExW( // ファイル名
        0, L"BUTTON", L"プロジェクトファイル名",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, 50, 200, 25,
        hWnd, (HMENU)rppname, GetModuleHandle(NULL), NULL
    );
    SendMessage(hCheck_rppname, BM_SETCHECK, isrppname ? BST_CHECKED : BST_UNCHECKED, 0);
    hCheck_activeplaystate = CreateWindowExW( // playstate
        0, L"BUTTON", L"再生中",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        20, 80, 200, 25,
        hWnd, (HMENU)activeplaystate, GetModuleHandle(NULL), NULL
    );
    SendMessage(hCheck_activeplaystate, BM_SETCHECK, isactiveplaystate ? BST_CHECKED : BST_UNCHECKED, 0);

    return hWnd;
}

extern "C"
{
    REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance, reaper_plugin_info_t* rec)
    {
        static prefs_page_register_t g_myPrefs = {
            "discordrpc",      // idstr
            "DiscordRPC",      // displayname
            OnCreatePrefWnd,           // create
            0,                         // par_id
            NULL,                      // par_idstr
            0,                         // childrenFlag
            NULL,                      // treeitem
            NULL,                      // hwndCache
            ""                         // _extra
        };
        LoadSettings();
        if (rec) {
            InitCommonControls();
            int numOfErrors = REAPERAPI_LoadAPI(rec->GetFunc);

            plugin_register("timer", (void*)TimerCallback);
            plugin_register("prefpage", &g_myPrefs);

            return 1;
        }
        else {
            plugin_register("-timer", (void*)TimerCallback);
            plugin_register("-prefpage", &g_myPrefs);
            Discord_Shutdown();

            return 0;
        }
    }
}