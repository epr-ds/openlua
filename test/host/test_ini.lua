-- INI host tests (run by test/host/host_main).

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

local ROOT = "iniwork"
files.delete(ROOT)
files.mkdir(ROOT)

local function write_file(path, body)
    local f = assert(io.open(path, "wb"))
    f:write(body)
    f:close()
end

local function read_file(path)
    local f = assert(io.open(path, "rb"))
    local body = f:read("*a")
    f:close()
    return body
end

local function count_plain(hay, needle)
    local n, pos = 0, 1
    while true do
        local a, b = hay:find(needle, pos, true)
        if not a then break end
        n = n + 1
        pos = b + 1
    end
    return n
end

local p = ROOT .. "/sections.ini"
write_file(p, "top=0\n\n[a]\na1=1\n\n[b]\nb1=2\n\n[c]\nc1=3\n")

-- Middle section: insert before the next header.
ini.write(p, "b", "b2", "22")
local body = read_file(p)
local bpos = body:find("[b]", 1, true)
local cpos = body:find("[c]", 1, true)
local kpos = body:find("b2 = 22", 1, true)
check(ini.read(p, "b", "b2") == "22", "read middle insertion")
check(bpos and cpos and kpos and bpos < kpos and kpos < cpos,
      "middle key before next header")

-- First and last sections.
ini.write(p, "a", "a2", "12")
body = read_file(p)
local a2pos = body:find("a2 = 12", 1, true)
local bpos2 = body:find("[b]", 1, true)
check(a2pos and bpos2 and a2pos < bpos2, "first-section key before [b]")

ini.write(p, "c", "c2", "32")
body = read_file(p)
local c2pos = body:find("c2 = 32", 1, true)
check(c2pos and c2pos > cpos, "last-section key after [c]")
check(ini.read(p, "c", "c2") == "32", "read last insertion")

-- Replace an existing key in place.
ini.write(p, "b", "b1", "200")
body = read_file(p)
check(ini.read(p, "b", "b1") == "200", "replace middle value")
check(count_plain(body, "b1 = 200") == 1, "replacement appears once")
check(count_plain(body, "b1=2") == 0, "old value removed")
check(count_plain(body, "[b]") == 1, "section header not duplicated")

-- Append a new section after all existing ones.
ini.write(p, "d", "d1", "4")
body = read_file(p)
local dpos = body:find("[d]", 1, true)
local d1pos = body:find("d1 = 4", 1, true)
check(dpos and d1pos and dpos < d1pos, "new section appended")
check(ini.read(p, "d", "d1") == "4", "read appended section")

-- Sectionless keys sit before the first section header.
local p2 = ROOT .. "/nosection.ini"
write_file(p2, "top=0\n\n[a]\na=1\n[b]\nb=2\n")
ini.write(p2, "top", "10")
body = read_file(p2)
local top10 = body:find("top = 10", 1, true)
local apos = body:find("[a]", 1, true)
check(top10 and apos and top10 < apos, "replace sectionless key")
check(ini.read(p2, "top") == "10", "read replaced sectionless key")

ini.write(p2, "top2", "2")
body = read_file(p2)
local top2 = body:find("top2 = 2", 1, true)
apos = body:find("[a]", 1, true)
check(top2 and apos and top2 < apos, "insert sectionless key")
check(ini.read(p2, "top2") == "2", "read inserted sectionless key")
check(count_plain(body, "[]") == 0, "no synthetic empty section")

-- Empty file and new section creation.
local p3 = ROOT .. "/empty.ini"
ini.write(p3, "k", "v")
check(read_file(p3) == "k = v\n", "empty sectionless file")
check(ini.read(p3, "k") == "v", "read empty-file key")

local p4 = ROOT .. "/newsection.ini"
ini.write(p4, "s", "k", "v")
check(read_file(p4) == "[s]\nk = v\n", "new section on empty file")

-- Comments are preserved and keys stay in the right section.
local p5 = ROOT .. "/comments.ini"
write_file(p5, "[a]\n; keep me\na=1\n[b]\nb=2\n")
ini.write(p5, "a", "a", "9")
body = read_file(p5)
check(body:find("; keep me", 1, true) ~= nil, "comment preserved")
check(ini.read(p5, "a", "a") == "9", "comment section updated")
check(ini.read(p5, "b", "b") == "2", "comment sibling intact")

-- No-section file with no headers.
local p6 = ROOT .. "/flat.ini"
write_file(p6, "x=1\ny=2\n")
ini.write(p6, "z", "3")
check(ini.read(p6, "z") == "3", "append sectionless key")
check(ini.read(p6, "x") == "1", "existing flat key intact")

files.delete(ROOT)

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
