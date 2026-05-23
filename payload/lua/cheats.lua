-- cheats.lua
-- Cheat implementations and state management

Cheats = {
    infinite_health = false,
    infinite_resources = false,
    fast_build = false,
    infinite_morale = false,
    infinite_cap = false,
    instant_equip = false,
    remove_fow = false
}

function ToggleCheat(cheat_name)
    if Cheats[cheat_name] ~= nil then
        Cheats[cheat_name] = not Cheats[cheat_name]

        -- Call the corresponding C++ hook toggler dynamically
        local func_name = "Toggle_" .. cheat_name
        if _G[func_name] then
            _G[func_name](Cheats[cheat_name])
        end
    end
end

-- Function to get the current status for the display
function GetCheatStatusString()
    local status = "=== SOULSTORM CHEATS ===\n\n"
    status = status .. "[F2] Infinite Health: " .. (Cheats.infinite_health and "ON" or "OFF") .. "\n"
    status = status .. "[F3] Infinite Resources: " .. (Cheats.infinite_resources and "ON" or "OFF") .. "\n"
    status = status .. "[F4] Fast Build/Research: " .. (Cheats.fast_build and "ON" or "OFF") .. "\n"
    status = status .. "[F5] Infinite Morale: " .. (Cheats.infinite_morale and "ON" or "OFF") .. "\n"
    status = status .. "[F6] Infinite Squad/Vehicle Cap: " .. (Cheats.infinite_cap and "ON" or "OFF") .. "\n"
    status = status .. "[F7] Instant Equipment: " .. (Cheats.instant_equip and "ON" or "OFF") .. "\n"
    status = status .. "[F8] Remove Fog of War: " .. (Cheats.remove_fow and "ON" or "OFF") .. "\n"
    status = status .. "\n[END] Uninject & Close\n"
    return status
end

print("cheats.lua loaded.")
