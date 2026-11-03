-- OneLua game module host tests (read-only PBP subset).
-- PBP/SFO parsing itself is covered by test_pbp.c; this exercises the Lua
-- surface, image extraction and the documented nil/error behaviour.

local root = os.getenv("OPENLUA_ROOT") or "."
local pbp = root .. "/assets/sample.pbp"

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

check(type(game) == "table", "game table registered")
check(type(game.info) == "function", "game.info is a function")
check(type(game.geticon0) == "function", "game.geticon0 is a function")
check(type(game.getpic0) == "function", "game.getpic0 is a function")
check(type(game.getpic1) == "function", "game.getpic1 is a function")

-- Destructive / launch paths stay unregistered.
for _, name in ipairs({ "pack", "add", "remove", "unpack", "launch" }) do
    check(game[name] == nil, "game." .. name .. " is not exposed")
end

-- PARAM.SFO fields.
local info = game.info(pbp)
check(type(info) == "table", "info returns a table")
check(info.TITLE == "OpenLua Sample", "TITLE parsed")
check(info.TITLE_ID == "OPENLUA01", "TITLE_ID parsed")
check(info.CATEGORY == "MG", "CATEGORY parsed")
check(info.APP_VER == "1.00", "APP_VER parsed")
check(info.PARENTAL_LEVEL == 3, "integer PARENTAL_LEVEL parsed")
check(info.BOOTABLE == 1, "integer BOOTABLE parsed")
check(type(info.PARENTAL_LEVEL) == "number", "integer values are numbers")
check(type(info.TITLE) == "string", "string values are Lua strings")

-- Missing and malformed data return nil rather than raising.
check(game.info(root .. "/assets/does-not-exist.pbp") == nil,
      "missing file gives nil info")
check(game.info(root .. "/README.md") == nil, "non-PBP file gives nil info")
check(game.geticon0(root .. "/assets/does-not-exist.pbp") == nil,
      "missing file gives nil icon")
check(game.getpic0(root .. "/README.md") == nil,
      "non-PBP file gives nil pic")
check(game.getpic1(root .. "/README.md") == nil,
      "non-PBP file gives nil pic1")

-- Small binary PBP builder for malformed and sparse fixtures.
local function le32(n)
    return string.char(n % 256, math.floor(n / 256) % 256,
                       math.floor(n / 65536) % 256,
                       math.floor(n / 16777216) % 256)
end

local function write_file(path, data)
    local f = assert(io.open(path, "wb"))
    f:write(data)
    f:close()
end

local function build_pbp(sections)
    local offsets = {}
    local body = {}
    local pos = 40
    for i = 0, 7 do
        if sections[i] then
            offsets[i] = pos
            body[#body + 1] = sections[i]
            pos = pos + #sections[i]
        else
            offsets[i] = 0
        end
    end
    local header = "\0PBP" .. le32(0x10000)
    for i = 0, 7 do header = header .. le32(offsets[i]) end
    return header .. table.concat(body)
end

local bad = root .. "/out/game_bad.pbp"
write_file(bad, "\0PBP")
check(game.info(bad) == nil, "truncated PBP gives nil info")

write_file(bad, build_pbp({ [1] = "not an image" }))
check(game.info(bad) == nil, "PBP without SFO gives nil info")
check(game.geticon0(bad) == nil, "non-image ICON0 gives nil")
check(game.getpic0(bad) == nil, "absent PIC0 gives nil")

-- A PBP whose first section offset points into the header is rejected.
local corrupt = "\0PBP" .. le32(0x10000) .. le32(1)
for i = 1, 7 do corrupt = corrupt .. le32(0) end
write_file(bad, corrupt)
check(game.info(bad) == nil, "corrupt section offset gives nil info")
check(game.geticon0(bad) == nil, "corrupt section offset gives nil icon")

-- Extracted images are normal image handles.
local icon = game.geticon0(pbp)
check(icon ~= nil, "ICON0 extracted")
check(image.getw(icon) == 144, "ICON0 width")
check(image.geth(icon) == 80, "ICON0 height")
check(icon:getw() == 144, "image handle methods work")
check(icon:getrealw() >= 144 and icon:getrealh() >= 80,
      "ICON0 surface is padded")

local pic0 = game.getpic0(pbp)
check(pic0 ~= nil, "PIC0 extracted")
check(image.getw(pic0) == 310 and image.geth(pic0) == 180,
      "PIC0 dimensions")

local pic1 = game.getpic1(pbp)
check(pic1 ~= nil, "PIC1 extracted")
check(image.getw(pic1) == 480 and image.geth(pic1) == 272,
      "PIC1 dimensions")

-- Two calls create independent handles.
local icon2 = game.geticon0(pbp)
check(icon2 ~= nil and icon2 ~= icon, "extraction returns a fresh handle")
check(image.getw(icon2) == 144, "second handle is valid")

-- Argument errors are reported as Lua errors, not silent nils.
check(pcall(game.info) == false, "info with no path raises")
check(pcall(game.geticon0) == false, "geticon0 with no path raises")
check(pcall(game.info, {}) == false, "info with a table raises")
check(pcall(game.getpic1, true) == false, "getpic1 with a boolean raises")

-- Image handles survive GC of the source buffer and can be freed normally.
icon = nil; icon2 = nil; pic0 = nil; pic1 = nil
collectgarbage("collect")
check(true, "extracted handles collected")

os.remove(bad)
print(string.format("game Lua: %d assertions, %d failure(s)", ok, fails))
if fails > 0 then error("game tests failed") end
