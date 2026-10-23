-- OneLua sfont compatibility tests (run by test/host/host_main).
-- The classic asset uses magenta top-row separators and variable widths;
-- assets/font.png is the fixed 95-cell fallback.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

check(type(sfont) == "table", "sfont table registered")
check(type(sfont.load) == "function", "sfont.load registered")
check(type(sfont.print) == "function", "sfont.print registered")
check(type(sfont.letters) == "function", "sfont.letters registered")
check(type(sfont.width) == "function", "sfont.width registered")

local sf = sfont.load("../assets/sfont.png")
check(sf ~= nil, "classic SFONT loads")
check(sfont.letters(sf) == 94, "classic letters count")
check(sfont.width(sf, "I") > 0, "classic I width")
check(sfont.width(sf, "W") > sfont.width(sf, "I"),
      "classic W is wider than I")
local endx = sfont.print(sf, 10, 20, "AB")
check(endx == 10 + sfont.width(sf, "AB"),
      "classic print returns final X")
check(sfont.print(sf, 10, 20, "A B") == 10 + sfont.width(sf, "A B"),
      "classic print handles spaces")

local fixed = sfont.load("../assets/font.png")
check(fixed ~= nil, "fixed-grid fallback loads")
check(sfont.letters(fixed) == 95, "fixed-grid letters count")
check(sfont.width(fixed, "AB") == 16, "fixed-grid width is 2 cells")
check(sfont.print(fixed, 10, 20, "AB") == 26,
      "fixed-grid print returns final X")

-- The existing fixed-cell font API must remain unchanged and handle-compatible.
local f = font.new("../assets/font.png", 8, 16)
check(f ~= nil, "font.new still loads")
check(font.textwidth(f, "AB") == 16, "font.textwidth unchanged")
check(font.textwidth("AB", f) == 16, "font.textwidth text-first form unchanged")

check(sfont.load("../assets/not-a-font.png") == nil,
      "missing SFONT returns nil")

font.free(sf)
font.free(fixed)
font.free(f)

print("sfont Lua: " .. ok .. " assertions, " .. fails .. " failure(s)")
if fails > 0 then error("sfont Lua tests failed") end
