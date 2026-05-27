cmake_minimum_required(VERSION 3.14)
project(PayloadTarget)

include(FetchContent)

# --- MinHook for function hooking ---
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.3
)
FetchContent_MakeAvailable(minhook)

# MinHook does not have a CMakeLists.txt in its root directory,
# so we must compile its source files manually.
file(GLOB MINHOOK_SOURCES "${minhook_SOURCE_DIR}/src/*.c" "${minhook_SOURCE_DIR}/src/hde/*.c")
add_library(MinHook STATIC ${MINHOOK_SOURCES})
target_include_directories(MinHook PUBLIC "${minhook_SOURCE_DIR}/include")


# --- Lua for scripting ---
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.4
)
FetchContent_MakeAvailable(lua)

# Get all Lua C files but exclude the executables and onelua.c
file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
list(REMOVE_ITEM LUA_SOURCES "${lua_SOURCE_DIR}/lua.c" "${lua_SOURCE_DIR}/luac.c" "${lua_SOURCE_DIR}/onelua.c")

add_library(lua_static STATIC ${LUA_SOURCES})
target_include_directories(lua_static PUBLIC "${lua_SOURCE_DIR}")

if(MSVC)
    target_compile_options(lua_static PRIVATE /W0) # suppress lua warnings
endif()


# --- Dear ImGui for UI ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.88
)
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
)
target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}" "${imgui_SOURCE_DIR}/backends")


# --- Embed Lua scripts as resources ---
file(GLOB LUA_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/lua/*.lua")
set(RESOURCE_FILE "${CMAKE_CURRENT_BINARY_DIR}/resources.rc")
file(WRITE ${RESOURCE_FILE} "")
foreach(script ${LUA_SCRIPTS})
    get_filename_component(script_name ${script} NAME_WE)
    string(TOUPPER ${script_name} script_name_upper)
    file(APPEND ${RESOURCE_FILE} "${script_name_upper} RCDATA \"${script}\"\n")
endforeach()

# --- Main Payload DLL ---
add_library(payload SHARED dllmain.cpp scanner.cpp scanner.h seh_helpers.cpp seh_helpers.h ${LUA_SCRIPTS} ${RESOURCE_FILE})

# Link all libraries
target_link_libraries(payload PRIVATE MinHook lua_static imgui psapi dbghelp)

# Add include directories for all dependencies
target_include_directories(payload PRIVATE
    "${lua_SOURCE_DIR}"
    "${minhook_SOURCE_DIR}/include"
    "${imgui_SOURCE_DIR}"
)
