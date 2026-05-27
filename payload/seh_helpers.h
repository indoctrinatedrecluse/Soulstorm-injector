#pragma once
#include <windows.h>

// Safely checks if an entity is owned by the player.
bool IsPlayerEntity(uintptr_t entityPtr, uintptr_t playerBase);

// Safely checks if a resource update is for the player.
bool IsPlayerResource(uintptr_t resourceOwnerPtr, uintptr_t playerBase);

// Safely checks if a morale update is for the player.
bool IsPlayerMorale(uintptr_t moraleOwnerPtr, uintptr_t playerBase);
