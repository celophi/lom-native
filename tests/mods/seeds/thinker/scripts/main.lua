local function think()
    local n = 0
    for i = 1, 4000000 do
        n = n + i
    end
    return n
end
lom.on("game_start", think)
lom.on("frame", think)
