-- main.lua
-- Note: 'cheats' module is loaded separately by the C++ loader now, no need to require it here

print("Soulstorm Lua Payload with ImGui Initialized.")

-- This function is called every frame by the C++ EndScene hook
function on_draw_frame()
    ImGui_Begin("Soulstorm Cheats")

    if ImGui_Checkbox("Infinite Health", Cheats.infinite_health) then ToggleCheat("infinite_health") end
    if ImGui_Checkbox("One-Hit Kill", Cheats.one_hit_kill) then ToggleCheat("one_hit_kill") end
    if ImGui_Checkbox("Infinite Resources (Req/Power)", Cheats.infinite_resources) then ToggleCheat("infinite_resources") end
    if ImGui_Checkbox("Infinite Faith (Sisters of Battle)", Cheats.infinite_faith) then ToggleCheat("infinite_faith") end
    if ImGui_Checkbox("Infinite Souls (Dark Eldar)", Cheats.infinite_souls) then ToggleCheat("infinite_souls") end
    if ImGui_Checkbox("Infinite Morale", Cheats.infinite_morale) then ToggleCheat("infinite_morale") end
    if ImGui_Checkbox("Infinite Squad/Vehicle Cap", Cheats.infinite_cap) then ToggleCheat("infinite_cap") end
    if ImGui_Checkbox("Fast Build/Research", Cheats.fast_build) then ToggleCheat("fast_build") end
    if ImGui_Checkbox("Instant Equipment", Cheats.instant_equip) then ToggleCheat("instant_equip") end
    if ImGui_Checkbox("Fast Special Abilities", Cheats.fast_abilities) then ToggleCheat("fast_abilities") end
    if ImGui_Checkbox("Instant Strategic Point Capture", Cheats.instant_capture) then ToggleCheat("instant_capture") end
    if ImGui_Checkbox("Remove Fog of War", Cheats.remove_fow) then ToggleCheat("remove_fow") end
    if ImGui_Checkbox("Get All Wargear (Campaign)", Cheats.all_wargear) then ToggleCheat("all_wargear") end

    ImGui_End()
end
