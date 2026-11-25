local last = {}
local hurt = false

local function check()
    for _, member in ipairs(lom.party()) do
        local before = last[member.slot]

        if before and before.max_hp == member.max_hp then
            assert(member.hp <= before.hp, "slot " .. member.slot .. " healed from " .. before.hp .. " to " .. member.hp)
        end

        if member.hp < member.max_hp then
            hurt = true
        end

        last[member.slot] = {hp = member.hp, max_hp = member.max_hp}
    end
end

lom.on("scene_enter", function()
    last = {}
end)

lom.on("frame", check)

lom.on("battle_end", function()
    check()
    assert(hurt, "nobody was hurt in the fight, so this test didn't show anything")
end)
