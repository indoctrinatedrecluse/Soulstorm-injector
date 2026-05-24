# v1.1.0

- Reverted to MinHook for robust and reliable function hooking.
- Fixed Lua linker errors by excluding `onelua.c` from the build process.
- Refactored detour functions to standard C++ calling conventions, replacing problematic inline assembly.
- Added comprehensive crash protection:
  - Implemented `nullptr` checks for failed memory pattern scans.
  - Wrapped memory access within detour functions in `__try`/`__except` blocks to prevent crashes from access violations.
- Implemented a unified `Shutdown()` routine triggered on DLL detach or the 'END' key for clean uninjection.

# v1.0.0

- Initial release of the Soulstorm Injector and Lua Payload.
- Implemented core injection mechanism with waiting logic for Soulstorm.exe.
- Implemented robust manual JMP hooking for game functions.
- Integrated Dear ImGui for an interactive in-game overlay.
- Embedded Lua 5.4 scripting engine for dynamic cheat toggling.
- Added cheats:
  - Infinite Health
  - One-Hit Kill
  - Infinite Resources (Requisition & Power)
  - Infinite Faith (Sisters of Battle)
  - Infinite Souls (Dark Eldar)
  - Infinite Morale
  - Infinite Squad/Vehicle Cap
  - Fast Build/Research
  - Instant Equipment
  - Fast Special Abilities
  - Instant Strategic Point Capture
  - Remove Fog of War
  - Get All Wargear (Campaign)
- Added safe uninjection feature on 'END' key.