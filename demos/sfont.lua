-- OpenLua - OneLua sfont compatibility demo.
--
--   sfont.load(path)              -> SFONT handle or nil
--   sfont.print(handle, x, y, s)  -> final X cursor
--   sfont.letters(handle)         -> glyph count
--   sfont.width(handle, s)        -> measured pixel width
--
-- assets/sfont.png is a classic SFont: magenta separators on its top row
-- and variable-width glyphs starting at ASCII '!'.  assets/font.png is the
-- fixed 8x16 sheet used by the original font demo.
--
-- Regenerate both sheets with: python3 tools/gen_font.py

local sf = sfont.load("assets/sfont.png")
if not sf then
    screen.clear(color.new(0x10, 0x00, 0x00))
    screen.flip()
    return
end

local fixed = font.new("assets/font.png", 8, 16)
if fixed then
    screen.font(fixed)
end

local white  = color.new(235, 240, 255)
local grey   = color.new(130, 142, 165)
local green  = color.new(40, 220, 120)
local yellow = color.new(250, 205, 60)
local cyan   = color.new(80, 200, 255)
local panel  = color.new(20, 28, 44)

local function render()

screen.clear(color.new(0x10, 0x12, 0x18))
draw.fillrect(12, 12, 456, 248, panel)
draw.rect(12, 12, 456, 248, grey)

if fixed then
    screen.print(20, 20, "OneLua sfont compatibility", fixed, white)
    screen.print(20, 38, "Classic SFont / variable width", fixed, grey)
end

-- sfont.print draws with the loaded sfont directly and returns the cursor.
sfont.print(sf, 20, 66, "sfont.print: Hello PSP!")
local endx = sfont.print(sf, 20, 84, "End X marker ->")
draw.rect(endx, 82, 1, 9, green)

-- sfont.width measures the same advances used by sfont.print.
local text = "Width: I W i w"
local width = sfont.width(sf, text)
sfont.print(sf, 20, 112, text)
draw.fillrect(20, 124, width, 2, yellow)

-- sfont.letters reports the number of glyphs in the sheet.
local letters = sfont.letters(sf)
if fixed then
    screen.print(20, 140, "letters = " .. letters, fixed, cyan)
    screen.print(20, 158, "width   = " .. width .. " px", fixed, cyan)
end

-- Show why the classic font is variable width: 'I' is narrower than 'W'.
local wi = sfont.width(sf, "I")
local ww = sfont.width(sf, "W")
sfont.print(sf, 20, 190, "I")
draw.rect(20, 188, wi, 11, green)
sfont.print(sf, 80, 190, "W")
draw.rect(80, 188, ww, 11, cyan)
if fixed then
    screen.print(130, 187, "I=" .. wi .. "px  W=" .. ww .. "px", fixed, grey)
end

sfont.print(sf, 20, 222, "sfont.load / print / width / letters")

screen.flip()
end

while true do
    render()
end
