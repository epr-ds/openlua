-- OneLua compatibility-layer tests (run by test/host/host_main).
-- Covers compound assignment, named colors, oneLua screen.* signatures and
-- the .pgf -> sibling .png atlas fallback.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

-- ---- compound assignment ------------------------------------------------
local n = 1
n += 2; check(n == 3, "local +=")
n *= 4; check(n == 12, "local *=")
n -= 2; check(n == 10, "local -=")
n /= 4; check(n == 2.5, "local /=")
n %= 2; check(n == 0.5, "local %=")
n ^= 2; check(n == 0.25, "local ^=")

local t = { v = 10 }
t.v += 5; check(t.v == 15, "field +=")
t["v"] -= 3; check(t.v == 12, "index -=")

-- The indexed target must be evaluated once, and the RHS must read the old
-- value (lowered as `target = target OP rhs`).
local calls = 0
local function key() calls += 1; return "v" end
t[key()] += 1
check(calls == 1, "index key evaluated once")
check(t.v == 13, "indexed += writes the result")

local tcalls = 0
local function tbl() tcalls += 1; return t end
tbl().v += 1
check(tcalls == 1, "table expression evaluated once")
check(t.v == 14, "table expression += writes the result")

local a = 2
a += a * 3
check(a == 8, "RHS reads the old value")

-- ---- named colors ------------------------------------------------------
check(type(color) == "table", "color table registered")
check(type(color.loadpalette) == "function", "color.loadpalette registered")
check(color.white == color.new(255, 255, 255, 255), "color.white")
check(color.black == color.new(0, 0, 0, 255), "color.black")
check(color.red   == color.new(255, 0, 0, 255),   "color.red")
check(color.transparent == 0, "color.transparent")
check(color.loadpalette() == nil, "color.loadpalette is a no-op")

-- ---- OneLua alignment constants ---------------------------------------
check(__ALEFT == 0, "__ALEFT")
check(__ACENTER == 1, "__ACENTER")
check(__ARIGHT == 2, "__ARIGHT")
check(type(__AFULL) == "number", "__AFULL")
check(type(__SLEFT) == "number" and type(__SRIGHT) == "number" and
      type(__STHROUGH) == "number" and type(__SSEESAW) == "number",
      "__S* shadow-style constants")

-- ---- table.unpack -----------------------------------------------------
check(type(table.unpack) == "function", "table.unpack alias")
local ua, ub, uc = table.unpack({ 1, 2, 3 })
check(ua == 1 and ub == 2 and uc == 3, "table.unpack works")

-- ---- OneLua timer module ---------------------------------------------
check(type(timer) == "table", "timer table registered")
check(type(timers) == "table", "timers table still registered")
check(type(timer.new) == "function", "timer.new")
check(type(timer.start) == "function", "timer.start")
check(type(timer.time) == "function", "timer.time")

local clock = timer.new()
timer.start(clock)
local t0 = timer.time(clock)
check(type(t0) == "number" and t0 >= 0, "timer.time returns ms")
check(timer.stoptime(clock) >= t0, "timer.stoptime")

-- ---- OneLua screen API -------------------------------------------------
check(type(screen.textheight) == "function", "screen.textheight registered")

local f = font.load("../assets/test.pgf")
check(f ~= nil, "host PGF loads")
check(font.cell(f) == 7 and select(2, font.cell(f)) == 20,
      "host PGF cell metrics")

-- font-first form
check(screen.textwidth(f, "AB", 1.0) == 14, "textwidth(font, text, 1.0)")
check(screen.textwidth(f, "AB", 0.5) == 7,  "textwidth(font, text, 0.5)")
check(screen.textheight(f, 1.0) == 20, "textheight(font, 1.0)")
check(screen.textheight(f, 0.5) == 10, "textheight(font, 0.5)")

local ex = screen.print(f, 10, 20, "AB", 1.0, color.white, color.black, __ALEFT)
check(ex == 24, "print(font, x, y, text, size, color, shadow, align)")
check(type(screen.print(f, 10, 20, "AB", 0.5, color.red, color.black,
                        __ACENTER)) == "number",
      "print with center alignment")
check(type(screen.print(f, 10, 20, "AB", 0.5, color.white, color.black,
                        __ARIGHT)) == "number",
      "print with right alignment")

-- text-first form uses the default font, with a size argument
font.setdefault(f)
check(screen.textwidth("AB", 0.5) == 7, "textwidth(text, size) uses default")
check(screen.textheight(0.5) == 10, "textheight(size) uses default")
check(type(screen.print(10, 20, "AB", 0.5, color.white, color.black,
                        __ALEFT)) == "number",
      "print(x, y, text, size, color, shadow, align)")
check(type(screen.print(10, 20, "AB")) == "number",
      "print(x, y, text) uses defaults")

-- Lines longer than the internal stack buffer must not be truncated.
local long = string.rep("A", 300)
check(screen.print(0, 0, long, 1.0) == 2100,
      "print does not truncate long lines")

-- legacy forms still work
check(screen.textwidth("AB", f) == 14, "legacy textwidth(text, handle)")
check(screen.print(10, 20, "AB", f, color.red) == 24,
      "legacy print(x, y, text, handle, tint)")
check(screen.print(10, 20, "AB", f) == 24, "legacy print(x, y, text, handle)")

-- ---- .pgf -> sibling .png atlas fallback ------------------------------
local src = io.open("../assets/font.png", "rb")
check(src ~= nil, "font.png available for fallback test")
if src then
    local data = src:read("*a")
    src:close()
    local dst = io.open("not-a-font.png", "wb")
    dst:write(data)
    dst:close()

    -- The host sceLibFont stub rejects paths containing "not-a-font",
    -- so font.load() must fall back to not-a-font.png.
    local fb = font.load("not-a-font.pgf")
    check(fb ~= nil, "pgf load falls back to sibling png")
    if fb then
        check(font.nchars(fb) == 95, "fallback atlas exposes 95 glyphs")
        check(font.cell(fb) == 8 and select(2, font.cell(fb)) == 16,
              "fallback atlas cell metrics")
        check(screen.textwidth(fb, "AB", 1.0) == 16,
              "fallback atlas text width")
        font.free(fb)
    end
    os.remove("not-a-font.png")
end

print(("compat: %d ok, %d fail"):format(ok, fails))
if fails > 0 then os.exit(1) end
