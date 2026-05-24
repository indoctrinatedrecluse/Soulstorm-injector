#include <windows.h>
#include <string>
#include <vector>
#include <d3d9.h>

#include "scanner.h"
#include "MinHook.h"
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
HMODULE g_hModule = nullptr;

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

    __try {
        if (*(uintptr_t*)(entityOwner + 0x40) == g_playerBase) return;
    } __except(EXCEPTION_EXECUTE_HANDLER) {}

    pOrigHealthUpdate(ecx, edx, damage);
}

void __fastcall DetourResourceUpdate(void* ecx, void* edx, void* resource_struct) {
    uintptr_t resourceOwner = 0;
    __asm { mov resourceOwner, esi }

    __try {
        if (resourceOwner == g_playerBase) {
            if (Cheats::infinite_resources) {
                *(float*)((uintptr_t)ecx + 0x00) = 99999.0f; // Req
                *(float*)((uintptr_t)ecx + 0x04) = 99999.0f; // Power
            }
            if (Cheats::infinite_faith) *(float*)((uintptr_t)ecx + 0x0C) = 99999.0f;
            if (Cheats::infinite_souls) *(float*)((uintptr_t)ecx + 0x10) = 99999.0f;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}

    pOrigResourceUpdate(ecx, edx, resource_struct);
}

void __fastcall DetourBuildTimeUpdate(void* ecx, void* edx, void* build_struct) {
    __try { *(float*)((uintptr_t)ecx + 0x0C) = 0.0f; } __except(EXCEPTION_EXECUTE_HANDLER) {}
    pOrigBuildTimeUpdate(ecx, edx, build_struct);
}

void __fastcall DetourMoraleUpdate(void* ecx, void* edx, void* esp_plus_10) {
    uintptr_t entityOwner = 0;
    __asm { mov entityOwner, ebp }

    __try {
        if (*(uintptr_t*)(entityOwner + 0x10) == g_playerBase) {
            *(float*)((uintptr_t)ecx + 0x08) = 1.0f;
            return;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}

    pOrigMoraleUpdate(ecx, edx, esp_plus_10);
}

void __fastcall DetourSquadCapUpdate(void* ecx, void* edx) {
    __try { *(float*)ecx = 0.0f; } __except(EXCEPTION_EXECUTE_HANDLER) {}
    pOrigSquadCapUpdate(ecx, edx);
}

void __fastcall DetourEquipTimeUpdate(void* ecx, void* edx) {
    __try { *(float*)((uintptr_t)ecx + 0x08) = 0.0f; } __except(EXCEPTION_EXECUTE_HANDLER) {}
    pOrigEquipTimeUpdate(ecx, edx);
}

void __fastcall DetourFogOfWarUpdate(void* ecx, void* edx) {
    __try { *(float*)((uintptr_t)ecx + 0xC58) = 0.1f; } __except(EXCEPTION_EXECUTE_HANDLER) {}
    pOrigFogOfWarUpdate(ecx, edx);
}

void __fastcall DetourOneHitKill(void* ecx, void* edx, void* arg1) {
     uintptr_t entityOwner = 0;
    __asm { mov entityOwner, esi }

    __try {
        if (*(uintptr_t*)(entityOwner + 0x40) != g_playerBase) {
            *(float*)((uintptr_t)ecx + 0x08) = 90000.0f;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    pOrigOneHitKill(ecx, edx, arg1);
}

void __fastcall DetourInstantCapture(void* ecx, void* edx) {
    uintptr_t entityOwner = 0;
    __asm { mov entityOwner, esi }

    __try {
        if (*(uintptr_t*)(entityOwner + 0x40) == g_playerBase) {
            *(float*)((uintptr_t)ecx + 0x44) = 360.0f;
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    pOrigInstantCapture(ecx, edx);
}

void __fastcall DetourFastAbilities(void* ecx, void* edx) {
     __try { *(float*)((uintptr_t)ecx + 0x78) = 0.0f; } __except(EXCEPTION_EXECUTE_HANDLER) {}
     pOrigFastAbilities(ecx, edx);
}

void __fastcall DetourAllWargear(void* ecx, void* edx) {
    pOrigAllWargear(ecx, edx);
}

// --- Lua Bridge ---
void ToggleHook(uintptr_t address, void* detour, void** original, bool enable) {
    if (address == 0) return;
    if (enable) {
        if (*original == nullptr) {
            MH_CreateHook((LPVOID)address, detour, original);
        }
        MH_EnableHook((LPVOID)address);
    } else {
        MH_DisableHook((LPVOID)address);
    }
}

static int l_ToggleCheat(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    bool enable = lua_toboolean(L, 2);

    if (strcmp(name, "infinite_health") == 0) ToggleHook(addr_InfiniteHealth, DetourHealthUpdate, (void**)&pOrigHealthUpdate, enable);
    else if (strcmp(name, "infinite_resources") == 0) { Cheats::infinite_resources = enable; ToggleHook(addr_InfiniteResources, DetourResourceUpdate, (void**)&pOrigResourceUpdate, enable || Cheats::infinite_faith || Cheats::infinite_souls); }
    else if (strcmp(name, "infinite_faith") == 0) { Cheats::infinite_faith = enable; ToggleHook(addr_InfiniteResources, DetourResourceUpdate, (void**)&pOrigResourceUpdate, enable || Cheats::infinite_resources || Cheats::infinite_souls); }
    else if (strcmp(name, "infinite_souls") == 0) { Cheats::infinite_souls = enable; ToggleHook(addr_InfiniteResources, DetourResourceUpdate, (void**)&pOrigResourceUpdate, enable || Cheats::infinite_resources || Cheats::infinite_faith); }
    else if (strcmp(name, "fast_build") == 0) ToggleHook(addr_FastBuild, DetourBuildTimeUpdate, (void**)&pOrigBuildTimeUpdate, enable);
    else if (strcmp(name, "infinite_morale") == 0) ToggleHook(addr_InfiniteMorale, DetourMoraleUpdate, (void**)&pOrigMoraleUpdate, enable);
    else if (strcmp(name, "infinite_cap") == 0) ToggleHook(addr_InfiniteCap, DetourSquadCapUpdate, (void**)&pOrigSquadCapUpdate, enable);
    else if (strcmp(name, "instant_equip") == 0) ToggleHook(addr_InstantEquipment, DetourEquipTimeUpdate, (void**)&pOrigEquipTimeUpdate, enable);
    else if (strcmp(name, "instant_capture") == 0) ToggleHook(addr_InstantCapture, DetourInstantCapture, (void**)&pOrigInstantCapture, enable);
    else if (strcmp(name, "fast_abilities") == 0) ToggleHook(addr_FastAbilities, DetourFastAbilities, (void**)&pOrigFastAbilities, enable);
    else if (strcmp(name, "remove_fow") == 0) ToggleHook(addr_RemoveFOW, DetourFogOfWarUpdate, (void**)&pOrigFogOfWarUpdate, enable);
    else if (strcmp(name, "one_hit_kill") == 0) ToggleHook(addr_OneHitKill, DetourOneHitKill, (void**)&pOrigOneHitKill, enable);
    else if (strcmp(name, "all_wargear") == 0) ToggleHook(addr_AllWargear, DetourAllWargear, (void**)&pOrigAllWargear, enable);

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
        g_window = FindWindowA("W40kWindow", "Dawn of War: Soulstorm");
        ImGui::CreateContext();
        ImGui_ImplWin32_Init(g_window);
        ImGui_ImplDX9_Init(pDevice);
        pOriginalWndProc = (WNDPROC)SetWindowLongPtr(g_window, GWLP_WNDPROC, (LONG_PTR)DetourWndProc);
        init = true;
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (g_showMenu) {
        lua_getglobal(g_LuaState, "on_draw_frame");
        if (lua_isfunction(g_LuaState, -1)) lua_pcall(g_LuaState, 0, 0, 0);
    }

    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return pOriginalEndScene(pDevice);
}

void Shutdown() {
    if (pOriginalWndProc) {
        SetWindowLongPtr(g_window, GWLP_WNDPROC, (LONG_PTR)pOriginalWndProc);
    }

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (g_LuaState) lua_close(g_LuaState);
}

bool LoadScriptFromResource(const char* scriptName) {
    HRSRC hRes = FindResource(g_hModule, scriptName, RT_RCDATA);
    if (!hRes) return false;
    HGLOBAL hResLoad = LoadResource(g_hModule, hRes);
    if (!hResLoad) return false;
    char* scriptData = (char*)LockResource(hResLoad);
    size_t scriptSize = SizeofResource(g_hModule, hRes);
    if (scriptData && scriptSize > 0) {
        if (luaL_loadbuffer(g_LuaState, scriptData, scriptSize, scriptName) == LUA_OK) {
            return lua_pcall(g_LuaState, 0, 0, 0) == LUA_OK;
        }
    }
    return false;
}

DWORD WINAPI PayloadThread(LPVOID lpParam) {
    // Find addresses
    addr_GetPlayerBase = Memory::FindPattern("WXPMod.dll", "3B 87 B8 01 00 00 75 19 8B CE E8 50");
    addr_InfiniteHealth = Memory::FindPattern("WXPMod.dll", "D9 56 14 D9 E8");
    addr_InfiniteResources = Memory::FindPattern("WXPMod.dll", "D9 58 04 D9 41 08");
    addr_FastBuild = Memory::FindPattern("WXPMod.dll", "8B 48 0C 89 4F 0C");
    addr_InfiniteMorale = Memory::FindPattern("WXPMod.dll", "D9 46 08 D8 64 24 10");
    addr_InfiniteCap = Memory::FindPattern("WXPMod.dll", "8B 28 8D 4C 24 28");
    addr_InstantEquipment = Memory::FindPattern("WXPMod.dll", "8B 50 08 89 57 08 8B 40 0C 89 47 0C 83");
    addr_InstantCapture = Memory::FindPattern("WXPMod.dll", "D9 5E 44 74 18");
    addr_FastAbilities = Memory::FindPattern("WXPMod.dll", "DB 46 78 D9 5C 24 08");
    addr_RemoveFOW = Memory::FindPattern("soulstorm.exe", "D9 81 60 0C 00 00");
    addr_OneHitKill = Memory::FindPattern("WXPMod.dll", "D9 46 08 D9 5C 24 18");
    addr_AllWargear = Memory::FindPattern("WXPMod.dll", "83 39 00 74 03");

    // Hook EndScene for drawing
    HWND window = FindWindowA("W40kWindow", "Dawn of War: Soulstorm");
    IDirect3D9* pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS d3dpp = {};
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    IDirect3DDevice9* pDummyDevice = nullptr;
    pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &pDummyDevice);
    uintptr_t* vTable = (uintptr_t*)pDummyDevice;
    vTable = (uintptr_t*)vTable[0];
    uintptr_t endSceneAddr = vTable[42];
    pDummyDevice->Release();
    pD3D->Release();

    if (MH_Initialize() != MH_OK) return 1;
    MH_CreateHook((LPVOID)endSceneAddr, &DetourEndScene, (LPVOID*)&pOriginalEndScene);
    MH_EnableHook((LPVOID)endSceneAddr);

    if (addr_GetPlayerBase) {
        MH_CreateHook((LPVOID)addr_GetPlayerBase, &DetourGetPlayerBase, (LPVOID*)&pOrigGetPlayerBase);
        MH_EnableHook((LPVOID)addr_GetPlayerBase);
    }

    while (g_playerBase == 0) Sleep(200);

    g_LuaState = luaL_newstate();
    luaL_openlibs(g_LuaState);
    lua_register(g_LuaState, "ToggleCheat", l_ToggleCheat);
    lua_register(g_LuaState, "ImGui_Checkbox", l_ImGui_Checkbox);
    lua_register(g_LuaState, "ImGui_Begin", l_ImGui_Begin);
    lua_register(g_LuaState, "ImGui_End", l_ImGui_End);

    if (!LoadScriptFromResource("main") || !LoadScriptFromResource("cheats")) {
        MessageBoxA(NULL, "Failed to load Lua scripts from resource.", "Lua Error", MB_OK);
    }

    Beep(750, 300);

    while (!g_uninject) {
        if (GetAsyncKeyState(VK_INSERT) & 1) g_showMenu = !g_showMenu;
        if (GetAsyncKeyState(VK_END) & 1) g_uninject = true;
        Sleep(100);
    }

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
        Shutdown();
    }
    return TRUE;
}