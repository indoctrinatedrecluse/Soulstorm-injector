#include "scanner.h"
#include <psapi.h>
#include <iostream>

#define INRANGE(x,a,b)  (x >= a && x <= b)
#define getBits( x )    (INRANGE((x&(~0x20)),'A','F') ? ((x&(~0x20)) - 'A' + 0xa) : (INRANGE(x,'0','9') ? x - '0' : 0))
#define getByte( x )    (getBits(x[0]) << 4 | getBits(x[1]))

namespace Memory {
    uintptr_t FindPattern(const char* moduleName, const char* pattern) {
        const char* pat = pattern;
        uintptr_t firstMatch = 0;

        HMODULE hModule = GetModuleHandleA(moduleName);
        if (!hModule) return 0;

        MODULEINFO mi;
        if (!GetModuleInformation(GetCurrentProcess(), hModule, &mi, sizeof(MODULEINFO))) {
            return 0;
        }

        uintptr_t rangeStart = (uintptr_t)mi.lpBaseOfDll;
        uintptr_t rangeEnd = rangeStart + mi.SizeOfImage;

        for (uintptr_t pCur = rangeStart; pCur < rangeEnd; pCur++) {
            if (!*pat)
                return firstMatch;

            if (*(PBYTE)pat == '\?' || *(BYTE*)pCur == getByte(pat)) {
                if (!firstMatch)
                    firstMatch = pCur;

                if (!pat[2])
                    return firstMatch;

                if (*(PWORD)pat == '\?\?' || *(PBYTE)pat != '\?')
                    pat += 3;
                else
                    pat += 2; // one ?
            }
            else {
                pat = pattern;
                firstMatch = 0;
            }
        }
        return 0;
    }

    void PlaceJmp(uintptr_t address, void* hookFunc, size_t instructionSize, uintptr_t* returnAddress, uintptr_t* branchAddress, int branchOffset) {
        if (address == 0) return;

        DWORD oldProtect;
        VirtualProtect((LPVOID)address, instructionSize, PAGE_EXECUTE_READWRITE, &oldProtect);

        if (branchAddress != nullptr) {
            // Read the relative offset of the conditional jump
            // Usually 74 XX or 0F 84 XX XX XX XX
            if (*(BYTE*)address == 0x0F) { // Long jump
                int relOffset = *(int*)(address + 2);
                *branchAddress = address + 6 + relOffset;
            } else { // Short jump
                char relOffset = *(char*)(address + 1);
                *branchAddress = address + 2 + relOffset;
            }
            // Adjust if the branch address was provided at an offset
            if (branchOffset > 0) {
                 if (*(BYTE*)(address+branchOffset) == 0x0F) {
                     int relOffset = *(int*)(address + branchOffset + 2);
                     *branchAddress = address + branchOffset + 6 + relOffset;
                 } else {
                     char relOffset = *(char*)(address + branchOffset + 1);
                     *branchAddress = address + branchOffset + 2 + relOffset;
                 }
            }
        }

        *returnAddress = address + instructionSize;

        *(BYTE*)address = 0xE9; // JMP
        *(uintptr_t*)(address + 1) = (uintptr_t)hookFunc - address - 5;

        for (size_t i = 5; i < instructionSize; i++) {
            *(BYTE*)(address + i) = 0x90; // NOP
        }

        VirtualProtect((LPVOID)address, instructionSize, oldProtect, &oldProtect);
    }
}
