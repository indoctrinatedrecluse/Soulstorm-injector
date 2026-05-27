#include <windows.h>
#include <string>
#include <vector>
#include <d3d9.h>
#include <map>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <dbghelp.h>

#include "scanner.h"
#include "seh_helpers.h"
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dbghelp.lib")

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

// --- Globals ---
uintptr_t g_playerBase = 0;
lua_State* g_LuaState = nullptr;
bool g_uninject = false;
bool g_showMenu = true;
HWND g_window = nullptr;
HMODULE g_hModule = nullptr;

// --- Logging Setup ---
std::ofstream g_logFile;

void Log(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char buf[100];
    ctime_s(buf, sizeof(buf), &time);
    std::string timeStr(buf);
    timeStr.erase(timeStr.length() - 1); // remove newline

    std::string logLine = "[" + timeStr + "] [PAYLOAD] " + message;

    if (g_logFile.is_open()) {
        g_logFile << logLine << std::endl;
        g_logFile.flush();
    }
}

// --- Crash Handler ---
LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* pExceptionInfo) {
    Log("CRITICAL ERROR: Unhandled Exception Caught!");

    char dumpPath[MAX_PATH];
    GetModuleFileNameA(g_hModule, dumpPath, MAX_PATH);
    std::string strDumpPath(dumpPath);
    strDumpPath = strDumpPath.substr(0, strDumpPath.find_last_of("\\/")) + "\\crash.dmp";

    HANDLE hFile = CreateFileA(strDumpPath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
        dumpInfo.ThreadId = GetCurrentThreadId();
        dumpInfo.ExceptionPointers = pExceptionInfo;
        dumpInfo.ClientPointers = FALSE;

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpNormal, &dumpInfo, NULL, NULL);
        CloseHandle(hFile);
        Log("Crash dump saved to: " + strDumpPath);
    } else {
        Log("Failed to create crash dump file.");
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

// --- Function Pointers ---
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
WNDPROC pOriginalWndProc = nullptr;

typedef long(__stdcall* EndScene_t)(IDirect3DDevice9* pDevice);
EndScene_t pOriginalEndScene = nullptr;

// --- Hook Function Typedefs ---
typedef int(__fastcall* GetPlayerBase_t)(void* ecx, void* edx, void* arg1);
typedef void(__fastcall* HealthUpdate_t)(void* ecx, void* edx, float damage);
typedef void(__fastcall* ResourceUpdate_t)(void* ecx, void* edx, void* resource_struct);
typedef void(__fastcall* BuildTimeUpdate_t)(void* ecx, void* edx, void* build_struct);
typedef void(__fastcall* MoraleUpdate_t)(void* ecx, void* edx, void* esp_plus_10);
typedef void(__fastcall* SquadCapUpdate_t)(void* ecx, void* edx);
typedef void(__fastcall* EquipTimeUpdate_t)(void* ecx, void* edx);
typedef void(__fastcall* FogOfWarUpdate_t)(void* ecx, void* edx);
typedef void(__fastcall* OneHitKill_t)(void* ecx, void* edx, void* arg1);
typedef void(__fastcall* InstantCapture_t)(void* ecx, void* edx);
typedef void(__fastcall* FastAbilities_t)(void* ecx, void* edx);
typedef void(__fastcall* AllWargear_t)(void* ecx, void* edx);

// --- Original Pointers ---
GetPlayerBase_t pOrigGetPlayerBase = nullptr;
HealthUpdate_t pOrigHealthUpdate = nullptr;
ResourceUpdate_t pOrigResourceUpdate = nullptr;
BuildTimeUpdate_t pOrigBuildTimeUpdate = nullptr;
MoraleUpdate_t pOrigMoraleUpdate = nullptr;
SquadCapUpdate_t pOrigSquadCapUpdate = nullptr;
EquipTimeUpdate_t pOrigEquipTimeUpdate = nullptr;
FogOfWarUpdate_t pOrigFogOfWarUpdate = nullptr;
OneHitKill_t pOrigOneHitKill = nullptr;
InstantCapture_t pOrigInstantCapture = nullptr;
FastAbilities_t pOrigFastAbilities = nullptr;
AllWargear_t pOrigAllWargear = nullptr;

// --- Hook Addresses ---
uintptr_t addr_GetPlayerBase = 0;
uintptr_t addr_InfiniteHealth = 0;
uintptr_t addr_InfiniteResources = 0;
uintptr_t addr_FastBuild = 0;
uintptr_t addr_InfiniteMorale = 0;
uintptr_t addr_InfiniteCap = 0;
uintptr_t addr_InstantEquipment = 0;
uintptr_t addr_RemoveFOW = 0;
uintptr_t addr_OneHitKill = 0;
uintptr_t addr_InstantCapture = 0;
uintptr_t addr_FastAbilities = 0;
uintptr_t addr_AllWargear = 0;

// --- Cheat States ---
namespace Cheats {
    bool infinite_resources = false;
    bool infinite_faith = false;
    bool infinite_souls = false;
}

// --- Detours ---
int __fastcall DetourGetPlayerBase(void* ecx, void* edx, void* arg1) {
    __asm { mov g_playerBase, ebx }
    return pOrigGetPlayerBase(ecx, edx, arg1);
}

void __fastcall DetourHealthUpdate(void* ecx, void* edx, float damage) {
    uintptr_t entityOwner = 0;
    __asm { mov entityOwner, esi }

    if (IsPlayerEntity(entityOwner, g_playerBase)) return;

    pOrigHealthUpdate(ecx, edx, damage);
}

void __fastcall DetourResourceUpdate(void* ecx, void* edx, void* resource_struct) {
    uintptr_t resourceOwner = 0;
    __asm { mov resourceOwner, esi }

    if (IsPlayerResource(resourceOwner, g_playerBase)) {
        if (Cheats::infinite_resources) {
            *(float*)((uintptr_t)ecx + 0x00) = 99999.0f; // Req
            *(float*)((uintptr_t)ecx + 0x04) = 99999.0f; // Power
        }
        if (Cheats::infinite_faith) *(float*)((uintptr_t)ecx + 0x0C) = 99999.0f;
        if (Cheats::infinite_souls) *(float*)((uintptr_t)ecx + 0x10) = 99999.0f;
    }

    pOrigResourceUpdate(ecx, edx, resource_struct);
}

void __fastcall DetourBuildTimeUpdate(void* ecx, void* edx, void* build_struct) {
    __try { *(float*)((uintptr_t)ecx + 0x0C) = 0.0f; } __except(EXCEPTION_EXECUTE_HANDLER) { Log("Exception caught in DetourBuildTimeUpdate"); }
    pOrigBuildTimeUpdate(ecx, edx, build_struct);
}

void __fastcall DetourMoraleUpdate(void* ecx, void* edx, void* esp_plus_10) {
    uintptr_t entityOwner = 0;
    __asm { mov entityOwner, ebp }

    if (IsPlayerMorale(entityOwner, g_playerBase)) {
        __try {
            *(float*)((uintptr_t)ecx + 0x08) = 1.0f;
        } __except(EXCEPTION_EXECUTE_HANDLER) { Log("Exception caught in DetourMoraleUpdate"); }
        return;
    }

    pOrigMoraleUpdate(ecx, edx, esp_plus_10);
}

void __fastcall DetourSquadCapUpdate(void* ecx, void* edx) {
    __try { *(float*)ecx = 0.0f; } __except(EXCEPTION_EXECUTE_HANDLER) { Log("Exception caught in DetourSquadCapUpdate"); }
    pOrigSquadCapUpdate(ecx, edx);
}

void __fastcall DetourEquipTimeUpdate(void* ecx, void* edx) {
    __try { *(float*)((uintptr_t)ecx + 0x08) = 0.0f; } __except(EXCEPTION_EXECUTE_HANDLER) { Log("Exception caught in DetourEquipTimeUpdate"); }
    pOrigEquipTimeUpdate(ecx, edx);
}

void __fastcall DetourFogOfWarUpdate(void* ecx, void* edx) {
    __try { *(float*)((uintptr_t)ecx + 0xC58) = 0.1f; } __except(EXCEPTION_EXECUTE_HANDLER) { Log("Exception caught in DetourFogOfWarUpdate"); }
    pOrigFogOfWarUpdate(ecx, edx);
}

void __fastcall DetourOneHitKill(void* ecx, void* edx, void* arg1) {
    uintptr_t entityOwner = 0;
    __asm { mov entityOwner, esi }

    if (!IsPlayerEntity(entityOwner, g_playerBase)) {
        __try {
            *(float*)((uintptr_t)ecx + 0x08) = 90000.0f;
        } __except(EXCEPTION_EXECUTE_HANDLER) { Log("Exception caught in DetourOneHitKill"); }
    }

    pOrigOneHitKill(ecx, edx, arg1);
}

void __fastcall DetourInstantCapture(void* ecx, void* edx) {
    uintptr_t entityOwner = 0;
    __asm { mov entityOwner, esi }

    if (IsPlayerEntity(entityOwner, g_playerBase)) {
         __try {
            *(float*)((uintptr_t)ecx + 0x44) = 360.0f;
         } __except(EXCEPTION_EXECUTE_HANDLER) { Log("Exception caught in DetourInstantCapture"); }
    }

    pOrigInstantCapture(ecx, edx);
}

void __fastcall DetourFastAbilities(void* ecx, void* edx) {
     __try { *(float*)((uintptr_t)ecx + 0x78) = 0.0f; } __except(EXCEPTION_EXECUTE_HANDLER) { Log("Exception caught in DetourFastAbilities"); }
     pOrigFastAbilities(ecx, edx);
}

void __fastcall DetourAllWargear(void* ecx, void* edx) {
    pOrigAllWargear(ecx, edx);
}

// --- Lua Bridge ---
void ToggleHook(uintptr_t address, void* detour, void** original, bool enable, const char* name) {
    if (address == 0) {
        Log(std::string("Failed to toggle ") + name + ": Address is 0 (Pattern not found).");
        return;
    }

    if (enable) {
        if (*original == nullptr) {
            if (MH_CreateHook((LPVOID)address, detour, original) != MH_OK) {
                Log(std::string("Failed to create hook for ") + name);
                return;
            }
        }
        if (MH_EnableHook((LPVOID)address) == MH_OK) {
            Log(std::string("Enabled hook for ") + name);
            Beep(600, 100);
        } else {
             Log(std::string("Failed to enable hook for ") + name);
        }
    } else {
        if (MH_DisableHook((LPVOID)address) == MH_OK) {
             Log(std::string("Disabled hook for ") + name);
             Beep(400, 100);
        } else {
             Log(std::string("Failed to disable hook for ") + name);
        }
    }
}

static int l_ToggleCheat(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    bool enable = lua_toboolean(L, 2);

    Log(std::string("Lua requested toggle: ") + name + " -> " + (enable ? "ON" : "OFF"));

    if (strcmp(name, "infinite_health") == 0) ToggleHook(addr_InfiniteHealth, DetourHealthUpdate, (void**)&pOrigHealthUpdate, enable, name);
    else if (strcmp(name, "infinite_resources") == 0) { Cheats::infinite_resources = enable; ToggleHook(addr_InfiniteResources, DetourResourceUpdate, (void**)&pOrigResourceUpdate, enable || Cheats::infinite_faith || Cheats::infinite_souls, "infinite_resources (shared)"); }
    else if (strcmp(name, "infinite_faith") == 0) { Cheats::infinite_faith = enable; ToggleHook(addr_InfiniteResources, DetourResourceUpdate, (void**)&pOrigResourceUpdate, enable || Cheats::infinite_resources || Cheats::infinite_souls, "infinite_faith (shared)"); }
    else if (strcmp(name, "infinite_souls") == 0) { Cheats::infinite_souls = enable; ToggleHook(addr_InfiniteResources, DetourResourceUpdate, (void**)&pOrigResourceUpdate, enable || Cheats::infinite_resources || Cheats::infinite_faith, "infinite_souls (shared)"); }
    else if (strcmp(name, "fast_build") == 0) ToggleHook(addr_FastBuild, DetourBuildTimeUpdate, (void**)&pOrigBuildTimeUpdate, enable, name);
    else if (strcmp(name, "infinite_morale") == 0) ToggleHook(addr_InfiniteMorale, DetourMoraleUpdate, (void**)&pOrigMoraleUpdate, enable, name);
    else if (strcmp(name, "infinite_cap") == 0) ToggleHook(addr_InfiniteCap, DetourSquadCapUpdate, (void**)&pOrigSquadCapUpdate, enable, name);
    else if (strcmp(name, "instant_equip") == 0) ToggleHook(addr_InstantEquipment, DetourEquipTimeUpdate, (void**)&pOrigEquipTimeUpdate, enable, name);
    else if (strcmp(name, "instant_capture") == 0) ToggleHook(addr_InstantCapture, DetourInstantCapture, (void**)&pOrigInstantCapture, enable, name);
    else if (strcmp(name, "fast_abilities") == 0) ToggleHook(addr_FastAbilities, DetourFastAbilities, (void**)&pOrigFastAbilities, enable, name);
    else if (strcmp(name, "remove_fow") == 0) ToggleHook(addr_RemoveFOW, DetourFogOfWarUpdate, (void**)&pOrigFogOfWarUpdate, enable, name);
    else if (strcmp(name, "one_hit_kill") == 0) ToggleHook(addr_OneHitKill, DetourOneHitKill, (void**)&pOrigOneHitKill, enable, name);
    else if (strcmp(name, "all_wargear") == 0) ToggleHook(addr_AllWargear, DetourAllWargear, (void**)&pOrigAllWargear, enable, name);
    else {
        Log(std::string("Unknown cheat requested: ") + name);
    }

    return 0;
}

static int l_ImGui_Checkbox(lua_State* L) {
    const char* label = lua_tostring(L, 1);
    bool value = lua_toboolean(L, 2);
    if (ImGui::Checkbox(label, &value)) {
        lua_pushboolean(L, value);
        return 1;
    }
    return 0;
}

static int l_ImGui_Begin(lua_State* L) { ImGui::Begin(lua_tostring(L, 1)); return 0; }
static int l_ImGui_End(lua_State* L) { ImGui::End(); return 0; }

// --- DirectX & ImGui ---
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DetourWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (g_showMenu && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) return true;
    return CallWindowProc(pOriginalWndProc, hWnd, uMsg, wParam, lParam);
}

long __stdcall DetourEndScene(IDirect3DDevice9* pDevice) {
    static bool init = false;
    if (!init) {
        Log("Initializing ImGui within EndScene hook...");
        g_window = FindWindowA("W40kWindow", "Dawn of War: Soulstorm");
        if(g_window == nullptr) {
            Log("WARNING: Could not find game window for input hooking.");
        } else {
            ImGui::CreateContext();
            ImGui_ImplWin32_Init(g_window);
            ImGui_ImplDX9_Init(pDevice);
            pOriginalWndProc = (WNDPROC)SetWindowLongPtr(g_window, GWLP_WNDPROC, (LONG_PTR)DetourWndProc);
            Log("ImGui initialized and WndProc hooked.");
            init = true;
        }
    }

    if (init) {
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (g_showMenu) {
            lua_getglobal(g_LuaState, "on_draw_frame");
            if (lua_isfunction(g_LuaState, -1)) {
                if(lua_pcall(g_LuaState, 0, 0, 0) != LUA_OK) {
                    Log(std::string("Lua Error during on_draw_frame: ") + lua_tostring(g_LuaState, -1));
                }
            }
        }

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    return pOriginalEndScene(pDevice);
}

void Shutdown() {
    Log("Executing Shutdown routine...");
    if (pOriginalWndProc) {
        SetWindowLongPtr(g_window, GWLP_WNDPROC, (LONG_PTR)pOriginalWndProc);
        Log("Restored original WndProc.");
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    Log("Uninitialized MinHook.");

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    Log("ImGui shut down.");

    if (g_LuaState) {
        lua_close(g_LuaState);
        Log("Lua state closed.");
    }

    if (g_logFile.is_open()) {
        Log("Closing log file.");
        g_logFile.close();
    }
}

bool LoadScriptFromResource(const char* scriptName) {
    HRSRC hRes = FindResource(g_hModule, scriptName, RT_RCDATA);
    if (!hRes) {
        Log(std::string("Failed to find resource: ") + scriptName);
        return false;
    }
    HGLOBAL hResLoad = LoadResource(g_hModule, hRes);
    if (!hResLoad) {
        Log(std::string("Failed to load resource: ") + scriptName);
        return false;
    }
    char* scriptData = (char*)LockResource(hResLoad);
    size_t scriptSize = SizeofResource(g_hModule, hRes);
    if (scriptData && scriptSize > 0) {
        if (luaL_loadbuffer(g_LuaState, scriptData, scriptSize, scriptName) == LUA_OK) {
            if(lua_pcall(g_LuaState, 0, 0, 0) == LUA_OK) {
                Log(std::string("Successfully executed script: ") + scriptName);
                return true;
            } else {
                 Log(std::string("Error executing script ") + scriptName + ": " + lua_tostring(g_LuaState, -1));
            }
        } else {
             Log(std::string("Error loading script ") + scriptName + ": " + lua_tostring(g_LuaState, -1));
        }
    }
    return false;
}

DWORD WINAPI PayloadThread(LPVOID lpParam) {
    // Set up crash handling
    SetUnhandledExceptionFilter(UnhandledExceptionHandler);

    char logPath[MAX_PATH];
    GetModuleFileNameA(g_hModule, logPath, MAX_PATH);
    std::string strLogPath(logPath);
    strLogPath = strLogPath.substr(0, strLogPath.find_last_of("\\/")) + "\\payload.log";
    g_logFile.open(strLogPath, std::ios::out | std::ios::app);
    Log("Payload thread started.");

    // Delay initialization to ensure game modules are fully loaded
    Log("Waiting for WXPMod.dll...");
    while (GetModuleHandleA("WXPMod.dll") == nullptr) {
        Sleep(500);
    }
    Log("WXPMod.dll found.");

    // Additional delay to ensure DirectX device is created
    Log("Giving the game 3 seconds to initialize DirectX...");
    Sleep(3000);

    #define INIT_HOOK(name, pattern, size) \
        Log("Scanning for " #name "..."); \
        addr_##name = Memory::FindPattern(size > 6 ? "WXPMod.dll" : "soulstorm.exe", pattern); \
        if (addr_##name) { \
            Log("Found " #name " at 0x" + std::to_string(addr_##name)); \
        } else { \
            Log("WARNING: Failed to find " #name); \
        }

    INIT_HOOK(GetPlayerBase, "3B 87 B8 01 00 00 75 19 8B CE E8 50", 6);
    INIT_HOOK(InfiniteHealth, "D9 56 14 D9 E8", 5);
    INIT_HOOK(InfiniteResources, "D9 58 04 D9 41 08", 6);
    INIT_HOOK(FastBuild, "8B 48 0C 89 4F 0C", 5);
    INIT_HOOK(InfiniteMorale, "D9 46 08 D8 64 24 10", 7);
    INIT_HOOK(InfiniteCap, "8B 28 8D 4C 24 28", 6);
    INIT_HOOK(InstantEquipment, "8B 50 08 89 57 08 8B 40 0C 89 47 0C 83", 6);
    INIT_HOOK(InstantCapture, "D9 5E 44 74 18", 5);
    INIT_HOOK(FastAbilities, "DB 46 78 D9 5C 24 08", 7);
    INIT_HOOK(RemoveFOW, "D9 81 60 0C 00 00", 6);
    INIT_HOOK(OneHitKill, "D9 46 08 D9 5C 24 18", 7);
    INIT_HOOK(AllWargear, "83 39 00 74 03", 5);

    // Skip DirectX init block to avoid scope errors
    HWND window = FindWindowA("W40kWindow", "Dawn of War: Soulstorm");
    if (!window) {
         Log("ERROR: W40kWindow not found. Cannot hook DirectX.");
         goto cleanup_and_exit;
    }

    IDirect3D9* pD3D;
    pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if(!pD3D) {
        Log("ERROR: Direct3DCreate9 failed.");
        goto cleanup_and_exit;
    }

    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;

    IDirect3DDevice9* pDummyDevice = nullptr;
    if(FAILED(pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice))) {
         Log("ERROR: CreateDevice failed.");
         pD3D->Release();
         goto cleanup_and_exit;
    }

    uintptr_t* vTable;
    vTable = (uintptr_t*)pDummyDevice;
    vTable = (uintptr_t*)vTable[0];
    uintptr_t endSceneAddr;
    endSceneAddr = vTable[42];
    pDummyDevice->Release();
    pD3D->Release();
    Log("EndScene address found at 0x" + std::to_string(endSceneAddr));

    if (MH_Initialize() != MH_OK) {
        Log("ERROR: MH_Initialize failed.");
        goto cleanup_and_exit;
    }

    if (MH_CreateHook((LPVOID)endSceneAddr, &DetourEndScene, (LPVOID*)&pOriginalEndScene) == MH_OK) {
        MH_EnableHook((LPVOID)endSceneAddr);
        Log("EndScene hooked via MinHook.");
    } else {
        Log("ERROR: Failed to hook EndScene.");
    }

    if (addr_GetPlayerBase) {
        if (MH_CreateHook((LPVOID)addr_GetPlayerBase, &DetourGetPlayerBase, (LPVOID*)&pOrigGetPlayerBase) == MH_OK) {
            MH_EnableHook((LPVOID)addr_GetPlayerBase);
            Log("Placed initial MinHook for GetPlayerBase.");
        } else {
             Log("ERROR: Failed to hook GetPlayerBase.");
        }
    } else {
        Log("WARNING: GetPlayerBase address is 0. Cheats relying on player detection will not work.");
    }

    Log("Waiting to acquire g_playerBase...");
    int wait_count = 0;
    while (g_playerBase == 0 && wait_count < 100) { // wait up to 20 seconds
        Sleep(200);
        wait_count++;
    }
    if (g_playerBase == 0) {
        Log("WARNING: Timeout waiting for g_playerBase. The game might be paused or in menus.");
    } else {
        Log("Acquired g_playerBase: 0x" + std::to_string(g_playerBase));
    }

    Log("Initializing Lua State...");
    g_LuaState = luaL_newstate();
    luaL_openlibs(g_LuaState);
    lua_register(g_LuaState, "ToggleCheat", l_ToggleCheat);
    lua_register(g_LuaState, "ImGui_Checkbox", l_ImGui_Checkbox);
    lua_register(g_LuaState, "ImGui_Begin", l_ImGui_Begin);
    lua_register(g_LuaState, "ImGui_End", l_ImGui_End);

    lua_pushnumber(g_LuaState, g_playerBase);
    lua_setglobal(g_LuaState, "playerBase");

    if (!LoadScriptFromResource("main") || !LoadScriptFromResource("cheats")) {
        Log("Failed to load embedded Lua scripts.");
    }

    Beep(750, 300);
    Log("Initialization complete. Entering hotkey loop.");

    // Prevent key repeat triggering rapidly
    bool f1_down = false;

    while (!g_uninject) {
        if (GetAsyncKeyState(VK_F1) & 0x8000) {
            if (!f1_down) {
                g_showMenu = !g_showMenu;
                Log(std::string("Menu visibility toggled: ") + (g_showMenu ? "ON" : "OFF"));
                Beep(g_showMenu ? 800 : 600, 100);
                f1_down = true;
            }
        } else {
            f1_down = false;
        }

        if (GetAsyncKeyState(VK_END) & 1) {
            Log("END key pressed. Initiating uninject.");
            g_uninject = true;
        }
        Sleep(50); // Faster polling
    }

cleanup_and_exit:
    Shutdown();
    FreeLibraryAndExitThread((HMODULE)lpParam, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, PayloadThread, hModule, 0, nullptr);
    } else if (ul_reason_for_call == DLL_PROCESS_DETACH) {
        // Only run shutdown if it wasn't already triggered by the END key
        if(!g_uninject) Shutdown();
    }
    return TRUE;
}
