# v1.1.2

- Fixed `LNK1181` linker error by manually defining the `minhook` library target in CMake, as the upstream project lacks a root `CMakeLists.txt`.
- Fixed CMake deprecation warnings by replacing `FetchContent_Populate` with the modern `FetchContent_MakeAvailable` command.
- Added `psapi` to linked libraries to resolve potential `GetModuleInformation` unresolved external symbol errors.
- Fixed `C2712` compiler errors by isolating SEH `__try`/`__except` blocks into pure C helper functions, separating them from C++ objects with destructors.

# v1.1.1

- Fixed build errors by correctly linking dependency include directories (MinHook, Lua, ImGui).
- Fixed Lua linker errors by excluding `onelua.c` from the build process.

# v1.1.0

- Reverted to MinHook for robust and reliable function hooking.
- Refactored detour functions to standard C++ calling conventions, replacing problematic inline assembly.
- Added comprehensive crash protection:
  - Implemented `nullptr` checks for failed memory pattern scans.
  - Wrapped memory access within detour functions in `__try`/`__except` blocks to prevent crashes from access violations.
- Implemented a unified `Shutdown()` routine triggered on DLL detach or the 'END' key for clean uninjection.

# v1.0.0

- Initial release of the Soulstorm Injector and Lua Payload.
- Implemented core injection mechanism with waiting logic for Soulstorm.exe.
- Integrated Dear ImGui for an interactive in-game overlay.
- Embedded Lua 5.4 scripting engine for dynamic cheat toggling.
- Implemented all cheats from the reference cheat table.
- Added safe uninjection feature on 'END' key.