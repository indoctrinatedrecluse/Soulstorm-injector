# Soulstorm Injector & Lua Payload

This project is a C++ and Lua-based modding framework for the game **Dawn of War: Soulstorm**. It uses DLL injection to load a payload that provides various in-game cheats, which can be toggled via hotkeys.

The framework is designed to be robust and flexible:
- **The Injector (`injector/`)**: A lightweight C++ executable that finds the `Soulstorm.exe` process and injects the payload DLL.
- **The Payload (`payload/`)**: The core of the mod. It uses pattern scanning to find memory addresses dynamically, ensuring it works across different game versions. It hooks into game functions using MinHook and integrates a Lua scripting engine to manage the cheat logic and UI.

## Features
- In-game menu to show the status of all cheats.
- Hotkey-toggleable cheats, including:
  - Infinite Health
  - Infinite Resources (Requisition, Power, Faith/Souls)
  - Fast Build & Research
  - Infinite Morale
  - Infinite Squad & Vehicle Cap
  - Instant Equipment Upgrades
  - Remove Fog of War
- Safe uninjection feature to cleanly unload the mod from the game.

## Dependencies

The project uses CMake's `FetchContent` to automatically download and link its dependencies during the build process. You do not need to download these manually:
* **[MinHook](https://github.com/TsudaKageyu/minhook)**: For safely hooking game functions.
* **[Lua](https://github.com/lua/lua)**: The official Lua 5.4 source code for the scripting backend.
* **DirectX 9 SDK**: Required for rendering the in-game menu. This is typically included with the Windows SDK in a Visual Studio environment.

## Build Instructions (CLion with MSVC)

**IMPORTANT:** Dawn of War: Soulstorm is a 32-bit application. Therefore, both the injector and the payload DLL **must** be compiled as 32-bit (x86) binaries.

1.  **Open the project folder in CLion.**
2.  **Configure Toolchain**:
    *   Go to `File -> Settings -> Build, Execution, Deployment -> Toolchains`.
    *   Ensure you have a **Visual Studio** toolchain selected.
    *   Set the **Architecture** to **x86**. This is critical for the injection to work.
3.  **Load CMake**: Wait for CLion to load the CMake project. It will automatically download MinHook and Lua. If it doesn't, reload it via `Tools -> CMake -> Reload CMake Project`.
4.  **Build**: Select the `injector` run configuration and click the **Build** (hammer) icon. This will compile both the injector and the payload DLL into the `cmake-build-debug` (or release) directory.

## Usage

1.  Launch *Dawn of War: Soulstorm*.
2.  Navigate to the build output directory (e.g., `cmake-build-debug/injector/`).
3.  Run the compiled `injector.exe`. You may need to run it as **Administrator** so it has permission to access the game's memory.
4.  A "beep" sound will confirm that the injection was successful, and a cheat menu will appear in the top-left corner of the game screen.

### Hotkeys
- **F2**: Toggle Infinite Health
- **F3**: Toggle Infinite Resources
- **F4**: Toggle Fast Build/Research
- **F5**: Toggle Infinite Morale
- **F6**: Toggle Infinite Squad/Vehicle Cap
- **F7**: Toggle Instant Equipment
- **F8**: Toggle Remove Fog of War
- **END**: Safely uninjects the DLL and closes the cheat menu.
