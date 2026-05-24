#pragma once
#include <windows.h>

namespace Memory {
    uintptr_t FindPattern(const char* module, const char* pattern);
}
