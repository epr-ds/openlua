-- Savedata host tests (run by test/host/host_main).

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

local GAME = "SVTEST1"
local DIR = "PSP/SAVEDATA/" .. GAME
data.delete(GAME)
files.delete(DIR)

check(data.save("payload", "Title", GAME, "Sub", "Desc", 1) == true,
      "autosave accepted")
check(files.exists(DIR .. "/AUTOSAVE"), "autosave file created")
check(files.exists(DIR .. "/META"), "metadata file created")
check(data.load(GAME, 1) == "payload", "autosave loaded")

local meta = io.open(DIR .. "/META", "r")
local body = meta and meta:read("*a")
if meta then meta:close() end
check(body and body:find("title=Title", 1, true), "metadata title")
check(body and body:find("subtitle=Sub", 1, true), "metadata subtitle")
check(body and body:find("description=Desc", 1, true), "metadata description")

check(data.save("slot-one", "Title", GAME, "", "", 0) == true,
      "first slot accepted")
check(files.exists(DIR .. "/SLOT1"), "SLOT1 created")
check(data.save("slot-two", "Title", GAME, "", "", 0) == true,
      "second slot accepted")
check(files.exists(DIR .. "/SLOT2"), "SLOT2 created")
check(data.load(GAME, 0) == "slot-two", "highest slot loaded")
local slot1 = assert(io.open(DIR .. "/SLOT1", "wb"))
slot1:write("rewritten-later")
slot1:close()
check(data.load(GAME, 0) == "slot-two", "highest slot wins over mtime")
check(data.load(GAME, 1) == "payload", "autosave untouched")

check(data.save(string.rep("x", 300), "T", GAME, "", "", 1) == true,
      "long payload accepted")
check(#data.load(GAME, 1) == 256, "payload capped at 256")

check(data.save("x", string.rep("t", 129), GAME, "", "", 1) == false,
      "long title rejected")
check(data.save("x", "T", GAME, string.rep("s", 129), "", 1) == false,
      "long subtitle rejected")
check(data.save("x", "T", GAME, "", string.rep("d", 1025), 1) == false,
      "long description rejected")

local bad_ids = { "", "..", "../EVIL", "a/b", "a\\b", "A:B", "A B",
                  string.rep("X", 14) }
for _, id in ipairs(bad_ids) do
    check(data.save("x", "T", id, "", "", 1) == false,
          "bad gameid rejected: " .. string.format("%q", id))
    check(data.load(id, 1) == nil,
          "bad gameid load nil: " .. string.format("%q", id))
    check(data.delete(id) == false,
          "bad gameid delete false: " .. string.format("%q", id))
end
check(not files.exists("../EVIL"), "traversal target not created")

local longest = string.rep("A", 13)
check(data.save("x", "T", longest, "", "", 1) == true,
      "13-character gameid accepted")
check(data.delete(longest) == true, "13-character gameid deleted")

check(data.delete(GAME) == true, "game directory deleted")
check(data.load(GAME, 0) == nil, "load after delete is nil")
check(not files.exists(DIR), "game directory removed")

-- Multi-root lookup: the host's second root stands in for PSP Go internal
-- storage / M2. Loads and deletes must search every root.
local GAME2 = "ROOTTEST"
local DIR2 = "data/PSP/SAVEDATA/" .. GAME2
data.delete(GAME2)
files.delete(DIR2)
files.mkdir(DIR2)

local function write_slot(name, body)
    local f = assert(io.open(DIR2 .. "/" .. name, "wb"))
    f:write(body)
    f:close()
end

write_slot("AUTOSAVE", "second-root-auto")
check(data.load(GAME2, 1) == "second-root-auto",
      "autosave found in second root")
check(data.load(GAME2, 0) == "second-root-auto",
      "dialog load falls back to second-root autosave")

write_slot("SLOT2", "second-root-two")
write_slot("SLOT4", "second-root-four")
check(data.load(GAME2, 0) == "second-root-four",
      "highest slot found across roots")

check(data.save("second-root-auto", "T", GAME2, "", "", 1) == true,
      "save stays on existing second root")
check(files.exists(DIR2 .. "/AUTOSAVE"), "second-root autosave updated")
check(not files.exists("PSP/SAVEDATA/" .. GAME2),
      "existing save not split into primary root")
check(data.save("second-root-one", "T", GAME2, "", "", 0) == true,
      "slot save stays on existing second root")
check(files.exists(DIR2 .. "/SLOT1"), "second-root slot created")
check(data.load(GAME2, 0) == "second-root-four",
      "higher slot in second root wins")

local GAME3 = "NEWROOT"
data.delete(GAME3)
check(data.save("primary", "T", GAME3, "", "", 1) == true,
      "new game uses first writable root")
check(files.exists("PSP/SAVEDATA/" .. GAME3 .. "/AUTOSAVE"),
      "new game created in primary root")
check(data.delete(GAME3) == true, "new-game save deleted")

files.mkdir("PSP/SAVEDATA/" .. GAME2)
local primary = assert(io.open("PSP/SAVEDATA/" .. GAME2 .. "/AUTOSAVE", "wb"))
primary:write("primary-copy")
primary:close()
check(data.delete(GAME2) == true, "delete removes both roots")
check(not files.exists("PSP/SAVEDATA/" .. GAME2),
      "primary-root save deleted")
check(not files.exists(DIR2), "second-root save deleted")

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
