-- OpenLua - OneLua os system-info demo.
--
-- Exercises the read-only system queries:
--   os.nick(), os.language(), os.mac(), os.cfw(), os.versiontxt()
--   os.getdate(), os.cpu(), os.bus(), os.ram(), os.totalram()
--   os.infoms0(), os.infoef0()
--
-- os.delay() is safe but not needed in this loop.  os.exit(), os.restart(),
-- os.message()/os.messagebox() and the PRX helpers are deliberately NOT
-- called: they exit/relaunch the console or are unsupported stubs.  The bottom
-- status line says so explicitly.
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
    ["@"] = { ".###.", "#...#", "#.###", "#.#.#", "#.###", "#....", ".###." },
}

local white  = color.new(235, 240, 255)
local grey   = color.new(120, 132, 152)
local dim    = color.new(55, 65, 82)
local dark   = color.new(14, 20, 32)
local panel  = color.new(22, 30, 48)
local green  = color.new(40, 220, 120)
local yellow = color.new(250, 205, 60)
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

local MB = 1024 * 1024

local function clean(text, limit)
    text = string.upper(tostring(text or ""))
    text = text:gsub("[\r\n\t]+", " ")
    if text == "" then text = "N/A" end
    return text:sub(1, limit)
end

local function mb(value)
    if type(value) ~= "number" then return "N/A" end
    return string.format("%.1f", value / MB)
end

local function pair_text(info)
    if type(info) ~= "table" or type(info.used) ~= "number" or
       type(info.max) ~= "number" then
        return "N/A"
    end
    return mb(info.used) .. "/" .. mb(info.max)
end

local nick = clean(os.nick(), 15)
local language = clean(os.language(), 11)
local mac = clean(os.mac(), 17)
local cfw = clean(os.cfw(), 7)
local version = clean(os.versiontxt(), 12)
local cpu = os.cpu()
local bus = os.bus()
local total = os.totalram()
local frame = 0
local ram
local ms0
local ef0

while true do
    frame = frame + 1
    if frame == 1 or frame % 30 == 0 then
        ram = os.ram()
        ms0 = os.infoms0()
        ef0 = os.infoef0()
    end
    local date = os.getdate()

    screen.clear(dark)

    draw_text("OS INFO", 20, 12, 2, white)
    draw.line(20, 34, 136, 34, cyan)

    draw.fillrect(16, 40, 448, 176, panel)
    draw.rect(16, 40, 448, 176, grey)

    -- Left: identity and date.
    draw_text("NICK", 28, 54, 1, grey)
    draw_text(nick, 104, 54, 1, white)
    draw_text("LANG", 28, 76, 1, grey)
    draw_text(language, 104, 76, 1, white)
    draw_text("MAC", 28, 98, 1, grey)
    draw_text(mac, 104, 98, 1, white)
    draw_text("CFW", 28, 120, 1, grey)
    draw_text(cfw == "UNK" and "UNKNOWN" or cfw, 104, 120, 1,
              cfw == "HOST" and green or yellow)
    draw_text("VER", 28, 142, 1, grey)
    draw_text(version, 104, 142, 1, white)
    draw_text("DATE", 28, 164, 1, grey)
    draw_text(date, 104, 164, 1, white)

    -- Right: clocks, memory and storage.
    draw_text("CPU", 272, 54, 1, grey)
    draw_text(tostring(cpu), 340, 54, 1, white)
    draw_text("BUS", 272, 76, 1, grey)
    draw_text(tostring(bus), 340, 76, 1, white)
    draw_text("RAM", 272, 98, 1, grey)
    draw_text(mb(ram) .. " MB", 340, 98, 1, green)
    draw_text("TOTAL", 272, 120, 1, grey)
    draw_text(mb(total) .. " MB", 340, 120, 1, white)
    draw_text("MS0", 272, 142, 1, grey)
    draw_text(pair_text(ms0), 340, 142, 1,
              type(ms0) == "table" and white or yellow)
    draw_text("EF0", 272, 164, 1, grey)
    draw_text(pair_text(ef0), 340, 164, 1,
              type(ef0) == "table" and white or yellow)

    -- Explicitly show which entry points the demo does not touch.
    draw_text("DIALOG/PRX N/A", 20, 228, 1, grey)
    draw_text("EXIT/RST NOT RUN", 20, 246, 1, dim)

    screen.flip()
end
