lom.on("file_load", function(path, bytes)
    if path == "ANA/MAPINFO/A.IMG" then
        assert(string.unpack("<I2", bytes, 3) == 0xBEEF, "it didn't see the mod above's change")
        return bytes:sub(1, 1) .. "Z" .. bytes:sub(3)
    end
end)
