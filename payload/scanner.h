#pragma once
#include <windows.h>

namespace Memory {
    uintptr_t FindPattern(const char* module, const char* pattern);
    void PlaceHook(uintptr_t address, void* hookFunc, size_t instructionSize, uintptr_t* returnAddress);
}
