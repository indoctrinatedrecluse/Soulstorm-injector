#include <windows.h>
#include <string>
#include <vector>
#include <d3d9.h>

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
HMODULE g_hModule = nullptr; // Handle to our own DLL

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

// --- Hook & Detour Declarations ---
#define DECLARE_HOOK(name, aob, size) \
    uintptr_t addr_##name = 0; \
    uintptr_t ret_##name = 0; \
    uintptr_t branch_##name = 0; \
    void __declspec(naked) detour_##name()

// --- Hooks ---
DECLARE_HOOK(GetPlayerBase, "3B 87 B8 01 00 00 75 19 8B CE E8 50", 6);
DECLARE_HOOK(InfiniteHealth, "D9 56 14 D9 E8", 5);
DECLARE_HOOK(InfiniteResources, "D9 58 04 D9 41 08", 6);
DECLARE_HOOK(FastBuild, "8B 48 0C 89 4F 0C", 5);
DECLARE_HOOK(InfiniteMorale, "D9 46 08 D8 64 24 10", 7);
DECLARE_HOOK(InfiniteCap, "8B 28 8D 4C 24 28", 6);
DECLARE_HOOK(InstantEquipment, "8B 50 08 89 57 08 8B 40 0C 89 47 0C 83", 6);
DECLARE_HOOK(InstantCapture, "D9 5E 44 74 18", 5);
DECLARE_HOOK(FastAbilities, "DB 46 78 D9 5C 24 08", 7);
DECLARE_HOOK(RemoveFOW, "D9 81 60 0C 00 00", 6);
DECLARE_HOOK(OneHitKill, "D9 46 08 D9 5C 24 18", 7);
DECLARE_HOOK(AllWargear, "83 39 00 74 03", 5);

// --- Detour Implementations ---
void __declspec(naked) detour_GetPlayerBase() { __asm { mov g_playerBase, ebx; cmp eax, [edi+0x1B8]; jmp ret_GetPlayerBase; } }
void __declspec(naked) detour_FastBuild() { __asm { mov dword ptr [eax+0x0C], 0; jmp ret_FastBuild; } }
void __declspec(naked) detour_InfiniteCap() { __asm { mov dword ptr [eax], 0; mov ebp, [eax]; lea ecx, [esp+0x28]; jmp ret_InfiniteCap; } }
void __declspec(naked) detour_InstantEquipment() { __asm { mov dword ptr [eax+0x08], 0; jmp ret_InstantEquipment; } }
void __declspec(naked) detour_FastAbilities() { __asm { mov dword ptr [esi+0x78], 0; jmp ret_FastAbilities; } }
void __declspec(naked) detour_RemoveFOW() { __asm { mov dword ptr [ecx+0xC58], 0x3dcccccd; jmp ret_RemoveFOW; } }
void __declspec(naked) detour_AllWargear() { __asm { nop; nop; nop; jmp ret_AllWargear; } }

void __declspec(naked) detour_InfiniteHealth() {
    __asm {
        mov ecx, [g_playerBase]
        cmp ebp, ecx
        jne original_code
        mov [esi+0x14], 1
        jmp ret_InfiniteHealth
    original_code:
        fst dword ptr [esi+0x14]
        fld1
        jmp ret_InfiniteHealth
    }
}

void __declspec(naked) detour_InfiniteResources() {
    __asm {
        cmp esi, [g_playerBase]
        jne original_code

        mov dword ptr [eax], 0x49742400
        mov dword ptr [eax+0x04], 0x49742400
        mov dword ptr [eax+0x0C], 0x49742400
        mov dword ptr [eax+0x10], 0x49742400
        jmp ret_InfiniteResources

    original_code:
        fstp dword ptr [eax+0x04]
        fld dword ptr [ecx+0x08]
        jmp ret_InfiniteResources
    }
}

void __declspec(naked) detour_InfiniteMorale() {
    __asm {
        mov eax, [ebp+0x10]
        cmp eax, [g_playerBase]
        jne original_code
        fld dword ptr [esi+0x08]
        mov dword ptr [esi+0x08], 0x3F800000
        jmp ret_InfiniteMorale

    original_code:
        fld dword ptr [esi+0x08]
        fsub dword ptr [esp+0x10]
        jmp ret_InfiniteMorale
    }
}

void __declspec(naked) detour_InstantCapture() {
    __asm {
        mov edx, [esi+0x40]
        cmp edx, [g_playerBase]
        jne original_code
        mov dword ptr [esi+0x44], 0x43B40000
        jmp ret_InstantCapture

    original_code:
        fstp dword ptr [esi+0x44]
        je branch_InstantCapture
        jmp ret_InstantCapture
    }
}

void __declspec(naked) detour_OneHitKill() {
    __asm {
        fld dword ptr [esi+0x08]
        fstp dword ptr [esp+0x18]
        mov dword ptr [esi+0x08], 0x47AF0000
        jmp ret_OneHitKill
    }
}

// --- Lua Bridge ---
static int l_ToggleCheat(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    bool enable = lua_toboolean(L, 2);

    #define TOGGLE_JMP_HOOK(cheat, size, ...) if (strcmp(name, #cheat) == 0) { Cheats::cheat = enable; Memory::PlaceJmp(addr_##cheat, enable ? detour_##cheat : nullptr, size, &ret_##cheat, ##__VA_ARGS__); }

    TOGGLE_JMP_HOOK(infinite_health, 5);
    TOGGLE_JMP_HOOK(infinite_resources, 6);
    TOGGLE_JMP_HOOK(fast_build, 5);
    TOGGLE_JMP_HOOK(infinite_morale, 7);
    TOGGLE_JMP_HOOK(infinite_cap, 6);
    TOGGLE_JMP_HOOK(instant_equip, 6);
    TOGGLE_JMP_HOOK(instant_capture, 5, &branch_InstantCapture);
    TOGGLE_JMP_HOOK(fast_abilities, 7);
    TOGGLE_JMP_HOOK(remove_fow, 6);
    TOGGLE_JMP_HOOK(one_hit_kill, 7);
    TOGGLE_JMP_HOOK(all_wargear, 5);

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

// --- Main Payload & Hooks ---
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

    Memory::PlaceJmp(addr_GetPlayerBase, detour_GetPlayerBase, 6, &ret_GetPlayerBase);
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

    Beep(500, 300);
    SetWindowLongPtr(g_window, GWLP_WNDPROC, (LONG_PTR)pOriginalWndProc);
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();
    lua_close(g_LuaState);

    FreeLibraryAndExitThread((HMODULE)lpParam, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_hModule = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, PayloadThread, hModule, 0, nullptr);
    }
    return TRUE;
}
