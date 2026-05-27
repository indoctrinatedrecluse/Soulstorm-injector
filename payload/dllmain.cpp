#include <windows.h>
#include <string>
#include <vector>
#include <d3d9.h>
#include <map>
#include <fstream>
#include <chrono>
#include <iomanip>

#include "scanner.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "d3d9.lib")

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
WNDPROC pOriginalWndProc = nullptr;
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

// --- Hook Management ---
struct HookInfo {
    void* detourFunction;
    uintptr_t returnAddress;
    uintptr_t branchAddress;
    size_t instructionSize;
    std::vector<BYTE> originalBytes;
};
std::map<std::string, HookInfo> g_hooks;

// --- Cheat States ---
namespace Cheats {
    bool infinite_health = false;
    bool infinite_resources = false;
    bool infinite_faith = false;
    bool infinite_souls = false;
    bool fast_build = false;
    bool infinite_morale = false;
    bool infinite_cap = false;
    bool instant_equip = false;
    bool remove_fow = false;
    bool one_hit_kill = false;
    bool instant_capture = false;
    bool fast_abilities = false;
    bool all_wargear = false;
}

// --- Hook Targets ---
uintptr_t addr_GetPlayerBase = 0;
uintptr_t ret_GetPlayerBase = 0;
uintptr_t branch_GetPlayerBase = 0;

uintptr_t addr_InfiniteHealth = 0;
uintptr_t ret_InfiniteHealth = 0;
uintptr_t branch_InfiniteHealth = 0;

uintptr_t addr_InfiniteResources = 0;
uintptr_t ret_InfiniteResources = 0;
uintptr_t branch_InfiniteResources = 0;

uintptr_t addr_FastBuild = 0;
uintptr_t ret_FastBuild = 0;
uintptr_t branch_FastBuild = 0;

uintptr_t addr_InfiniteMorale = 0;
uintptr_t ret_InfiniteMorale = 0;
uintptr_t branch_InfiniteMorale = 0;

uintptr_t addr_InfiniteCap = 0;
uintptr_t ret_InfiniteCap = 0;
uintptr_t branch_InfiniteCap = 0;

uintptr_t addr_InstantEquipment = 0;
uintptr_t ret_InstantEquipment = 0;
uintptr_t branch_InstantEquipment = 0;

uintptr_t addr_InstantCapture = 0;
uintptr_t ret_InstantCapture = 0;
uintptr_t branch_InstantCapture = 0;

uintptr_t addr_FastAbilities = 0;
uintptr_t ret_FastAbilities = 0;
uintptr_t branch_FastAbilities = 0;

uintptr_t addr_RemoveFOW = 0;
uintptr_t ret_RemoveFOW = 0;
uintptr_t branch_RemoveFOW = 0;

uintptr_t addr_OneHitKill = 0;
uintptr_t ret_OneHitKill = 0;
uintptr_t branch_OneHitKill = 0;

uintptr_t addr_AllWargear = 0;
uintptr_t ret_AllWargear = 0;
uintptr_t branch_AllWargear = 0;

// --- Detour Implementations ---
void __declspec(naked) detour_GetPlayerBase() {
    __asm {
        mov g_playerBase, ebx
        cmp eax, [edi+0x1B8]
        jmp [ret_GetPlayerBase]
    }
}

void __declspec(naked) detour_FastBuild() {
    __asm {
        __try {
            mov dword ptr [eax+0x0C], 0
            jmp [ret_FastBuild]
        } __except (EXCEPTION_EXECUTE_HANDLER) {
             jmp [ret_FastBuild]
        }
    }
}

void __declspec(naked) detour_InfiniteCap() {
    __asm {
         __try {
            mov dword ptr [eax], 0
            mov ebp, [eax]
            lea ecx, [esp+0x28]
            jmp [ret_InfiniteCap]
         } __except (EXCEPTION_EXECUTE_HANDLER) {
             jmp [ret_InfiniteCap]
         }
    }
}

void __declspec(naked) detour_InstantEquipment() {
    __asm {
         __try {
            mov dword ptr [eax+0x08], 0
            jmp [ret_InstantEquipment]
         } __except (EXCEPTION_EXECUTE_HANDLER) {
             jmp [ret_InstantEquipment]
         }
    }
}

void __declspec(naked) detour_FastAbilities() {
    __asm {
         __try {
            mov dword ptr [esi+0x78], 0
            jmp [ret_FastAbilities]
         } __except (EXCEPTION_EXECUTE_HANDLER) {
             jmp [ret_FastAbilities]
         }
    }
}

void __declspec(naked) detour_RemoveFOW() {
    __asm {
         __try {
            mov dword ptr [ecx+0xC58], 0x3dcccccd
            jmp [ret_RemoveFOW]
         } __except (EXCEPTION_EXECUTE_HANDLER) {
             jmp [ret_RemoveFOW]
         }
    }
}

void __declspec(naked) detour_AllWargear() {
    __asm {
        nop
        nop
        nop
        jmp [ret_AllWargear]
    }
}

void __declspec(naked) detour_InfiniteHealth() {
    __asm {
        __try {
            mov ecx, [g_playerBase]
            cmp ebp, ecx
            jne orig_InfiniteHealth
            mov [esi+0x14], 1
            jmp [ret_InfiniteHealth]
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            jmp orig_InfiniteHealth
        }
    orig_InfiniteHealth:
        fst dword ptr [esi+0x14]
        fld1
        jmp [ret_InfiniteHealth]
    }
}

void __declspec(naked) detour_InfiniteResources() {
    __asm {
        __try {
            cmp esi, [g_playerBase]
            jne orig_InfiniteResources

            mov dword ptr [eax], 0x49742400
            mov dword ptr [eax+0x04], 0x49742400
            mov dword ptr [eax+0x0C], 0x49742400
            mov dword ptr [eax+0x10], 0x49742400
            jmp [ret_InfiniteResources]
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            jmp orig_InfiniteResources
        }
    orig_InfiniteResources:
        fstp dword ptr [eax+0x04]
        fld dword ptr [ecx+0x08]
        jmp [ret_InfiniteResources]
    }
}

void __declspec(naked) detour_InfiniteMorale() {
    __asm {
        __try {
            mov eax, [ebp+0x10]
            cmp eax, [g_playerBase]
            jne orig_InfiniteMorale
            fld dword ptr [esi+0x08]
            mov dword ptr [esi+0x08], 0x3F800000
            jmp [ret_InfiniteMorale]
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            jmp orig_InfiniteMorale
        }
    orig_InfiniteMorale:
        fld dword ptr [esi+0x08]
        fsub dword ptr [esp+0x10]
        jmp [ret_InfiniteMorale]
    }
}

void __declspec(naked) detour_InstantCapture() {
    __asm {
        __try {
            mov edx, [esi+0x40]
            cmp edx, [g_playerBase]
            jne orig_InstantCapture
            mov dword ptr [esi+0x44], 0x43B40000
            jmp [ret_InstantCapture]
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            jmp orig_InstantCapture
        }
    orig_InstantCapture:
        fstp dword ptr [esi+0x44]
        je branch_InstantCapture_jump
        jmp [ret_InstantCapture]
    branch_InstantCapture_jump:
        jmp [branch_InstantCapture]
    }
}

void __declspec(naked) detour_OneHitKill() {
    __asm {
        __try {
            fld dword ptr [esi+0x08]
            fstp dword ptr [esp+0x18]
            mov dword ptr [esi+0x08], 0x47AF0000
            jmp [ret_OneHitKill]
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            jmp [ret_OneHitKill]
        }
    }
}

// --- Lua Bridge ---
static int l_ToggleCheat(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    bool enable = lua_toboolean(L, 2);

    Log(std::string("Lua requested toggle: ") + name + " -> " + (enable ? "ON" : "OFF"));

    if (g_hooks.find(name) != g_hooks.end()) {
        uintptr_t address = Memory::FindPattern(g_hooks[name].originalBytes.size() > 6 ? "WXPMod.dll" : "soulstorm.exe", (const char*)g_hooks[name].originalBytes.data());
        if(address == 0) {
            Log(std::string("Failed to toggle cheat ") + name + ": AOB pattern not found in memory.");
            return 0;
        }

        if (enable) {
            Log(std::string("Placing JMP hook for ") + name + " at 0x" + std::to_string(address));
            Memory::PlaceJmp(address, g_hooks[name].detourFunction, g_hooks[name].instructionSize, &g_hooks[name].returnAddress, &g_hooks[name].branchAddress);
            Beep(600, 100);
        } else {
            Log(std::string("Restoring original bytes for ") + name + " at 0x" + std::to_string(address));
            Memory::RestoreJmp(address, g_hooks[name].instructionSize, g_hooks[name].originalBytes.data());
            Beep(400, 100);
        }
    } else {
        Log(std::string("Cheat '") + name + "' not found in hook dictionary.");
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
typedef long(__stdcall* EndScene_t)(IDirect3DDevice9* pDevice);
EndScene_t pOriginalEndScene = nullptr;

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

    for(auto const& [name, hook] : g_hooks) {
        uintptr_t address = Memory::FindPattern(hook.originalBytes.size() > 6 ? "WXPMod.dll" : "soulstorm.exe", (const char*)hook.originalBytes.data());
        if(address) {
             Memory::RestoreJmp(address, hook.instructionSize, hook.originalBytes.data());
             Log("Restored bytes for hook: " + name);
        }
    }

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    Log("ImGui shut down.");

    if (g_LuaState) {
        lua_close(g_LuaState);
        Log("Lua state closed.");
    }

    if (g_logFile.is_open()) g_logFile.close();
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

#include "MinHook.h"

DWORD WINAPI PayloadThread(LPVOID lpParam) {
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
            g_hooks[#name] = {detour_##name, 0, 0, size, {}}; \
            g_hooks[#name].originalBytes.assign((BYTE*)addr_##name, (BYTE*)addr_##name + size); \
        } else { \
            Log("Failed to find " #name); \
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

    Log("Creating dummy D3D9 device for hooking EndScene...");
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

    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    IDirect3DDevice9* pDummyDevice;
    pDummyDevice = nullptr;
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
        Memory::PlaceJmp(addr_GetPlayerBase, detour_GetPlayerBase, 6, &ret_GetPlayerBase);
        Log("Placed initial JMP for GetPlayerBase.");
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
