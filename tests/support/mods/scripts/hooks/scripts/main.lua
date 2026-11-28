local seen = {}

local function note(name)
    seen[#seen + 1] = name
end

lom.on("scene_enter", function(scene)
    assert(scene == 42, "scene_enter didn't pass the scene")
    note("scene_enter")
end)

lom.on("frame", function()
    note("frame")
end)

lom.on("frame", function()
    note("frame again")
end)

lom.on("battle_start", function()
    note("battle_start")
end)

lom.on("battle_end", function()
    note("battle_end")
    local order = table.concat(seen, ",")
    assert(order == "scene_enter,frame,frame again,frame,frame again,battle_start,battle_end", "hooks ran as " .. order)
end)

assert(not pcall(lom.on, "no_such_hook", function() end), "an unknown hook didn't raise an error")
assert(not pcall(lom.on, "frame", 5), "lom.on took something that isn't a function")
