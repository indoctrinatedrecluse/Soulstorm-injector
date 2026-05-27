#include "seh_helpers.h"

// Note: No C++ headers (like string or iostream) should be included in this file
// to avoid C2712 compiler errors when mixing C++ objects and SEH (__try/__except).

bool IsPlayerEntity(uintptr_t entityPtr, uintptr_t playerBase) {
    __try {
        if (entityPtr != 0 && *(uintptr_t*)(entityPtr + 0x40) == playerBase) {
            return true;
        }
    } __except (1) { // EXCEPTION_EXECUTE_HANDLER
        // Exception caught, return false safely
    }
    return false;
}

bool IsPlayerResource(uintptr_t resourceOwnerPtr, uintptr_t playerBase) {
    __try {
        if (resourceOwnerPtr == playerBase) {
            return true;
        }
    } __except (1) {}
    return false;
}

bool IsPlayerMorale(uintptr_t moraleOwnerPtr, uintptr_t playerBase) {
    __try {
        if (moraleOwnerPtr != 0 && *(uintptr_t*)(moraleOwnerPtr + 0x10) == playerBase) {
            return true;
        }
    } __except (1) {}
    return false;
}

bool SafeWriteFloat(uintptr_t address, float value) {
    __try {
        if (address != 0) {
            *(float*)address = value;
            return true;
        }
    } __except (1) {}
    return false;
}

bool SafeWriteInt(uintptr_t address, int value) {
     __try {
        if (address != 0) {
            *(int*)address = value;
            return true;
        }
    } __except (1) {}
    return false;
}
