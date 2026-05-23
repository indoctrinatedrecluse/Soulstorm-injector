cmake_minimum_required(VERSION 3.14)
project(PayloadTarget)

include(FetchContent)

# Fetch MinHook for reliable API and function hooking
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.3
)
FetchContent_MakeAvailable(minhook)

# Fetch official Lua source
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.4
)
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
    # Get all Lua C files but exclude the executables
    file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
    list(REMOVE_ITEM LUA_SOURCES "${lua_SOURCE_DIR}/lua.c" "${lua_SOURCE_DIR}/luac.c")

    add_library(lua_static STATIC ${LUA_SOURCES})
    target_include_directories(lua_static PUBLIC "${lua_SOURCE_DIR}")

    if(MSVC)
        target_compile_options(lua_static PRIVATE /W0) # suppress lua warnings
    endif()
endif()

add_library(payload SHARED dllmain.cpp scanner.cpp scanner.h)

# Link our libraries
target_link_libraries(payload PRIVATE minhook lua_static)
target_include_directories(payload PRIVATE "${lua_SOURCE_DIR}")
