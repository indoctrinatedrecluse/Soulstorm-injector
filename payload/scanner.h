#pragma once
#include <windows.h>

namespace Memory {
    uintptr_t FindPattern(const char* module, const char* pattern);

    // Places a JMP instruction at 'address' to 'hookFunc'.
    // Overwrites 'instructionSize' bytes with NOPs.
    // Populates 'returnAddress' with the address immediately following the hooked instructions.
    // If the hooked instruction was a conditional jump, 'branchAddress' is populated with the destination of that jump.
    void PlaceJmp(uintptr_t address, void* hookFunc, size_t instructionSize, uintptr_t* returnAddress, uintptr_t* branchAddress = nullptr, int branchOffset = 0);
}
