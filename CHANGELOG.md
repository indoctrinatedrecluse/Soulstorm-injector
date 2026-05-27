# v1.2.0

- Implemented comprehensive logging for both the injector and payload, writing to `injector.log` and `payload.log` respectively.
- Added a top-level exception filter to generate a `crash.dmp` file if the game process terminates unexpectedly.
- Hardened all cheat detours by wrapping memory access in `__try`/`__except` blocks to prevent the game from crashing due to bad pointers or offsets.
- Added a delay and module check to the payload's initialization to improve stability when injecting into an already running game.
- Changed the menu toggle hotkey to F1.
- Added audible `Beep()` feedback for all hotkey and cheat toggle actions.

# v1.1.3

- Fixed GitHub Actions release failure by granting `contents: write` permissions to the build job.

# v1.1.2

- Fixed `LNK1181` linker error by manually defining the `minhook` library target in CMake.
- Fixed CMake deprecation warnings by using `FetchContent_MakeAvailable`.
- Added `psapi` to linked libraries.

# v1.1.1

- Fixed build errors by correctly linking dependency include directories.
- Fixed Lua linker errors by excluding `onelua.c` from the build.

# v1.1.0

- Reverted to MinHook for robust and reliable function hooking.
- Refactored detour functions to standard C++ calling conventions.
- Added initial crash protection and a unified `Shutdown()` routine.

# v1.0.0

- Initial release of the Soulstorm Injector and Lua Payload.
- Implemented core injection mechanism, ImGui overlay, and all cheats from the reference cheat table.