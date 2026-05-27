#include "seh_helpers.h"

bool IsPlayerEntity(uintptr_t entityPtr, uintptr_t playerBase) {
    __try {
        if (entityPtr != 0 && *(uintptr_t*)(entityPtr + 0x40) == playerBase) {
            return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        // Log this exception if possible, but do it from the calling function
        // to avoid C++ objects in this SEH frame.
    }
    return false;
}

bool IsPlayerResource(uintptr_t resourceOwnerPtr, uintptr_t playerBase) {
    __try {
        if (resourceOwnerPtr == playerBase) {
            return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}

bool IsPlayerMorale(uintptr_t moraleOwnerPtr, uintptr_t playerBase) {
    __try {
        if (moraleOwnerPtr != 0 && *(uintptr_t*)(moraleOwnerPtr + 0x10) == playerBase) {
            return true;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return false;
}
