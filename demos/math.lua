-- OpenLua - OneLua math helpers demo.
--
--   math.minmax(x, min, max)
--   math.bitnot(bit)
--   math.bitand(a, b)
--   math.bitor(a, b)
--
-- Bitwise results use 32-bit signed semantics.  Standard Lua math functions
-- remain available.  This demo uses a tiny 5x7 vector font so it does not
-- depend on external assets.

local FONT = {
    ["0"] = { ".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###." },
    ["1"] = { "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###." },
    ["2"] = { ".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####" },
    ["3"] = { "#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###." },
    ["4"] = { "...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#." },
    ["5"] = { "#####", "#....", "#....", "####.", "....#", "#...#", ".###." },
    ["6"] = { ".###.", "#....", "#....", "####.", "#...#", "#...#", ".###." },
    ["7"] = { "#####", "....#", "...#.", "..#..", "..#..", "..#..", "..#.." },
    ["8"] = { ".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###." },
    ["9"] = { ".###.", "#...#", "#...#", ".####", "....#", "....#", ".###." },
    ["-"] = { ".....", ".....", ".....", "#####", ".....", ".....", "....." },
    [" "] = { ".....", ".....", ".....", ".....", ".....", ".....", "....." },
    ["A"] = { ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" },
    ["B"] = { "####.", "#...#", "#...#", "####.", "#...#", "#...#", "####." },
    ["D"] = { "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####." },
    ["E"] = { "#####", "#....", "#....", "####.", "#....", "#....", "#####" },
    ["G"] = { ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###." },
    ["I"] = { ".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###." },
    ["M"] = { "#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#" },
    ["N"] = { "#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#" },
    ["O"] = { ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." },
    ["R"] = { "####.", "#...#", "#...#", "####.", "##...", "#.#..", "#..#." },
    ["S"] = { ".####", "#....", "#....", ".###.", "....#", "....#", "####." },
    ["T"] = { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." },
    ["W"] = { "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#" },
    ["X"] = { "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#" },
}

local white  = color.new(235, 240, 255)
local green  = color.new(40, 220, 120)
local yellow = color.new(250, 205, 60)
local blue   = color.new(45, 65, 125)

local function draw_text(text, x, y, scale, tint)
    for i = 1, #text do
        local glyph = FONT[text:sub(i, i)]
        if glyph then
            for row = 1, 7 do
                local line = glyph[row]
                for col = 1, 5 do
                    if line:sub(col, col) == "#" then
                        draw.fillrect(x + (i - 1) * 6 * scale + (col - 1) * scale,
                                      y + (row - 1) * scale,
                                      scale, scale, tint)
                    end
                end
            end
        end
    end
end

local function draw_bits(value, x, y)
    for bit = 0, 7 do
        local on = math.bitand(value, 2 ^ bit) ~= 0
        draw.fillrect(x + bit * 14, y, 10, 10, on and yellow or blue)
    end
end

local frame = 0
while true do
    frame = frame + 1
    screen.clear(color.new(12, 18, 34))

    -- A marker clamped by minmax to the middle of the screen.
    draw.rect(40, 14, 400, 10, white)
    local marker = math.minmax((frame * 5) % 520, 40, 440)
    draw.fillrect(marker, 14, 8, 10, green)

    draw_text("MINMAX", 20, 34, 2, white)

    -- Values 30, 0 and 12 clamped into [0, 30].
    local v1 = math.minmax(45, 0, 30)
    local v2 = math.minmax(-5, 0, 10)
    local v3 = math.minmax(12, 0, 30)
    local rows = { { v1, 30 }, { v2, 10 }, { v3, 30 } }
    for i, row in ipairs(rows) do
        local y = 54 + (i - 1) * 24
        local value = row[1]
        local max = row[2]
        draw_text(tostring(value), 20, y, 1, white)
        draw.rect(60, y, max * 7, 12, white)
        if value > 0 then
            draw.fillrect(61, y + 1, value * 7, 10, green)
        end
    end

    draw_text("BITWISE", 20, 130, 2, white)
    local andv = math.bitand(0xAA, 0xCC)
    local orv  = math.bitor(0xAA, 0xCC)
    local notv = math.bitnot(0x0F)
    local bitrows = { { "AND", andv }, { "OR", orv }, { "NOT", notv } }
    for i, row in ipairs(bitrows) do
        local y = 156 + (i - 1) * 28
        draw_text(row[1], 20, y + 2, 1, white)
        draw_bits(row[2], 64, y)
        draw_text(tostring(row[2]), 192, y + 2, 1, yellow)
    end
    draw_text("32-BIT SIGNED", 20, 246, 1, blue)

    screen.flip()
end
