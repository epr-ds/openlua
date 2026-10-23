-- OpenLua - OneLua PGF font compatibility demo.
--
--   font.load("flash0:/font/ltn0.pgf") -> PGF handle or nil
--   font.setdefault(handle)             -> screen.print uses it
--   font.setdefault()                   -> current default handle
--   font.cell(handle)                   -> cell width, line height
--   font.textwidth(handle, text)        -> measured width
--
-- ltn0.pgf is the PSP's Latin system font. The engine reads it through
-- sceLibFont and caches printable ASCII in a <=512px RGBA atlas. No Sony
-- font file is redistributed with OpenLua. If the system path is missing,
-- the demo falls back to the fixed-cell PNG so it still has output.

local pgf = font.load("flash0:/font/ltn0.pgf")

if not pgf then
    local fallback = font.new("assets/font.png", 8, 16)
    if fallback then
        screen.font(fallback)
    end
    while true do
        screen.clear(color.new(0x18, 0x0a, 0x0a))
        if fallback then
            screen.print(20, 30, "PGF not available on this system")
            screen.print(20, 52, "Tried: flash0:/font/ltn0.pgf")
            screen.print(20, 74, "Falling back to font.new PNG")
        end
        screen.flip()
    end
end

font.setdefault(pgf)

local white  = color.new(235, 240, 255)
local grey   = color.new(145, 158, 180)
local green  = color.new(40, 220, 120)
local yellow = color.new(250, 205, 60)
local cyan   = color.new(80, 200, 255)
local panel  = color.new(20, 28, 44)

local text = "Width follows PGF advances"
local width = font.textwidth(pgf, text)
local cw, ch = font.cell(pgf)
local nc = font.nchars(pgf)

local function render()
    screen.clear(color.new(0x10, 0x12, 0x18))
    draw.fillrect(12, 12, 456, 248, panel)
    draw.rect(12, 12, 456, 248, grey)

    screen.print(20, 20, "OneLua PGF font", nil, white)
    screen.print(20, 38, "flash0:/font/ltn0.pgf", nil, grey)

    -- screen.print with no explicit handle uses font.setdefault().
    screen.print(20, 68, "PGF text: Hello, PSP!", nil, cyan)
    screen.print(20, 88, "The quick brown fox 0123", nil, white)
    screen.print(20, 108, "abcdef XYZ ~ .,;:!?", nil, white)

    screen.print(20, 138, text, nil, green)
    draw.fillrect(20, 154, width, 2, yellow)
    screen.print(20, 160, "width = " .. width .. " px", nil, grey)
    screen.print(20, 180, "cell = " .. cw .. " x " .. ch .. " px", nil, grey)
    screen.print(20, 198, "nchars = " .. nc, nil, grey)

    -- Explicit handle form (screen.print(x, y, text, handle, tint)).
    screen.print(20, 222, "font.setdefault / font.load", pgf, yellow)

    screen.flip()
end

while true do
    render()
end
