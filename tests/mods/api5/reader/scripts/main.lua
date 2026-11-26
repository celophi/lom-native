assert(lom.api == 5)
assert(lom.game() == "SLUS-01013")
assert(lom.seed() == nil, "a mod that doesn't ask for a seed got one")

assert(lom.disc_file("ANA/MAPINFO/A.IMG") == "ABCDEFGH")
assert(lom.disc_file("ANA/MAPINFO/NONE.IMG") == nil)
for _, path in ipairs({"../secret", "/etc/passwd", "ANA//A.IMG", "ANA/./A.IMG", "ANA\\A.IMG", ""}) do
    assert(not pcall(lom.disc_file, path), "lom.disc_file took " .. path)
end

local files = lom.disc_files("ANA")
assert(#files == 3 and files[1] == "ANA/INFO_BON/C.IMG" and files[2] == "ANA/MAPINFO/A.IMG" and files[3] == "ANA/MAPINFO/B.IMG")
assert(#lom.disc_files() == 4 and #lom.disc_files("") == 4)
assert(lom.disc_files("NOPE") == nil)
assert(not pcall(lom.disc_files, "../"))
assert(lom.unpack(lom.disc_file("BIN/PACKED.BIN")) == "unpacked")
assert(not pcall(lom.unpack, "not packed"))

local b = lom.bytes("\1\2\3\4\255\255\255\255")
assert(#b == 8 and b:u8(0) == 1 and b:u16(0) == 0x0201 and b:u32(0) == 0x04030201)
assert(b:s8(4) == -1 and b:s16(4) == -1 and b:s32(4) == -1 and b:u32(4) == 0xFFFFFFFF)
b:s16(0, -2)
assert(b:u16(0) == 0xFFFE)
b:u32(4, 0x12345678)
assert(b:string() == "\254\255\3\4\120\86\52\18")
assert(not pcall(b.u32, b, 5), "read past the end")
assert(not pcall(b.u8, b, -1), "read before the start")
assert(not pcall(b.u8, b, 0, 256), "wrote 256 in a byte")
assert(not pcall(b.s8, b, 0, -129), "wrote -129 in a byte")

local expected = {2949682060, 1503998484, 862323420, 1609669750}
local r = lom.random(42)
for i = 1, 4 do
    assert(r:int(0, 4294967295) == expected[i], "PCG32 drew another number")
end
local dice = {5, 1, 1, 5, 4, 6, 3, 3}
r = lom.random(42)
for i = 1, 8 do
    assert(r:int(6) == dice[i], "PCG32 rolled another die")
end
local shuffled = lom.random(7):shuffle({1, 2, 3, 4, 5, 6, 7, 8, 9, 10})
assert(table.concat(shuffled, ",") == "7,6,1,5,8,2,4,10,3,9", table.concat(shuffled, ","))
r = lom.random(1)
for _ = 1, 100 do
    local f = r:float()
    assert(f >= 0 and f < 1)
    local n = r:int(-3, 3)
    assert(n >= -3 and n <= 3)
end
assert(lom.random(3):pick({"only"}) == "only")
assert(not pcall(r.pick, r, {}))
assert(not pcall(r.int, r, 5, 4))
assert(not pcall(r.int, r, 0, 4294967296), "a range wider than 2^32")
