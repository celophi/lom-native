lom.on("file_load", function(path, bytes)
    if path == "ANA/MAPINFO/B.IMG" then
        return bytes .. "x"
    end
end)
