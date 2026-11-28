local started = false
local scenes = 0

lom.on("game_start", function(seed)
    assert(seed == 4000123456 and lom.seed() == seed, "the game started with seed " .. tostring(seed))
    started = true
end)

lom.on("file_load", function(path, bytes)
    if path == "BIN/FIELD.BIN" then
        assert(bytes == lom.unpack(lom.disc_file(path)), "FIELD.BIN isn't what lom.unpack gives")
    elseif path:find("^ANA/MAPINFO/") or path:find("^ANA/INFO_") then
        assert(started, path .. " loaded before game_start")
        assert(bytes == lom.disc_file(path), path .. " isn't what lom.disc_file gives")
        scenes = scenes + 1
    end
end)

lom.on("scene_enter", function(scene)
    assert(scenes > 0, "scene " .. scene .. " was entered without its scene file going through file_load")
end)

lom.on("artifact_award", function(land)
    if land == 1 then
        return 2
    end
end)
