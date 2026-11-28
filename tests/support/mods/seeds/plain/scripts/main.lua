lom.on("game_start", function(seed)
    assert(seed == nil and lom.seed() == nil, "a mod that doesn't ask for a seed got one")
end)
