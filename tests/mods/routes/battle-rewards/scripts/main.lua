local battles = 0
local ended = 0

lom.on("scene_enter", function(scene)
    assert(lom.scene() == scene)
end)

lom.on("frame", function()
    assert(lom.scene() ~= nil, "a frame came before the first scene_enter")
end)

lom.on("battle_start", function()
    battles = battles + 1
end)

lom.on("battle_end", function()
    ended = ended + 1
    assert(ended == battles, "battle_end without its battle_start")

    local before = lom.money()
    assert(lom.add_money(100))
    assert(lom.money() == before + 100, "add_money didn't add 100")

    local party = lom.party()
    assert(#party >= 1, "nobody in the party after a fight")

    for _, member in ipairs(party) do
        assert(member.max_hp > 0 and member.hp >= 0 and member.hp <= member.max_hp, "a party member's hit points don't add up")
    end

    lom.log("battle", ended, "ended: lucre", before, "to", lom.money(), "with", #party, "in the party")
end)
