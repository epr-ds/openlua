-- OpenLua - Font / SFont demo (script.lua)
--
-- Demonstrates the bitmap font pipeline:
--   font.new(path, cellW, cellH)  -> font handle
--   screen.font(handle)           -> set default font for screen.print
--   screen.print(x, y, text)      -> draw text (OneLua-compatible)
--   screen.print(x, y, text, h, tint) -> with explicit font + ABGR tint
--   screen.textwidth(text)        -> measure string width in px
--
-- Requires a PNG spritesheet "font.png" next to this script:
--   one row (or grid) of glyphs, cellW x cellH pixels each,
--   characters in ASCII order starting with space (0x20).
--
-- A minimal 8x16 sheet can be generated with:
--   python3 tools/gen_font.py

local CELL_W, CELL_H = 8, 16

local f = font.new("font.png", CELL_W, CELL_H)
if not f then
    -- no font available; fall back to a textless status screen
    screen.clear(color.new(0x00, 0x00, 0x10))
    screen.flip()
    return
end

screen.font(f)                              -- default font from here on

-- background
screen.clear(color.new(0x10, 0x12, 0x18))

-- white main title
screen.print(20, 30, "OpenLua font demo", f)

-- subtitle with tint (OneLua-style color argument)
local red    = color.new(255, 30, 30)
local green  = color.new(30, 220, 30)
local yellow = color.new(240, 200, 30)
screen.print(20, 60, "Red tint", f, red)
screen.print(20, 80, "Green tint", f, green)
screen.print(20, 100, "Yellow tint", f, yellow)

-- width measurement + a rule line under it
local w = screen.textwidth("Measuring 760px sheet")
draw.fillrect(200, 130, w, 2, green)
screen.print(200, 115, "width = " .. w .. " px", f)

-- ASCII coverage sweep of the loaded sheet
screen.print(20, 160, "The quick brown fox 0123", f)
screen.print(20, 180, "abcdef XYZ ~ .,;:! ?", f)

-- newline / tab behavior in the printer
screen.print(200, 160, "line one\nline two", f)

screen.flip()
