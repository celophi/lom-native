lom.on("file_load", function(path, bytes)
    if path == "ANA/MAPINFO/A.IMG" then
        local b = lom.bytes(bytes)
        b:u16(2, 0xBEEF)
        return b
    end
end)
