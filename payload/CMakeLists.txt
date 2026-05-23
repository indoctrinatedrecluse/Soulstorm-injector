cmake_minimum_required(VERSION 3.14)
project(PayloadTarget)

include(FetchContent)

# --- Lua for scripting ---
FetchContent_Declare(
    lua
    GIT_REPOSITORY https://github.com/lua/lua.git
    GIT_TAG        v5.4.4
)
FetchContent_GetProperties(lua)
if(NOT lua_POPULATED)
    FetchContent_Populate(lua)
    file(GLOB LUA_SOURCES "${lua_SOURCE_DIR}/*.c")
    list(REMOVE_ITEM LUA_SOURCES "${lua_SOURCE_DIR}/lua.c" "${lua_SOURCE_DIR}/luac.c")

    add_library(lua_static STATIC ${LUA_SOURCES})
    target_include_directories(lua_static PUBLIC "${lua_SOURCE_DIR}")

    if(MSVC)
        target_compile_options(lua_static PRIVATE /W0) # suppress lua warnings
    endif()
endif()

# --- Dear ImGui for UI ---
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.88
)
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
    add_library(imgui STATIC
        "${imgui_SOURCE_DIR}/imgui.cpp"
        "${imgui_SOURCE_DIR}/imgui_draw.cpp"
        "${imgui_SOURCE_DIR}/imgui_tables.cpp"
        "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_dx9.cpp"
        "${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp"
    )
    target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}")
    target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}/backends")
endif()

# --- Embed Lua scripts as resources ---
file(GLOB LUA_SCRIPTS "${CMAKE_CURRENT_SOURCE_DIR}/lua/*.lua")

# Create a resource file
set(RESOURCE_FILE "${CMAKE_CURRENT_BINARY_DIR}/resources.rc")
file(WRITE ${RESOURCE_FILE} "") # Create empty file

foreach(script ${LUA_SCRIPTS})
    get_filename_component(script_name ${script} NAME_WE)
    string(TOUPPER ${script_name} script_name_upper)
    file(APPEND ${RESOURCE_FILE} "${script_name_upper} RCDATA \"${script}\"\n")
endforeach()

# --- Main Payload DLL ---
add_library(payload SHARED dllmain.cpp scanner.cpp scanner.h ${LUA_SCRIPTS} ${RESOURCE_FILE})

# Link all libraries
target_link_libraries(payload PRIVATE lua_static imgui)
target_include_directories(payload PRIVATE "${lua_SOURCE_DIR}")
