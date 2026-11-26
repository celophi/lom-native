local started
lom.on("game_start", function(seed)
    assert(seed ~= nil and lom.seed() == seed, "the seed and lom.seed() differ")
    started = seed
end)
lom.on("scene_enter", function(check)
    assert(started % 1000000 == check, "started with seed " .. tostring(started))
end)
