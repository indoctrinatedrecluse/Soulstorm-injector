#pragma once
#include <windows.h>

// --- Safe Memory Checkers ---
bool IsPlayerEntity(uintptr_t entityPtr, uintptr_t playerBase);
bool IsPlayerResource(uintptr_t resourceOwnerPtr, uintptr_t playerBase);
bool IsPlayerMorale(uintptr_t moraleOwnerPtr, uintptr_t playerBase);

// --- Safe Memory Writers ---
// Returns true on success, false if an exception occurred.
bool SafeWriteFloat(uintptr_t address, float value);
bool SafeWriteInt(uintptr_t address, int value);
