-- OneLua PGF font API tests (run by test/host/host_main).
-- sceLibFont is stubbed on the host; PPSSPP verifies the real system PGF.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

check(type(font) == "table", "font table registered")
check(type(font.load) == "function", "font.load registered")
check(type(font.setdefault) == "function", "font.setdefault registered")
check(font.setdefault() == nil, "no default font initially")

check(font.load("../assets/missing.pgf") == nil, "missing PGF returns nil")

local f = font.load("../assets/test.pgf")
check(f ~= nil, "PGF loads through font.load")
check(font.nchars(f) == 95, "PGF exposes printable ASCII")
local cw, ch = font.cell(f)
check(cw == 7 and ch == 20, "PGF cell metrics")
check(font.textwidth(f, "AB") == 14, "PGF width")
check(font.textwidth("AB", f) == 14, "PGF width text-first form")
check(font.print(f, 10, 20, "AB") == 24, "PGF print returns final X")

font.setdefault(f)
check(font.setdefault() == f, "font.setdefault selects a handle")
check(screen.print(10, 20, "AB") == 24, "screen.print uses the PGF default")
check(screen.textwidth("AB") == 14, "screen.textwidth uses the PGF default")

font.setdefault("../assets/test.pgf")
local g = font.setdefault()
check(g ~= nil and g ~= f, "font.setdefault(path) loads a new handle")
check(screen.print(10, 20, "A B") == 29, "screen.print keeps PGF spacing")

-- Existing SFont users keep their paths: fixed-cell load alias and font.new.
local fixed = font.load("../assets/font.png", 8, 16)
check(fixed ~= nil, "font.load fixed-cell alias still works")
check(font.textwidth(fixed, "AB") == 16, "fixed-cell alias keeps metrics")
local fixed_new = font.new("../assets/font.png", 8, 16)
check(fixed_new ~= nil, "font.new still works")
font.free(fixed_new)
font.free(fixed)

font.free(g)
font.free(f)
check(font.setdefault() == nil, "freeing the default clears it")

print("pgf Lua: " .. ok .. " assertions, " .. fails .. " failure(s)")
if fails > 0 then error("pgf Lua tests failed") end
