-- OpenLua - OneLua battery telemetry demo.
--
-- Uses the read-only batt.* API.  The demo intentionally avoids the external
-- font asset and draws its labels with a built-in 5x7 vector font.
--
-- The PSP/PPSSPP may report "-" for values that are unavailable.  The demo
-- renders those as placeholders instead of failing.

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
    ["G"] = { ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###." },
    ["H"] = { "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" },
    ["I"] = { ".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###." },
    ["L"] = { "#....", "#....", "#....", "#....", "#....", "#....", "#####" },
    ["M"] = { "#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#" },
    ["N"] = { "#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#" },
    ["O"] = { ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." },
    ["P"] = { "####.", "#...#", "#...#", "####.", "#....", "#....", "#...." },
    ["R"] = { "####.", "#...#", "#...#", "####.", "##...", "#.#..", "#..#." },
    ["S"] = { ".####", "#....", "#....", ".###.", "....#", "....#", "####." },
    ["T"] = { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." },
    ["V"] = { "#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.." },
    ["W"] = { "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#" },
    ["X"] = { "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#" },
    ["Y"] = { "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.." },
    ["."] = { ".....", ".....", ".....", ".....", ".....", ".##..", ".##.." },
    [":"] = { ".....", ".##..", ".##..", ".....", ".##..", ".##..", "....." },
    ["%"] = { "##...", "##..#", "...#.", "..#..", ".#...", "#..##", "...##" },
    ["/"] = { "....#", "...#.", "...#.", "..#..", ".#...", ".#...", "#...." },
    ["+"] = { ".....", "..#..", "..#..", "#####", "..#..", "..#..", "....." },
}

local white  = color.new(235, 240, 255)
local grey   = color.new(95, 105, 125)
local dark   = color.new(18, 24, 38)
local green  = color.new(40, 220, 120)
local yellow = color.new(250, 205, 60)
local red    = color.new(245, 75, 75)
local blue   = color.new(55, 95, 165)

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

local function text_width(text, scale)
    return #text * 6 * scale
end

local function clamp_percent(value)
    if value < 0 then return 0 end
    if value > 100 then return 100 end
    return value
end

local frame = 0
while true do
    frame = frame + 1
    screen.clear(dark)

    local exists   = batt.exists()
    local charging = batt.charging()
    local low      = batt.low()
    local percent  = clamp_percent(batt.lifepercent())
    local lifetime = batt.lifetime()
    local temp     = batt.temp()
    local volt     = batt.volt()
    local remain   = batt.remaincap()
    local full     = batt.fullcap()
    local mode     = batt.mode()
    local serial   = batt.serial()

    draw_text("BATTERY", 20, 12, 2, white)

    -- Battery pack: body 20..440, terminal 440..448.
    draw.rect(20, 44, 420, 56, grey)
    draw.fillrect(24, 48, 412, 48, color.new(30, 42, 66))
    local level_color = green
    if not exists then
        level_color = grey
    elseif percent <= 20 then
        level_color = red
    elseif percent <= 50 then
        level_color = yellow
    elseif charging then
        level_color = blue
    end

    local fillw = math.floor(412 * percent / 100)
    if fillw > 0 then
        draw.fillrect(24, 48, fillw, 48, level_color)
    end
    draw.fillrect(440, 60, 8, 24, grey)

    local percent_text = exists and (tostring(percent) .. "%") or "--"
    draw_text(percent_text, 20 + (420 - text_width(percent_text, 3)) / 2,
              58, 3, white)

    local status, status_color
    if not exists then
        status, status_color = "NO BATTERY", grey
    elseif charging then
        status, status_color = "CHARGING", green
    elseif low then
        status, status_color = "LOW", red
    else
        status, status_color = "ON BATTERY", white
    end
    draw_text(status, 20, 112, 1, status_color)
    if charging and math.floor(frame / 20) % 2 == 0 then
        draw_text("+", 20 + text_width(status, 1) + 8, 112, 1, green)
    end

    -- Capacity bar: remaining / full.
    local remain_num = tonumber(remain) or 0
    local full_num = tonumber(full) or 0
    local cap_fraction = 0
    if full_num > 0 then
        cap_fraction = remain_num / full_num
        if cap_fraction < 0 then cap_fraction = 0 end
        if cap_fraction > 1 then cap_fraction = 1 end
    end
    draw_text("CAP", 20, 134, 1, white)
    draw.rect(64, 134, 340, 8, grey)
    if cap_fraction > 0 then
        draw.fillrect(65, 135, math.floor(338 * cap_fraction), 6, blue)
    end

    local temp_text = temp < 0 and "-" or (tostring(temp) .. " C")
    local volt_text = volt == "-" and "-" or (volt .. " V")
    local cap_text
    if remain == "-" or full == "-" then
        cap_text = "-"
    else
        cap_text = remain .. "/" .. full .. " MAH"
    end

    draw_text("TIME " .. lifetime, 20, 154, 1, white)
    draw_text("TEMP " .. temp_text, 20, 174, 1, white)
    draw_text("VOLT " .. volt_text, 20, 194, 1, white)
    draw_text("CAP  " .. cap_text, 20, 214, 1, white)
    draw_text("MODE " .. mode .. "  SERIAL " .. serial, 20, 234, 1, grey)

    screen.flip()
end
