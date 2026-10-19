-- Phase-3 host tests (run by test/host/host_main)
-- Exercises the OneLua-compatible files / ini / data APIs exactly as a
-- script would, on the POSIX backend of engine/fs (no PSP needed).

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

local ROOT = "t3work"          -- relative dir used for all FS tests (pre-cleaned by run.sh)

--[[ ============ files ============ ]]
check(files.exists(ROOT .. "/zzz_missing") == false, "exists(false)")

files.mkdir(ROOT .. "/a/b")
check(files.exists(ROOT .. "/a/b"), "mkdir -p chain")
check(files.exists(ROOT .. "/a"), "mkdir -p parent")

-- write a small file
local f = io.open(ROOT .. "/a/hi.txt", "w"); f:write("hello world\n"); f:close()
check(files.exists(ROOT .. "/a/hi.txt") == true, "exists(true) after write")
check(files.size(ROOT .. "/a/hi.txt") == 12, "size == 12")
check(files.ext(ROOT .. "/a/hi.txt") == "txt", "ext txt")
check(files.nopath(ROOT .. "/a/hi.txt") == "hi.txt", "nopath -> hi.txt")
check(files.nofile(ROOT .. "/a/hi.txt") == ROOT .. "/a", "nofile -> dir")

-- copy
check(files.copy(ROOT .. "/a/hi.txt", ROOT .. "/a/hi2.txt") == 1, "copy ok")
local g = io.open(ROOT .. "/a/hi2.txt"); check(g and g:read("*a") == "hello world\n"); if g then g:close() end

-- rename
check(files.rename(ROOT .. "/a/hi2.txt", ROOT .. "/a/hi3.txt") == 1, "rename ok")
check(files.exists(ROOT .. "/a/hi3.txt") and not files.exists(ROOT .. "/a/hi2.txt"), "rename moved")

-- move
check(files.move(ROOT .. "/a/hi3.txt", ROOT .. "/hi.mov") == 1, "move ok")
check(files.exists(ROOT .. "/hi.mov") and not files.exists(ROOT .. "/a/hi3.txt"), "move moved")

-- list (files present: a/ dir, hi.mov)
local L = files.list(ROOT)
check(type(L) == "table" and #L >= 2, "list >= 2 entries")
local names = {}
for _, e in ipairs(L or {}) do names[e.name] = e end
check(names["a"] and names["a"].directory == true, "list dir flagged")
check(names["hi.mov"] and names["hi.mov"].directory == false, "list file flagged")
check(type(names["hi.mov"].mode) == "string" and #names["hi.mov"].mode == 3, "mode string")

local LF = files.listfiles(ROOT)
check(#LF == 1 and LF[1].name == "hi.mov", "listfiles filters")
local LD = files.listdirs(ROOT)
check(#LD == 1 and LD[1].name == "a", "listdirs filters")

-- delete tree
check(files.delete(ROOT .. "/a") == true, "delete ok")
check(files.exists(ROOT .. "/a") == false, "deleted")

--[[ ============ ini ============ ]]
local inipath = ROOT .. "/cfg.ini"

-- fresh file: no section
check(ini.read(inipath, "", "key1", "DEF") == "DEF", "ini miss -> default")
ini.write(inipath, "", "g1", "v")
check(ini.read(inipath, "", "g1") == "v", "ini no-section read")

-- with section
ini.write(inipath, "sec", "k1", "one")
ini.write(inipath, "sec", "k2", "two")
check(ini.read(inipath, "sec", "k1") == "one", "ini sec read 1")
check(ini.read(inipath, "sec", "k2") == "two", "ini sec read")
check(ini.read(inipath, "sec", "missing", "D") == "D", "ini sec default")

-- replace in section
ini.write(inipath, "sec", "k1", "ONE")
check(ini.read(inipath, "sec", "k1") == "ONE", "ini replace value")
check(ini.read(inipath, "sec", "k2") == "two", "ini sibling intact after replace")

-- second section created
ini.write(inipath, "other", "x", "y")
check(ini.read(inipath, "other", "x") == "y", "ini second section")
check(ini.read(inipath, "sec", "k1") == "ONE", "ini original section intact")

-- content sanity: one [sec] header, k1 exactly once
local h =  io.open(inipath, "r"); local body = h:read("*a"); h:close()
check(select(2, body:gsub("[s][e][c]", "%0")) == 1, "ini single [sec] header")
local k1_ct = 0; for _ in body:gmatch("[k]1[ ]*=") do k1_ct = k1_ct + 1 end
check(k1_ct == 1, "ini k1 appears once")

--[[ ============ data (savedata) ============ ]]
check(data.save("payload-42", "Game", "TESTGAME", "sub", "desc", 1) == true, "data.save autosave")
check(data.load("TESTGAME", 1) == "payload-42", "data.load autosave")

check(data.save("slot-one", "Game", "TESTGAME", "sub", "desc", 0) == true, "data.save slot")
check(data.load("TESTGAME", 0) == "slot-one", "data.load most-recent slot")
check(data.load("TESTGAME", 1) == "payload-42", "autosave untouched by slot save")

check(data.save("slot-two", "Game", "TESTGAME", "sub", "desc", 0) == true, "second slot save")
check(data.load("TESTGAME", 0) == "slot-two", "newest slot wins")

check(data.save("cap-" .. string.rep("x", 300), "G", "TESTGAME", "s", "d", 1) == true, "256+ truncates")
local capped = data.load("TESTGAME", 1)
check(#capped == 256, "payload capped at 256 (got " .. #capped .. ")")

check(data.save("final", "G", "TESTGAME", "s", "d", 0) == true, "slot save again")
check(data.load("NOSUCHGAME", 0) == nil, "missing game -> nil")

check(data.delete("TESTGAME") == true, "data.delete")
check(data.load("TESTGAME", 0) == nil, "loaded nothing after delete")

--[[ ============ summary ============ ]]
print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
