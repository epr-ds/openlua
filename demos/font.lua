-- OpenLua - SFont bitmap font demo.
--
--   font.new(path, cellW, cellH)       -> handle or nil
--   screen.font(handle)                -> default font for screen.print
--   screen.print(x, y, text [, h, tint])
--   screen.textwidth(text [, h])       -> width in pixels
--
-- Asset: assets/font.png (8x16 cells, glyphs in ASCII order from space).
-- Regenerate it with: python3 tools/gen_font.py

local CELL_W, CELL_H = 8, 16
local f = font.new("assets/font.png", CELL_W, CELL_H)
if not f then
    screen.clear(color.new(0x00, 0x00, 0x10))
    screen.flip()
    return
end

screen.font(f)

screen.clear(color.new(0x10, 0x12, 0x18))
screen.print(20, 30, "OpenLua font demo")

local red    = color.new(255, 30, 30)
local green  = color.new(30, 220, 30)
local yellow = color.new(240, 200, 30)
screen.print(20, 60, "Red tint", f, red)
screen.print(20, 80, "Green tint", f, green)
screen.print(20, 100, "Yellow tint", f, yellow)

local w = screen.textwidth("Measuring text width")
draw.fillrect(200, 130, w, 2, green)
screen.print(200, 115, "width = " .. w .. " px", f)

screen.print(20, 160, "The quick brown fox 0123", f)
screen.print(20, 180, "abcdef XYZ ~ .,;:! ?", f)
screen.print(200, 160, "line one\nline two", f)

screen.flip()
