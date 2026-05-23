-- main.lua
require("cheats")

print("Soulstorm Lua Payload Initialized.")

if playerBase == nil or playerBase == 0 then
    print("Error: playerBase not set from C++!")
    return
end

print("playerBase is: " .. string.format("0x%X", playerBase))

-- This function is called every frame by the C++ EndScene hook
function on_render()
    -- Get the status of all cheats
    local cheat_menu_text = GetCheatStatusString()

    -- Draw the text on screen at position (10, 10)
    -- The draw_text function is provided by our C++ code.
    draw_text(10, 10, cheat_menu_text)
end
