#include <windows.h>
#include <string>
#include <vector>
#include <d3d9.h>
#include <map>

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

// --- Detour Declarations ---
#define DECLARE_DETOUR(name) void __declspec(naked) detour_##name()

DECLARE_DETOUR(GetPlayerBase);
DECLARE_DETOUR(InfiniteHealth);
DECLARE_DETOUR(InfiniteResources);
DECLARE_DETOUR(FastBuild);
DECLARE_DETOUR(InfiniteMorale);
DECLARE_DETOUR(InfiniteCap);
DECLARE_DETOUR(InstantEquipment);
DECLARE_DETOUR(InstantCapture);
DECLARE_DETOUR(FastAbilities);
DECLARE_DETOUR(RemoveFOW);
DECLARE_DETOUR(OneHitKill);
DECLARE_DETOUR(AllWargear);

// --- Detour Implementations ---
void __declspec(naked) detour_GetPlayerBase() { __asm { mov g_playerBase, ebx; cmp eax, [edi+0x1B8]; jmp g_hooks["GetPlayerBase"].returnAddress; } }
void __declspec(naked) detour_FastBuild() { __asm { mov dword ptr [eax+0x0C], 0; jmp g_hooks["FastBuild"].returnAddress; } }
void __declspec(naked) detour_InfiniteCap() { __asm { mov dword ptr [eax], 0; mov ebp, [eax]; lea ecx, [esp+0x28]; jmp g_hooks["InfiniteCap"].returnAddress; } }
void __declspec(naked) detour_InstantEquipment() { __asm { mov dword ptr [eax+0x08], 0; jmp g_hooks["InstantEquipment"].returnAddress; } }
void __declspec(naked) detour_FastAbilities() { __asm { mov dword ptr [esi+0x78], 0; jmp g_hooks["FastAbilities"].returnAddress; } }
void __declspec(naked) detour_RemoveFOW() { __asm { mov dword ptr [ecx+0xC58], 0x3dcccccd; jmp g_hooks["RemoveFOW"].returnAddress; } }
void __declspec(naked) detour_AllWargear() { __asm { nop; nop; nop; jmp g_hooks["AllWargear"].returnAddress; } }

void __declspec(naked) detour_InfiniteHealth() {
    __asm {
        __try {
            mov ecx, [g_playerBase]
            cmp ebp, ecx
            jne original_code
            mov [esi+0x14], 1
            jmp g_hooks["InfiniteHealth"].returnAddress
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            jmp original_code
        }
    original_code:
        fst dword ptr [esi+0x14]
        fld1
        jmp g_hooks["InfiniteHealth"].returnAddress
    }
}

void __declspec(naked) detour_InfiniteResources() {
    __asm {
        __try {
            cmp esi, [g_playerBase]
            jne original_code

            mov dword ptr [eax], 0x49742400
            mov dword ptr [eax+0x04], 0x49742400
            mov dword ptr [eax+0x0C], 0x49742400
            mov dword ptr [eax+0x10], 0x49742400
            jmp g_hooks["InfiniteResources"].returnAddress
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            jmp original_code
        }
    original_code:
        fstp dword ptr [eax+0x04]
        fld dword ptr [ecx+0x08]
        jmp g_hooks["InfiniteResources"].returnAddress
    }
}

void __declspec(naked) detour_InfiniteMorale() {
    __asm {
        __try {
            mov eax, [ebp+0x10]
            cmp eax, [g_playerBase]
            jne original_code
            fld dword ptr [esi+0x08]
            mov dword ptr [esi+0x08], 0x3F800000
            jmp g_hooks["InfiniteMorale"].returnAddress
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            jmp original_code
        }
    original_code:
        fld dword ptr [esi+0x08]
        fsub dword ptr [esp+0x10]
        jmp g_hooks["InfiniteMorale"].returnAddress
    }
}

void __declspec(naked) detour_InstantCapture() {
    __asm {
        __try {
            mov edx, [esi+0x40]
            cmp edx, [g_playerBase]
            jne original_code
            mov dword ptr [esi+0x44], 0x43B40000
            jmp g_hooks["InstantCapture"].returnAddress
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            jmp original_code
        }
    original_code:
        fstp dword ptr [esi+0x44]
        je g_hooks["InstantCapture"].branchAddress
        jmp g_hooks["InstantCapture"].returnAddress
    }
}

void __declspec(naked) detour_OneHitKill() {
    __asm {
        fld dword ptr [esi+0x08]
        fstp dword ptr [esp+0x18]
        mov dword ptr [esi+0x08], 0x47AF0000
        jmp g_hooks["OneHitKill"].returnAddress
    }
}

// --- Lua Bridge ---
static int l_ToggleCheat(lua_State* L) {
    const char* name = lua_tostring(L, 1);
    bool enable = lua_toboolean(L, 2);

    if (g_hooks.find(name) != g_hooks.end()) {
        uintptr_t address = Memory::FindPattern(g_hooks[name].originalBytes.size() > 6 ? "WXPMod.dll" : "soulstorm.exe", (const char*)g_hooks[name].originalBytes.data());
        if(address == 0) return 0;

        if (enable) {
            Memory::PlaceJmp(address, g_hooks[name].detourFunction, g_hooks[name].instructionSize, &g_hooks[name].returnAddress, &g_hooks[name].branchAddress);
        } else {
            Memory::RestoreJmp(address, g_hooks[name].instructionSize, g_hooks[name].originalBytes.data());
        }
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

void Shutdown() {
    if (pOriginalWndProc) {
        SetWindowLongPtr(g_window, GWLP_WNDPROC, (LONG_PTR)pOriginalWndProc);
    }

    for(auto const& [name, hook] : g_hooks) {
        uintptr_t address = Memory::FindPattern(hook.originalBytes.size() > 6 ? "WXPMod.dll" : "soulstorm.exe", (const char*)hook.originalBytes.data());
        if(address) Memory::RestoreJmp(address, hook.instructionSize, hook.originalBytes.data());
    }

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
    #define INIT_HOOK(name, pattern, size) \
        addr_##name = Memory::FindPattern(size > 6 ? "WXPMod.dll" : "soulstorm.exe", pattern); \
        if (addr_##name) { \
            g_hooks[#name] = {detour_##name, 0, 0, size, {}}; \
            g_hooks[#name].originalBytes.assign((BYTE*)addr_##name, (BYTE*)addr_##name + size); \
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
