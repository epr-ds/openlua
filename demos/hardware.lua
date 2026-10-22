-- OpenLua - OneLua hardware information demo.
--
-- This demo only calls read-only hardware entry points:
--   hw.getmodel(), hw.region(), hw.firmware(), hw.board(), hw.gen()
--   hw.baryon(), hw.tachyon(), hw.pommel(), hw.fuseid(), hw.fusecfg()
--   hw.kirk(), hw.spock(), hw.nandseed(), hw.ofwinitial(), hw.nand()
--   hw.pandora(), hw.volume(), hw.mute(), hw.headphone(), hw.audioout()
--
-- hw.led(), hw.volume(level), hw.mute(mode) and hw.audioout(mode) are
-- deliberately NOT called.  LED control is kernel-only in PSPSDK, and the
-- setters would change the console's saved sound/route state.  The bottom
-- line says so explicitly.
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

local function clean(text, limit)
    text = string.upper(tostring(text or ""))
    text = text:gsub("[\r\n\t]+", " ")
    if text == "" then text = "N/A" end
    return text:sub(1, limit or 16)
end

local function num(value)
    if type(value) ~= "number" or value < 0 then return "N/A" end
    return tostring(value)
end

local function onoff(value)
    if value == 1 then return "ON" end
    if value == 0 then return "OFF" end
    return "N/A"
end

local function yesno(value)
    if value == 1 then return "YES" end
    if value == 0 then return "NO" end
    return "N/A"
end

local function route(value)
    if value == 1 then return "SPEAKERS" end
    if value == 0 then return "HEADPHONES" end
    return "N/A"
end

local model    = clean(hw.getmodel(), 12)
local region   = clean(hw.region(), 16)
local firmware = clean(hw.firmware(), 8)
local board    = clean(hw.board(), 8)
local gen      = clean(hw.gen(), 8)
local nand     = clean(hw.nand(), 8)
local volume   = num(hw.volume())
local mute     = onoff(hw.mute())
local phones   = yesno(hw.headphone())
local audioout = route(hw.audioout())
local pandora  = yesno(hw.pandora() and 1 or 0)

while true do
    screen.clear(dark)

    draw_text("HARDWARE", 20, 12, 2, white)
    draw.line(20, 34, 160, 34, cyan)

    draw.fillrect(16, 40, 448, 176, panel)
    draw.rect(16, 40, 448, 176, grey)

    -- Identity: user-mode reads where possible, documented fallbacks elsewhere.
    draw_text("MODEL", 28, 54, 1, grey)
    draw_text(model, 128, 54, 1, model == "EMULATOR" and cyan or white)
    draw_text("REGION", 28, 76, 1, grey)
    draw_text(region, 128, 76, 1, region == "--" and yellow or white)
    draw_text("FIRMWARE", 28, 98, 1, grey)
    draw_text(firmware, 128, 98, 1, white)
    draw_text("BOARD", 28, 120, 1, grey)
    draw_text(board, 128, 120, 1, board == "UNK" and yellow or white)
    draw_text("GEN", 28, 142, 1, grey)
    draw_text(gen, 128, 142, 1, gen == "UNK" and yellow or white)
    draw_text("NAND", 28, 164, 1, grey)
    draw_text(nand, 128, 164, 1, nand == "UNK" and yellow or white)

    -- Sound and power-related state.  All of these are read-only here.
    draw_text("VOLUME", 272, 54, 1, grey)
    draw_text(volume, 372, 54, 1, volume == "N/A" and yellow or green)
    draw_text("MUTE", 272, 76, 1, grey)
    draw_text(mute, 372, 76, 1, mute == "ON" and yellow or white)
    draw_text("HEADPHONE", 272, 98, 1, grey)
    draw_text(phones, 372, 98, 1, phones == "YES" and green or white)
    draw_text("AUDIOOUT", 272, 120, 1, grey)
    draw_text(audioout, 372, 120, 1, audioout == "N/A" and yellow or white)
    draw_text("PANDORA", 272, 142, 1, grey)
    draw_text(pandora, 372, 142, 1, white)
    draw_text("LED", 272, 164, 1, grey)
    draw_text("N/A", 372, 164, 1, dim)

    -- Make the deliberately unexercised entry points explicit.
    draw_text("ID/FUSE KERNEL ONLY", 20, 228, 1, grey)
    draw_text("LED/AUDIO SETTERS NOT RUN", 20, 246, 1, dim)

    screen.flip()
end
