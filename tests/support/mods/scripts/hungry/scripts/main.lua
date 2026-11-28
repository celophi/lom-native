local hoard = {}

lom.on("frame", function()
    for i = 1, 100000000 do
        hoard[i] = string.rep("x", 1000) .. i
    end
end)
