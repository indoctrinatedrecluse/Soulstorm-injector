cmake_minimum_required(VERSION 3.10)
project(SoulstormInjector)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(injector)
add_subdirectory(payload)

# This ensures the payload DLL is built before the injector executable.
add_dependencies(injector payload)
