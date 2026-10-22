-- OpenLua - OneLua sfont demo.
--
--   sfont.load(path)               -> handle or nil
--   sfont.print(h, x, y, text)     -> final cursor X
--   sfont.letters(h)               -> number of glyphs in the strip
--   sfont.width(h, text)           -> text width in pixels
--
-- The proportional strip is drawn in its own colours; the grid font labels
-- each line and every measured width is underlined to its exact length.
--
-- Assets: assets/sfont.png (SFont strip, '!'..'~', 18 px tall) and
--         assets/font.png (8x16 grid). Regenerate with
--         python3 tools/gen_sfont.py / python3 tools/gen_font.py

local sf = sfont.load("assets/sfont.png")
local f  = font.new("assets/font.png", 8, 16)

local dark  = color.new(14, 20, 32)
local grey  = color.new(120, 132, 152)
local white = color.new(235, 240, 255)
local green = color.new(40, 220, 120)
local red   = color.new(240, 70, 70)

if not sf or not f then
    while true do
        screen.clear(red)
        screen.flip()
    end
end

screen.font(f)
screen.setcolor(grey)

local LINES = {
    "SFont: Hello PSP!",
    "iiii vs MMMM",
    "0123456789 +-*/",
}

while true do
    screen.clear(dark)

    screen.print(20, 14, "sfont.letters = " .. sfont.letters(sf))

    local y = 40
    for _, text in ipairs(LINES) do
        local w = sfont.width(sf, text)
        local endx = sfont.print(sf, 20, y, text)
        draw.fillrect(20, y + 19, w, 2, endx == 20 + w and green or red)
        screen.print(300, y + 2, "w=" .. w .. " end=" .. endx)
        y = y + 34
    end

    -- Same text through the grid font for comparison.
    screen.print(20, y, "grid: iiii vs MMMM", f, white)
    screen.print(300, y, "w=" .. font.textwidth(f, "grid: iiii vs MMMM"))

    -- sfont handles work with screen.print too.
    screen.print(20, y + 30, "via screen.print", sf, 0xFFFFFFFF)

    screen.flip()
end
