-- OneLua sfont tests (run by test/host/font_test with ASSETS set).
-- assets/sfont.png: 94 glyphs '!'..'~', height 18, '!' (and space) 5 px.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

check(type(sfont) == "table", "sfont table registered")
for _, name in ipairs({ "load", "print", "letters", "width" }) do
    check(type(sfont[name]) == "function", "sfont." .. name .. " registered")
end
for _, name in ipairs({ "new", "load", "nchars", "cell", "draw", "print",
                        "textwidth", "free" }) do
    check(type(font[name]) == "function", "font." .. name .. " unchanged")
end

local sf = sfont.load(ASSETS .. "/sfont.png")
check(type(sf) == "number", "sfont.load returns a handle")
check(sfont.load(ASSETS .. "/missing.png") == nil, "missing file -> nil")
check(sfont.load(ASSETS .. "/font.png") == nil, "grid sheet has no markers -> nil")

check(sfont.letters(sf) == 94, "letters")
check(sfont.width(sf, "!") == 5, "'!' width")
check(sfont.width(sf, " ") == 5, "space uses '!' width")
check(sfont.width(sf, "") == 0, "empty width")
local wi, wm = sfont.width(sf, "i"), sfont.width(sf, "M")
check(wi < wm, "proportional: i narrower than M")
check(sfont.width(sf, "iM") == wi + wm, "width is additive")
check(font.textwidth(sf, "iM") == wi + wm, "font.textwidth accepts sfont handle")

blits()
local endx = sfont.print(sf, 10, 20, "iM")
local n, lx, ly, lw = blits()
check(endx == 10 + wi + wm, "print returns final x")
check(n == 2, "one blit per glyph")
check(lx == 10 + wi and ly == 20 and lw == wm, "second glyph placed after first")

endx = sfont.print(sf, 0, 0, "a b")
n = blits()
check(n == 2 and endx == 2 * sfont.width(sf, "a") + 5, "space advances, no blit")

endx = sfont.print(sf, 7, 0, "M\nM")
n, lx, ly = blits()
check(n == 2 and lx == 7 and ly == 18 and endx == 7 + wm, "newline")

local cw, ch = font.cell(sf)
check(ch == 18 and cw >= wm, "font.cell reports widest glyph and height")
check(font.nchars(sf) == 94, "font.nchars matches letters")

-- screen.print/screen.textwidth take sfont handles too.
endx = screen.print(0, 0, "iM", sf)
blits()
check(endx == wi + wm, "screen.print with sfont handle")
check(screen.textwidth("iM", sf) == wi + wm, "screen.textwidth with sfont handle")

-- Grid fonts are unaffected.
local f = font.new(ASSETS .. "/font.png", 8, 16)
check(type(f) == "number" and f ~= sf, "grid font loads beside sfont")
check(font.textwidth(f, "iM") == 16, "grid width unchanged")
check(sfont.width(f, "iM") == 16, "sfont.width wraps any handle")
check(font.print(f, 0, 0, "iM") == 16, "grid print unchanged")
blits()

font.free(sf)
check(sfont.letters(sf) == 0, "freed sfont handle")
font.free(f)

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
