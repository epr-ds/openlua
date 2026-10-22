-- OpenLua - OneLua power management demo.
--
-- Exercises only the safe calls:
--   power.plugged()  - is the AC adapter connected?
--   power.lock()     - lock the power switch
--   power.unlock()   - unlock it again
--   power.tick()     - keep the console awake
--
-- power.suspend() and power.off() are deliberately NOT called here.  They can
-- put a real PSP to sleep or standby, so they are left for scripts that
-- explicitly want that behaviour.  PPSSPP does not model them either.
--
-- The demo uses a built-in 5x7 vector font, so it needs no external assets.

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
    ["C"] = { ".###.", "#...#", "#....", "#....", "#....", "#...#", ".###." },
    ["D"] = { "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####." },
    ["E"] = { "#####", "#....", "#....", "####.", "#....", "#....", "#####" },
    ["F"] = { "#####", "#....", "#....", "####.", "#....", "#....", "#...." },
    ["G"] = { ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###." },
    ["H"] = { "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" },
    ["I"] = { ".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###." },
    ["K"] = { "#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#" },
    ["L"] = { "#....", "#....", "#....", "#....", "#....", "#....", "#####" },
    ["M"] = { "#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#" },
    ["N"] = { "#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#" },
    ["O"] = { ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." },
    ["P"] = { "####.", "#...#", "#...#", "####.", "#....", "#....", "#...." },
    ["R"] = { "####.", "#...#", "#...#", "####.", "##...", "#.#..", "#..#." },
    ["S"] = { ".####", "#....", "#....", ".###.", "....#", "....#", "####." },
    ["T"] = { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." },
    ["U"] = { "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." },
    ["V"] = { "#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.." },
    ["W"] = { "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#" },
    ["X"] = { "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#" },
    ["Y"] = { "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.." },
    ["."] = { ".....", ".....", ".....", ".....", ".....", ".##..", ".##.." },
    [":"] = { ".....", ".##..", ".##..", ".....", ".##..", ".##..", "....." },
    ["/"] = { "....#", "...#.", "...#.", "..#..", ".#...", ".#...", "#...." },
}

local white  = color.new(235, 240, 255)
local grey   = color.new(95, 105, 125)
local dim    = color.new(55, 65, 82)
local dark   = color.new(14, 20, 32)
local panel  = color.new(22, 30, 48)
local green  = color.new(40, 220, 120)
local yellow = color.new(250, 205, 60)
local red    = color.new(245, 75, 75)
local cyan   = color.new(80, 200, 255)

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

-- One lock/unlock pair at startup proves both calls work without leaving the
-- power switch locked when the demo is closed.
local lock_ok = power.lock()
local unlock_ok = power.unlock()
local tick_ok = false
local frame = 0

while true do
    frame = frame + 1
    screen.clear(dark)

    local plugged = power.plugged()
    tick_ok = (power.tick() == nil)

    draw_text("POWER", 20, 12, 2, white)
    draw.line(20, 34, 100, 34, cyan)

    draw.fillrect(16, 44, 448, 120, panel)
    draw.rect(16, 44, 448, 120, grey)

    local plug_color = plugged and green or dim
    draw.line(352, 86, 392, 86, plug_color)
    draw.fillrect(392, 72, 42, 28, plug_color)
    draw.fillrect(434, 78, 10, 4, grey)
    draw.fillrect(434, 90, 10, 4, grey)

    local cable_text = plugged and "PLUGGED YES" or "PLUGGED NO"
    draw_text(cable_text, 32, 58, 2, plugged and green or yellow)
    draw_text(plugged and "ON AC POWER" or "ON BATTERY", 32, 88, 1, white)

    draw_text("LOCK", 32, 112, 1, white)
    draw_text(lock_ok and "YES" or "NO", 140, 112, 1, lock_ok and green or red)
    if not lock_ok then
        draw_text("HLE N/A", 240, 112, 1, dim)
    end

    draw_text("UNLOCK", 32, 132, 1, white)
    draw_text(unlock_ok and "YES" or "NO", 140, 132, 1, unlock_ok and green or red)
    if not unlock_ok then
        draw_text("HLE N/A", 240, 132, 1, dim)
    end

    draw_text("TICK", 32, 152, 1, white)
    draw_text(tick_ok and "YES" or "NO", 140, 152, 1, tick_ok and green or red)
    if tick_ok and math.floor(frame / 20) % 2 == 0 then
        draw.fillrect(176, 152, 7, 7, green)
    end

    draw_text("SUSPEND", 32, 178, 1, dim)
    draw_text("NOT RUN", 180, 178, 1, grey)
    draw_text("OFF", 32, 198, 1, dim)
    draw_text("NOT RUN", 180, 198, 1, grey)

    draw_text("DESTRUCTIVE CALLS NOT RUN", 20, 232, 1, grey)
    screen.flip()
end
