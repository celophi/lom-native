lom.on("scene_enter", function(expected)
    local roll = math.random(1, 1000)
    assert(roll == expected, "math.random gave " .. roll .. ", the game's numbers give " .. expected)
end)
