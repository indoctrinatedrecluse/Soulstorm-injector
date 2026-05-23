#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <d3d9.h>
#include <d3dx9.h>

#include "scanner.h"
#include "MinHook.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

// Lua headers
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

// --- Globals ---
uintptr_t g_playerBase = 0;
lua_State* g_LuaState = nullptr;
bool g_uninject = false;
ID3DXFont* g_pFont = nullptr;

// --- Hook Function Pointer Typedefs ---
typedef int(__thiscall* PlayerBaseFn_t)(void* this_ptr, void* unused, void* arg1);
typedef void(__thiscall* HealthUpdateFn_t)(void* this_ptr, void* unused, float damage);
typedef void(__thiscall* ResourceUpdateFn_t)(void* this_ptr, void* unused, void* resource_struct);
typedef void(__thiscall* BuildTimeUpdateFn_t)(void* this_ptr, void* unused, void* build_struct);
typedef void(__thiscall* MoraleUpdateFn_t)(void* this_ptr, void* unused, void* esp_plus_10);
typedef void(__thiscall* SquadCapUpdateFn_t)(void* this_ptr, void* unused);
typedef void(__thiscall* EquipTimeUpdateFn_t)(void* this_ptr, void* unused);
typedef void(__thiscall* FogOfWarUpdateFn_t)(void* this_ptr, void* unused);
typedef long(__stdcall* EndScene_t)(IDirect3DDevice9* pDevice);

// --- Original Function Pointers ---
PlayerBaseFn_t pOriginalGetPlayerBase = nullptr;
HealthUpdateFn_t pOriginalHealthUpdate = nullptr;
ResourceUpdateFn_t pOriginalResourceUpdate = nullptr;
BuildTimeUpdateFn_t pOriginalBuildTimeUpdate = nullptr;
MoraleUpdateFn_t pOriginalMoraleUpdate = nullptr;
SquadCapUpdateFn_t pOriginalSquadCapUpdate = nullptr;
EquipTimeUpdateFn_t pOriginalEquipTimeUpdate = nullptr;
FogOfWarUpdateFn_t pOriginalFogOfWarUpdate = nullptr;
EndScene_t pOriginalEndScene = nullptr;

// --- Hook Targets ---
uintptr_t g_addrGetPlayerBase = 0;
uintptr_t g_addrHealthUpdate = 0;
uintptr_t g_addrResourceUpdate = 0;
uintptr_t g_addrBuildTimeUpdate = 0;
uintptr_t g_addrMoraleUpdate = 0;
uintptr_t g_addrSquadCapUpdate = 0;
uintptr_t g_addrEquipTimeUpdate = 0;
uintptr_t g_addrFogOfWarUpdate = 0;

// --- Detour Functions ---
int __fastcall DetourGetPlayerBase(void* this_ptr, void* edx, void* arg1) {
    __asm { mov g_playerBase, ebx }
    return pOriginalGetPlayerBase(this_ptr, edx, arg1);
}

void __fastcall DetourHealthUpdate(void* this_ptr, void* edx, float damage) {
    uintptr_t entityOwner = 0;
    __asm { mov entityOwner, esi }
    if (*(uintptr_t*)(entityOwner + 0x40) == g_playerBase) return;
    pOriginalHealthUpdate(this_ptr, edx, damage);
}

void __fastcall DetourResourceUpdate(void* this_ptr, void* edx, void* resource_struct) {
    uintptr_t resourceOwner = 0;
    __asm { mov resourceOwner, esi }
    if (resourceOwner == g_playerBase) {
        *(float*)((uintptr_t)this_ptr + 0x00) = 99999.0f; // Requisition
        *(float*)((uintptr_t)this_ptr + 0x04) = 99999.0f; // Power
        *(float*)((uintptr_t)this_ptr + 0x0C) = 99999.0f; // Faith/Souls
        return;
    }
    pOriginalResourceUpdate(this_ptr, edx, resource_struct);
}

void __fastcall DetourBuildTimeUpdate(void* this_ptr, void* edx, void* build_struct) {
    *(float*)((uintptr_t)this_ptr + 0x0C) = 0.0f;
    pOriginalBuildTimeUpdate(this_ptr, edx, build_struct);
}

void __fastcall DetourMoraleUpdate(void* this_ptr, void* edx, void* esp_plus_10) {
    uintptr_t entityOwner = 0;
    __asm { mov entityOwner, ebp }
    if (*(uintptr_t*)(entityOwner + 0x10) == g_playerBase) {
        *(float*)((uintptr_t)this_ptr + 0x08) = 1.0f; // Set morale to max
        return;
    }
    pOriginalMoraleUpdate(this_ptr, edx, esp_plus_10);
}

void __fastcall DetourSquadCapUpdate(void* this_ptr, void* edx) {
    *(float*)this_ptr = 0.0f; // Set current cap usage to 0
    pOriginalSquadCapUpdate(this_ptr, edx);
}

void __fastcall DetourEquipTimeUpdate(void* this_ptr, void* edx) {
    *(float*)((uintptr_t)this_ptr + 0x08) = 0.0f; // Set equipment time to 0
    pOriginalEquipTimeUpdate(this_ptr, edx);
}

void __fastcall DetourFogOfWarUpdate(void* this_ptr, void* edx) {
    *(float*)((uintptr_t)this_ptr + 0xC58) = 0.1f; // A value from the CT
    pOriginalFogOfWarUpdate(this_ptr, edx);
}

// --- Lua-to-C++ Bridge ---
void ToggleHookByName(const char* name, uintptr_t* pAddress, void* pDetour, void* pOriginal, bool enable) {
    if (*pAddress == 0) return;
    if (enable) {
        if (MH_CreateHook((LPVOID)*pAddress, pDetour, (LPVOID*)pOriginal) != MH_OK) return;
        MH_EnableHook((LPVOID)*pAddress);
    } else {
        MH_DisableHook((LPVOID)*pAddress);
    }
}

#define TOGGLE_CHEAT_FUNC(name, addr, detour, original) \
static int l_Toggle_##name(lua_State* L) { \
    bool enable = lua_toboolean(L, 1); \
    ToggleHookByName(#name, &addr, detour, &original, enable); \
    return 0; \
}

TOGGLE_CHEAT_FUNC(infinite_health, g_addrHealthUpdate, DetourHealthUpdate, pOriginalHealthUpdate)
TOGGLE_CHEAT_FUNC(infinite_resources, g_addrResourceUpdate, DetourResourceUpdate, pOriginalResourceUpdate)
TOGGLE_CHEAT_FUNC(fast_build, g_addrBuildTimeUpdate, DetourBuildTimeUpdate, pOriginalBuildTimeUpdate)
TOGGLE_CHEAT_FUNC(infinite_morale, g_addrMoraleUpdate, DetourMoraleUpdate, pOriginalMoraleUpdate)
TOGGLE_CHEAT_FUNC(infinite_cap, g_addrSquadCapUpdate, DetourSquadCapUpdate, pOriginalSquadCapUpdate)
TOGGLE_CHEAT_FUNC(instant_equip, g_addrEquipTimeUpdate, DetourEquipTimeUpdate, pOriginalEquipTimeUpdate)
TOGGLE_CHEAT_FUNC(remove_fow, g_addrFogOfWarUpdate, DetourFogOfWarUpdate, pOriginalFogOfWarUpdate)

static int l_draw_text(lua_State* L) {
    if (!g_pFont) return 0;
    int x = lua_tointeger(L, 1);
    int y = lua_tointeger(L, 2);
    const char* text = lua_tostring(L, 3);
    RECT rect = { x, y, x, y };
    g_pFont->DrawTextA(NULL, text, -1, &rect, DT_NOCLIP, D3DCOLOR_ARGB(255, 255, 255, 0));
    return 0;
}

// --- Main Payload & Hooks ---
long __stdcall DetourEndScene(IDirect3DDevice9* pDevice) {
    if (!g_pFont) {
        D3DXCreateFont(pDevice, 18, 0, FW_BOLD, 1, false, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Arial", &g_pFont);
    }

    lua_getglobal(g_LuaState, "on_render");
    if (lua_isfunction(g_LuaState, -1)) {
        lua_pcall(g_LuaState, 0, 0, 0);
    }

    return pOriginalEndScene(pDevice);
}

DWORD WINAPI PayloadThread(LPVOID lpParam) {
    // Find addresses
    g_addrGetPlayerBase = Memory::FindPattern("WXPMod.dll", "3B 87 B8 01 00 00 75 19 8B CE E8 50");
    g_addrHealthUpdate = Memory::FindPattern("WXPMod.dll", "D9 56 14 D9 E8");
    g_addrResourceUpdate = Memory::FindPattern("WXPMod.dll", "D9 58 04 D9 41 08");
    g_addrBuildTimeUpdate = Memory::FindPattern("WXPMod.dll", "8B 48 0C 89 4F 0C");
    g_addrMoraleUpdate = Memory::FindPattern("WXPMod.dll", "D9 46 08 D8 64 24 10");
    g_addrSquadCapUpdate = Memory::FindPattern("WXPMod.dll", "8B 28 8D 4C 24 28");
    g_addrEquipTimeUpdate = Memory::FindPattern("WXPMod.dll", "8B 50 08 89 57 08 8B 40 0C 89 47 0C 83");
    g_addrFogOfWarUpdate = Memory::FindPattern("soulstorm.exe", "D9 81 60 0C 00 00");

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

    // Initialize MinHook
    if (MH_Initialize() != MH_OK) return 1;

    // Create all hooks (but don't enable cheat hooks yet)
    MH_CreateHook((LPVOID)g_addrGetPlayerBase, &DetourGetPlayerBase, (LPVOID*)&pOriginalGetPlayerBase);
    MH_EnableHook((LPVOID)g_addrGetPlayerBase);
    MH_CreateHook((LPVOID)endSceneAddr, &DetourEndScene, (LPVOID*)&pOriginalEndScene);
    MH_EnableHook((LPVOID)endSceneAddr);

    while (g_playerBase == 0) Sleep(200);

    // Initialize Lua
    g_LuaState = luaL_newstate();
    luaL_openlibs(g_LuaState);
    lua_register(g_LuaState, "Toggle_infinite_health", l_Toggle_infinite_health);
    lua_register(g_LuaState, "Toggle_infinite_resources", l_Toggle_infinite_resources);
    lua_register(g_LuaState, "Toggle_fast_build", l_Toggle_fast_build);
    lua_register(g_LuaState, "Toggle_infinite_morale", l_Toggle_infinite_morale);
    lua_register(g_LuaState, "Toggle_infinite_cap", l_Toggle_infinite_cap);
    lua_register(g_LuaState, "Toggle_instant_equip", l_Toggle_instant_equip);
    lua_register(g_LuaState, "Toggle_remove_fow", l_Toggle_remove_fow);
    lua_register(g_LuaState, "draw_text", l_draw_text);

    lua_pushnumber(g_LuaState, g_playerBase);
    lua_setglobal(g_LuaState, "playerBase");

    if (luaL_dofile(g_LuaState, "F:/Projects/Soulstorm-injector/payload/lua/main.lua") != LUA_OK) {
        MessageBoxA(NULL, lua_tostring(g_LuaState, -1), "Lua Error", MB_OK);
    }

    Beep(750, 300);

    // Main loop for hotkeys
    while (!g_uninject) {
        if (GetAsyncKeyState(VK_F2) & 1) { lua_getglobal(g_LuaState, "ToggleCheat"); lua_pushstring(g_LuaState, "infinite_health"); lua_pcall(g_LuaState, 1, 0, 0); }
        if (GetAsyncKeyState(VK_F3) & 1) { lua_getglobal(g_LuaState, "ToggleCheat"); lua_pushstring(g_LuaState, "infinite_resources"); lua_pcall(g_LuaState, 1, 0, 0); }
        if (GetAsyncKeyState(VK_F4) & 1) { lua_getglobal(g_LuaState, "ToggleCheat"); lua_pushstring(g_LuaState, "fast_build"); lua_pcall(g_LuaState, 1, 0, 0); }
        if (GetAsyncKeyState(VK_F5) & 1) { lua_getglobal(g_LuaState, "ToggleCheat"); lua_pushstring(g_LuaState, "infinite_morale"); lua_pcall(g_LuaState, 1, 0, 0); }
        if (GetAsyncKeyState(VK_F6) & 1) { lua_getglobal(g_LuaState, "ToggleCheat"); lua_pushstring(g_LuaState, "infinite_cap"); lua_pcall(g_LuaState, 1, 0, 0); }
        if (GetAsyncKeyState(VK_F7) & 1) { lua_getglobal(g_LuaState, "ToggleCheat"); lua_pushstring(g_LuaState, "instant_equip"); lua_pcall(g_LuaState, 1, 0, 0); }
        if (GetAsyncKeyState(VK_F8) & 1) { lua_getglobal(g_LuaState, "ToggleCheat"); lua_pushstring(g_LuaState, "remove_fow"); lua_pcall(g_LuaState, 1, 0, 0); }
        if (GetAsyncKeyState(VK_END) & 1) { g_uninject = true; }
        Sleep(100);
    }

    // Cleanup
    Beep(500, 300);
    if (g_pFont) g_pFont->Release();
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    lua_close(g_LuaState);

    FreeLibraryAndExitThread((HMODULE)lpParam, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, PayloadThread, hModule, 0, nullptr);
    }
    return TRUE;
}
