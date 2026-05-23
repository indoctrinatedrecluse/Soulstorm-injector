-- cheats.lua
-- Manages the state of cheats. The C++ side reads these values.

Cheats = {
    infinite_health = false,
    infinite_resources = false,
    infinite_faith = false,
    infinite_souls = false,
    fast_build = false,
    infinite_morale = false,
    infinite_cap = false,
    instant_equip = false,
    remove_fow = false,
    one_hit_kill = false,
    instant_capture = false,
    fast_abilities = false,
    all_wargear = false
}

-- This is a generic toggle function.
-- It flips the boolean state and calls the C++ function to apply/remove the hook.
function ToggleCheat(cheat_name)
    if Cheats[cheat_name] ~= nil then
        Cheats[cheat_name] = not Cheats[cheat_name]

        -- Call the unified C++ function
        _G.ToggleCheat(cheat_name, Cheats[cheat_name])
        print(cheat_name .. " toggled to: " .. tostring(Cheats[cheat_name]))
    end
end

print("cheats.lua loaded.")
