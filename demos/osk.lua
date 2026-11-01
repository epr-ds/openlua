-- OneLua - OneLua-compatible on-screen keyboard demo.
--
--   local text = osk.init(desc, initial [, type [, textmax [, lines [, mode]]]])
--
-- Confirming returns the edited string (which may be "").  Cancelling returns
-- nil.  This demo uses the default normal keyboard, English/Firmware language
-- and a 20-character limit, then draws the result in the normal 2D loop.
--
-- Interactive PPSSPP check:
--   * type a name and press Enter -> "OSK returned: ..."
--   * press Escape              -> "OSK cancelled (nil)"
--
-- The OSK is PSP-only; the host build validates the arguments and echoes the
-- text without opening a window.

local f = font.new("assets/font.png", 8, 16)
if f then
    screen.font(f)
end

local desc = "OpenLua OSK demo"
local initial = "Player"

local text = osk.init(desc, initial, 1, 20, 1, 1)

local headline, detail, accent
if text == nil then
    headline = "OSK cancelled"
    detail = "osk.init returned nil"
    accent = color.new(230, 96, 96)
elseif text == "" then
    headline = "OSK confirmed"
    detail = "returned an empty string"
    accent = color.new(235, 180, 70)
else
    headline = "OSK returned:"
    detail = text
    accent = color.new(80, 200, 150)
end

local white = color.new(238, 240, 246)
local muted = color.new(120, 132, 154)
local dark  = color.new(10, 14, 26)

while true do
    screen.clear(dark)
    draw.fillrect(0, 0, 480, 4, accent)
    draw.fillrect(0, 268, 480, 4, color.new(48, 180, 160))

    if f then
        screen.print(24, 52, "OneLua osk.init", f, accent)
        screen.print(24, 88, "Description: " .. desc, f, muted)
        screen.print(24, 116, "Initial text: " .. initial, f, muted)
        screen.print(24, 160, headline, f, white)
        screen.print(24, 188, detail, f, accent)
        screen.print(24, 228, "Text is returned as UTF-8", f, muted)
    end

    -- Fill-only border: PPSSPP's software renderer can stall on line
    -- primitives, so the demo avoids draw.rect/draw.line.
    local edge = color.new(44, 52, 72)
    draw.fillrect(16, 40, 448, 2, edge)
    draw.fillrect(16, 242, 448, 2, edge)
    draw.fillrect(16, 42, 2, 200, edge)
    draw.fillrect(462, 42, 2, 200, edge)
    screen.flip()
end
