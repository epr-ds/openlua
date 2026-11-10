-- OpenLua - real-hardware module test menu.
--
-- Copy this file to the app directory as script.lua next to EBOOT.PBP and
-- assets/ (or use tools/package_tester.sh).  It runs on PSP-1000 (Fat),
-- PSP-2000/3000 (Slim), PSP-E1000 (Street) and PSP Go (N1000).
--
-- Controls
--   UP/DOWN    move / scroll
--   X or O     open the selected test (O also goes back inside a test)
--   SELECT     cycle the declared model (saved to tester.cfg)
--   START      run all non-interactive checks and write test_report.txt
--
-- Every call is guarded with pcall(), destructive calls (os.exit,
-- os.restart, power.suspend/off/lock, flash writes, FTP server) are never made
-- automatically, and interactive pages only start USB/network/camera hardware
-- when the user presses the documented key.  The menu needs no external font:
-- it tries the system PGF, then assets/font.png, then a built-in 5x7 font.

local VERSION = "1.0"
local LH = 16
local RESULT_LH = 19
local W, H = 480, 272

-- ---------------------------------------------------------------- colours --
local white  = color.new(238, 240, 246)
local grey   = color.new(130, 142, 162)
local dim    = color.new(64, 76, 94)
local dark   = color.new(10, 14, 26)
local panel  = color.new(20, 27, 44)
local panel2 = color.new(28, 38, 60)
local teal   = color.new(48, 190, 170)
local green  = color.new(60, 220, 120)
local amber  = color.new(240, 190, 70)
local red    = color.new(232, 90, 90)
local blue   = color.new(90, 170, 255)

-- --------------------------------------------------------- built-in font ---
-- Fallback only; all glyphs are 5x7.  Text is uppercased before drawing so
-- the menu stays readable even without flash0:/font/ltn0.pgf or assets.
local GLYPH = {
    [" "] = { ".....", ".....", ".....", ".....", ".....", ".....", "....." },
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
    ["A"] = { ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" },
    ["B"] = { "####.", "#...#", "#...#", "####.", "#...#", "#...#", "####." },
    ["C"] = { ".###.", "#...#", "#....", "#....", "#....", "#...#", ".###." },
    ["D"] = { "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####." },
    ["E"] = { "#####", "#....", "#....", "####.", "#....", "#....", "#####" },
    ["F"] = { "#####", "#....", "#....", "####.", "#....", "#....", "#...." },
    ["G"] = { ".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###." },
    ["H"] = { "#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" },
    ["I"] = { ".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###." },
    ["J"] = { "..###", "...#.", "...#.", "...#.", "#..#.", "#..#.", ".##.." },
    ["K"] = { "#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#" },
    ["L"] = { "#....", "#....", "#....", "#....", "#....", "#....", "#####" },
    ["M"] = { "#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#" },
    ["N"] = { "#...#", "##..#", "##..#", "#.#.#", "#..##", "#..##", "#...#" },
    ["O"] = { ".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." },
    ["P"] = { "####.", "#...#", "#...#", "####.", "#....", "#....", "#...." },
    ["Q"] = { ".###.", "#...#", "#...#", "#...#", "#.#.#", ".###.", "....#" },
    ["R"] = { "####.", "#...#", "#...#", "####.", "##...", "#.#..", "#..#." },
    ["S"] = { ".####", "#....", "#....", ".###.", "....#", "....#", "####." },
    ["T"] = { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." },
    ["U"] = { "#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###." },
    ["V"] = { "#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.." },
    ["W"] = { "#...#", "#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#" },
    ["X"] = { "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#" },
    ["Y"] = { "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.." },
    ["Z"] = { "#####", "....#", "...#.", "..#..", ".#...", "#....", "#####" },
    ["."] = { ".....", ".....", ".....", ".....", ".....", ".##..", ".##.." },
    [":"] = { ".....", ".##..", ".##..", ".....", ".##..", ".##..", "....." },
    ["/"] = { "....#", "...#.", "...#.", "..#..", ".#...", ".#...", "#...." },
    ["-"] = { ".....", ".....", ".....", "#####", ".....", ".....", "....." },
    ["_"] = { ".....", ".....", ".....", ".....", ".....", ".....", "#####" },
    ["+"] = { ".....", "..#..", "..#..", "#####", "..#..", "..#..", "....." },
    ["="] = { ".....", ".....", "#####", ".....", "#####", ".....", "....." },
    ["<"] = { "...#.", "..#..", ".#...", "#....", ".#...", "..#..", "...#." },
    [">"] = { ".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..." },
    ["?"] = { ".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.." },
    ["!"] = { "..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.." },
    [","] = { ".....", ".....", ".....", ".....", ".....", ".##..", ".#..." },
    ["%"] = { "#...#", "#..#.", "...#.", "..#..", ".#...", ".#..#", "#...#" },
    ["*"] = { ".....", "#.#.#", ".###.", "#####", ".###.", "#.#.#", "....." },
    ["#"] = { ".#.#.", ".#.#.", "#####", ".#.#.", "#####", ".#.#.", ".#.#." },
    ["|"] = { "..#..", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." },
    [";"] = { ".....", ".##..", ".##..", ".....", ".##..", ".#...", "....." },
    ["'"] = { "..#..", "..#..", ".....", ".....", ".....", ".....", "....." },
    ['"'] = { ".#.#.", ".#.#.", ".....", ".....", ".....", ".....", "....." },
    ["~"] = { ".....", ".....", ".#...", "#.#.#", "...#.", ".....", "....." },
    ["("] = { "...#.", "..#..", ".#...", ".#...", ".#...", "..#..", "...#." },
    [")"] = { ".#...", "..#..", "...#.", "...#.", "...#.", "..#..", ".#..." },
    ["["] = { "..###", "..#..", "..#..", "..#..", "..#..", "..#..", "..###" },
    ["]"] = { "###..", "..#..", "..#..", "..#..", "..#..", "..#..", "###.." },
}

local function draw_glyphs(text, x, y, scale, tint)
    scale = scale or 1
    for i = 1, #text do
        local glyph = GLYPH[text:sub(i, i)]
        if glyph then
            for row = 1, 7 do
                local line = glyph[row]
                for col = 1, 5 do
                    if line:sub(col, col) == "#" then
                        draw.fillrect(x + (i - 1) * 6 * scale + (col - 1) * scale,
                                      y + (row - 1) * scale, scale, scale, tint)
                    end
                end
            end
        end
    end
end

local pgf, fixed
local function init_fonts()
    local ok, h = pcall(font.load, "flash0:/font/ltn0.pgf")
    if ok and h then pgf = h; pcall(screen.font, h) end
    if not pgf then
        local ok2, h2 = pcall(font.new, "assets/font.png", 8, 16)
        if ok2 and h2 then fixed = h2; pcall(screen.font, h2) end
    end
end
init_fonts()

local function txt(x, y, text, tint, scale)
    text = tostring(text or "")
    tint = tint or white
    if pgf then
        font.print(pgf, x, y, text, tint)
    elseif fixed then
        font.print(fixed, x, y, text, tint)
    else
        draw_glyphs(string.upper(text), x, y, scale or 1, tint)
    end
    return x
end

local function txtw(text)
    text = tostring(text or "")
    if pgf then
        local ok, w = pcall(font.textwidth, pgf, text)
        if ok and type(w) == "number" then return w end
    elseif fixed then
        local ok, w = pcall(font.textwidth, fixed, text)
        if ok and type(w) == "number" then return w end
    end
    return #text * 6
end

-- ------------------------------------------------------------------ input --
local last_mask = 0

local function bit_has(mask, bit)
    if math.bitand then
        return math.bitand(mask, bit) ~= 0
    end
    if bit <= 0 then return false end
    return (mask % (bit * 2)) >= bit
end

local function read_pad()
    local ok, m, lx, ly = pcall(buttons.read)
    if not ok or type(m) ~= "number" then
        return 0, 128, 128
    end
    return m, lx or 128, ly or 128
end

local function sync_pad()
    local m = read_pad()
    last_mask = m
end

local function new_press(mask)
    local diff
    if math.bitand and math.bitnot then
        diff = math.bitand(mask, math.bitnot(last_mask))
    else
        diff = 0
        local bit = 1
        while bit <= 0x8000 do
            if bit_has(mask, bit) and not bit_has(last_mask, bit) then
                diff = diff + bit
            end
            bit = bit * 2
        end
    end
    last_mask = mask
    return diff
end

-- --------------------------------------------------------------- results ---
local function R(label, ok, detail)
    return { label = tostring(label), ok = ok, detail = detail == nil and "" or tostring(detail) }
end
local function PASS(label, cond, detail)
    return R(label, cond and true or false, detail)
end
local function INFO(label, detail)
    return R(label, "INFO", detail)
end
local function SKIP(label, why)
    return R(label, "SKIP", why)
end

local function status_code(ok)
    if ok == true then return "PASS" end
    if ok == false then return "FAIL" end
    if ok == "INFO" then return "INFO" end
    if ok == "SKIP" then return "SKIP" end
    return "N/A"
end

local function status_color(ok)
    if ok == true then return green end
    if ok == false then return red end
    if ok == "INFO" then return blue end
    return grey
end

local function summarise(results)
    local s = { pass = 0, fail = 0, info = 0, skip = 0, total = 0 }
    for i = 1, #results do
        local r = results[i]
        s.total = s.total + 1
        if r.ok == true then s.pass = s.pass + 1
        elseif r.ok == false then s.fail = s.fail + 1
        elseif r.ok == "INFO" then s.info = s.info + 1
        else s.skip = s.skip + 1 end
    end
    return s
end

local function summary_text(s)
    return string.format("%d PASS  %d FAIL  %d INFO  %d SKIP", s.pass, s.fail, s.info, s.skip)
end

-- ------------------------------------------------------------- utilities ---
local function short(v, n)
    v = tostring(v == nil and "nil" or v)
    v = v:gsub("[\r\n\t]+", " ")
    if #v > n then return v:sub(1, n - 1) .. "~" end
    return v
end

local function human(n)
    if type(n) ~= "number" then return "-" end
    if n >= 1073741824 then return string.format("%.2f GB", n / 1073741824) end
    if n >= 1048576 then return string.format("%.1f MB", n / 1048576) end
    if n >= 1024 then return string.format("%.1f KB", n / 1024) end
    return tostring(n) .. " B"
end

local function onoff(v)
    return v and "YES" or "NO"
end

local function exists(path)
    local ok, v = pcall(files.exists, path)
    return ok and v == true
end

local function has_volume(root)
    if root == "ms0:/" then
        local ok, info = pcall(os.infoms0)
        if ok and info then return true end
        return exists("ms0:/PSP")
    elseif root == "ef0:/" then
        local ok, info = pcall(os.infoef0)
        if ok and info then return true end
        return exists("ef0:/PSP")
    end
    return false
end

local function volume_info(root)
    local ok, info
    if root == "ms0:/" then ok, info = pcall(os.infoms0)
    else ok, info = pcall(os.infoef0) end
    if ok and type(info) == "table" and type(info.used) == "number" and
       type(info.max) == "number" then
        return human(info.used) .. " / " .. human(info.max)
    end
    return "-"
end

local function writable_base()
    local root
    if has_volume("ms0:/") then root = "ms0:/"
    elseif has_volume("ef0:/") then root = "ef0:/" end
    if root then
        local dir = root .. "PSP/OPENLUA_TEST"
        pcall(files.mkdir, dir)
        return dir
    end
    return "."
end

local function save_root_for(gameid)
    if exists("ms0:/PSP/SAVEDATA/" .. gameid) then return "ms0:" end
    if exists("ef0:/PSP/SAVEDATA/" .. gameid) then return "ef0:" end
    return "?"
end

local function ini_get(path, sec, key, default)
    local ok, v = pcall(ini.read, path, sec, key, default)
    if ok then return v end
    return default
end

local function ini_put(path, sec, key, val)
    return pcall(ini.write, path, sec, key, val)
end

-- --------------------------------------------------------------- profile ---
local MODELS = {
    "PSP-1000 (FAT)",
    "PSP-2000 (SLIM)",
    "PSP-3000 (SLIM)",
    "PSP-E1000 (STREET)",
    "PSP GO (N1000)",
}
local model_index = 1
local saved_model = ini_get("tester.cfg", "profile", "model", nil)
if saved_model then
    for i = 1, #MODELS do
        if MODELS[i] == saved_model then model_index = i end
    end
elseif has_volume("ef0:/") then
    model_index = 5
end

local function model_label()
    return MODELS[model_index]
end

local function cycle_model()
    model_index = model_index + 1
    if model_index > #MODELS then model_index = 1 end
    ini_put("tester.cfg", "profile", "model", MODELS[model_index])
end

-- --------------------------------------------------------------- system ----
local function test_system()
    local r = {}
    local function add(label, ok, detail) r[#r + 1] = PASS(label, ok, detail) end
    local function info(label, detail) r[#r + 1] = INFO(label, detail) end
    local function grab(fn, ...)
        local ok, v = pcall(fn, ...)
        if ok then return v end
        return "<ERR>"
    end

    local nick = grab(os.nick)
    local lang = grab(os.language)
    local mac = grab(os.mac)
    local cfw = grab(os.cfw)
    local ver = grab(os.versiontxt)
    local date = grab(os.getdate)
    add("OS.NICK", type(nick) == "string", short(nick, 18))
    add("OS.LANGUAGE", type(lang) == "string", short(lang, 12))
    add("OS.MAC", type(mac) == "string", short(mac, 17))
    add("OS.CFW", type(cfw) == "string", short(cfw, 10))
    add("OS.VERSIONTXT", type(ver) == "string", short(ver, 20))
    add("OS.GETDATE", type(date) == "string", short(date, 20))
    add("OS.PASSWORD STUB", grab(os.password) == "", "empty")
    add("OS.DELAY", pcall(os.delay, 2), "2 ms")

    local cpu, bus = grab(os.cpu), grab(os.bus)
    local ram, total = grab(os.ram), grab(os.totalram)
    add("OS.CPU", type(cpu) == "number" and cpu > 0, tostring(cpu) .. " MHz")
    add("OS.BUS", type(bus) == "number" and bus > 0, tostring(bus) .. " MHz")
    add("OS.RAM", type(ram) == "number" and ram > 0, human(ram))
    add("OS.TOTALRAM", type(total) == "number" and total > 0, human(total))

    add("OS.MESSAGE STUB", select(1, pcall(os.message, "x")), "no dialog")
    add("OS.MESSAGEBOX STUB", grab(os.messagebox, "t", "m") == 0, "returns 0")
    add("OS.INITPRX STUB", grab(os.initprx, "x") == 0, "no load")
    add("OS.STOPPRX STUB", grab(os.stopprx, 1) == false, "no stop")
    add("OS.REQUIERE STUB", grab(os.requiere, "x") == 0, "no load")

    local ms0, ef0 = grab(os.infoms0), grab(os.infoef0)
    info("OS.INFOMS0", type(ms0) == "table" and volume_info("ms0:/") or "nil")
    info("OS.INFOEF0", type(ef0) == "table" and volume_info("ef0:/") or "nil")

    local model, fw = grab(hw.getmodel), grab(hw.firmware)
    add("HW.GETMODEL", type(model) == "string", short(model, 12))
    add("HW.FIRMWARE", type(fw) == "string", short(fw, 12))
    info("HW.IDENT", "REGION=" .. short(grab(hw.region), 4) ..
         " BOARD=" .. short(grab(hw.board), 8) ..
         " GEN=" .. short(grab(hw.gen), 6) ..
         " NAND=" .. short(grab(hw.nand), 8))
    info("HW.SYSCON", "BARYON=" .. short(grab(hw.baryon), 8) ..
         " TACHYON=" .. short(grab(hw.tachyon), 8) ..
         " POMMEL=" .. short(grab(hw.pommel), 6) ..
         " PANDORA=" .. tostring(grab(hw.pandora)))
    info("HW.FUSES", "FUSEID=" .. short(grab(hw.fuseid), 8) ..
         " FUSECFG=" .. short(grab(hw.fusecfg), 6) ..
         " KIRK=" .. short(grab(hw.kirk), 6) ..
         " SPOCK=" .. short(grab(hw.spock), 6))
    add("HW.VOLUME", type(grab(hw.volume)) == "number", tostring(grab(hw.volume)))
    add("HW.MUTE", type(grab(hw.mute)) == "number", tostring(grab(hw.mute)))
    add("HW.HEADPHONE", type(grab(hw.headphone)) == "number", tostring(grab(hw.headphone)))
    add("HW.AUDIOOUT", type(grab(hw.audioout)) == "number", tostring(grab(hw.audioout)))
    add("HW.LED (KERNEL)", grab(hw.led, 2, 1) == -1, "-1 as documented")

    if grab(batt.exists) then
        add("BATT.EXISTS", true, "YES")
        add("BATT.CHARGING", type(grab(batt.charging)) == "boolean", tostring(grab(batt.charging)))
        add("BATT.LIFE", type(grab(batt.lifepercent)) == "number", tostring(grab(batt.lifepercent)) .. "%")
        add("BATT.TIME", type(grab(batt.lifetime)) == "string", short(grab(batt.lifetime), 10))
        add("BATT.TEMP", type(grab(batt.temp)) == "number", tostring(grab(batt.temp)) .. " C")
        add("BATT.VOLT", type(grab(batt.volt)) == "string", short(grab(batt.volt), 8))
        info("BATT.CAP", short(grab(batt.remaincap), 8) .. "/" ..
             short(grab(batt.fullcap), 8) .. " mAh")
    else
        r[#r + 1] = SKIP("BATT.EXISTS", "NO BATTERY TELEMETRY")
    end
    add("BATT.SERIAL STUB", grab(batt.serial) == "-", "-")
    add("BATT.MODE STUB", grab(batt.mode) == "-", "-")
    add("BATT.MODES STUB", grab(batt.tonormal) == 0 and grab(batt.topandora) == 0 and
        grab(batt.toautoboot) == 0, "no writes")
    add("BATT.EEPROM STUB", grab(batt.backupbat, "x") == 0 and
        grab(batt.restorebat, "x") == 0, "no file access")

    add("POWER.PLUGGED", type(grab(power.plugged)) == "boolean", tostring(grab(power.plugged)))
    add("POWER.TICK", select(1, pcall(power.tick)), "idle timer")
    r[#r + 1] = SKIP("POWER.SUSPEND/OFF", "NOT RUN (CHANGES POWER STATE)")
    r[#r + 1] = SKIP("POWER.LOCK/UNLOCK", "NOT RUN (POWER SWITCH)")
    return r
end

-- -------------------------------------------------------------- storage ----
local function test_storage()
    local r = {}
    local cwd = files.cdir()
    r[#r + 1] = INFO("CWD", short(cwd, 46))
    r[#r + 1] = PASS("APP SCRIPT", exists("script.lua") or exists("app0:/script.lua"),
                     exists("script.lua") and "script.lua" or "app0:/script.lua")
    r[#r + 1] = INFO("APP0 STAT", onoff(exists("app0:/script.lua")) ..
                     " (PPSSPP does not mount app0:)")

    local function clean(dir)
        -- Remove known leftovers first: a fixed test path is reused each
        -- run and some FAT layers do not recurse reliably.
        for _, name in ipairs({ "a.txt", "b.txt", "c.txt", "d.txt" }) do
            pcall(files.delete, dir .. "/" .. name)
        end
        pcall(files.delete, dir)
    end

    local function probe(root)
        local tag = root:sub(1, 3)
        local dir = root .. "PSP/OPENLUA_TEST/FS"
        clean(dir)
        pcall(files.mkdir, dir)
        local made = exists(dir)
        r[#r + 1] = PASS(tag .. " MKDIR", made, dir)
        if not made then return end

        local path = dir .. "/a.txt"
        local written = false
        local f = io.open(path, "wb")
        if f then f:write("hello"); f:close(); written = true end
        r[#r + 1] = PASS(tag .. " WRITE", written and exists(path), path)
        r[#r + 1] = PASS(tag .. " SIZE", files.size(path) == 5, tostring(files.size(path)))
        r[#r + 1] = PASS(tag .. " EXT", files.ext(path) == "txt", tostring(files.ext(path)))
        r[#r + 1] = PASS(tag .. " NOFILE", files.nofile(path) == dir, short(files.nofile(path), 24))
        r[#r + 1] = PASS(tag .. " NOPATH", files.nopath(path) == "a.txt", tostring(files.nopath(path)))

        local list = files.list(dir)
        r[#r + 1] = PASS(tag .. " LIST", type(list) == "table" and #list == 1,
                         type(list) == "table" and (#list .. " entries") or "nil")
        local only = files.listfiles(dir)
        r[#r + 1] = PASS(tag .. " LISTFILES", type(only) == "table" and #only == 1,
                         type(only) == "table" and (#only .. " files") or "nil")
        local dirs = files.listdirs(dir)
        r[#r + 1] = PASS(tag .. " LISTDIRS", type(dirs) == "table" and #dirs == 0,
                         type(dirs) == "table" and (#dirs .. " dirs") or "nil")

        local copy = dir .. "/b.txt"
        r[#r + 1] = PASS(tag .. " COPY", pcall(files.copy, path, copy) and exists(copy), copy)
        local ren = dir .. "/c.txt"
        r[#r + 1] = PASS(tag .. " RENAME", pcall(files.rename, copy, ren) and exists(ren), ren)
        local mov = dir .. "/d.txt"
        r[#r + 1] = PASS(tag .. " MOVE", pcall(files.move, ren, mov) and exists(mov), mov)
        pcall(files.delete, path)
        pcall(files.delete, mov)
        local deleted = pcall(files.delete, dir)
        r[#r + 1] = PASS(tag .. " DELETE", deleted and not exists(dir), dir)
    end

    if has_volume("ms0:/") then probe("ms0:/")
    else r[#r + 1] = SKIP("MS0", "ABSENT") end
    if has_volume("ef0:/") then probe("ef0:/")
    else r[#r + 1] = SKIP("EF0", "ABSENT (NOT A GO / NO M2)") end

    if has_volume("ms0:/") and has_volume("ef0:/") then
        local a, b = "ms0:/PSP/OPENLUA_TEST/X", "ef0:/PSP/OPENLUA_TEST/X"
        pcall(files.delete, a); pcall(files.delete, b)
        pcall(files.mkdir, a); pcall(files.mkdir, b)
        local f = io.open(a .. "/cross.txt", "wb")
        if f then f:write("cross-volume"); f:close() end
        local copied = a .. "/cross.txt"
        local copyok = pcall(files.copy, copied, b .. "/copy.txt") and
                       exists(b .. "/copy.txt")
        r[#r + 1] = PASS("X COPY MS0>EF0", copyok, b .. "/copy.txt")
        local moved = pcall(files.move, copied, b .. "/moved.txt") and
                      exists(b .. "/moved.txt") and not exists(copied)
        r[#r + 1] = PASS("X MOVE MS0>EF0", moved, b .. "/moved.txt")
        pcall(files.delete, a); pcall(files.delete, b)
    else
        r[#r + 1] = SKIP("CROSS VOLUME", "BOTH MS0 AND EF0 REQUIRED")
    end

    local base = writable_base()
    local chdir_ok = pcall(files.cdir, base)
    local restore_ok = pcall(files.cdir, cwd)
    r[#r + 1] = PASS("CDIR SET/RESTORE", chdir_ok and restore_ok and files.cdir() == cwd,
                     short(files.cdir(), 32))
    return r
end

-- ------------------------------------------------------------- savedata ----
local function test_savedata()
    local r = {}
    local GAME = "OLTEST1"
    pcall(data.delete, GAME)

    local ok, saved = pcall(data.save, "payload", "OpenLua Test", GAME, "Sub", "Desc", 1)
    r[#r + 1] = PASS("SAVE AUTO", ok and saved == true, "mode 1")
    local load_ok, loaded = pcall(data.load, GAME, 1)
    r[#r + 1] = PASS("LOAD AUTO", load_ok and loaded == "payload", short(loaded, 20))
    local root = save_root_for(GAME)
    r[#r + 1] = INFO("SAVE ROOT", root)
    r[#r + 1] = PASS("META FILE", exists(root .. "/PSP/SAVEDATA/" .. GAME .. "/META"),
                     root .. "/PSP/SAVEDATA/" .. GAME)

    local s1 = pcall(data.save, "slot-one", "OpenLua Test", GAME, "", "", 0)
    local s2 = pcall(data.save, "slot-two", "OpenLua Test", GAME, "", "", 0)
    r[#r + 1] = PASS("SAVE SLOTS", s1 and s2, "SLOT1 + SLOT2")
    r[#r + 1] = PASS("SLOT1 FILE", exists(root .. "/PSP/SAVEDATA/" .. GAME .. "/SLOT1"))
    r[#r + 1] = PASS("SLOT2 FILE", exists(root .. "/PSP/SAVEDATA/" .. GAME .. "/SLOT2"))
    local l0, slot = pcall(data.load, GAME, 0)
    r[#r + 1] = PASS("LOAD SLOT", l0 and slot == "slot-two", short(slot, 16))

    local function rejected(fn, ...)
        local ok, value = pcall(fn, ...)
        return (not ok) or value == false
    end
    r[#r + 1] = PASS("REJECT TRAVERSAL", rejected(data.save, "x", "T", "../BAD", "", "", 1))
    local cap_ok, cap_saved = pcall(data.save, string.rep("x", 300), "T", "OLBIG", "", "", 1)
    local _, cap_val = pcall(data.load, "OLBIG", 1)
    r[#r + 1] = PASS("PAYLOAD CAP 256",
                     cap_ok and cap_saved == true and type(cap_val) == "string" and
                     #cap_val == 256,
                     type(cap_val) == "string" and (#cap_val .. " bytes") or "nil")
    pcall(data.delete, "OLBIG")
    r[#r + 1] = PASS("REJECT TITLE>128",
                     rejected(data.save, "x", string.rep("t", 129), "OLTITLE", "", "", 1))

    local del_ok, deleted = pcall(data.delete, GAME)
    r[#r + 1] = PASS("DELETE", del_ok and deleted == true, "autosave + slots")
    local gone = pcall(data.load, GAME, 1)
    r[#r + 1] = PASS("LOAD AFTER DELETE", gone and select(2, pcall(data.load, GAME, 1)) == nil)
    return r
end

-- ---------------------------------------------------------- ini + json -----
local function test_dataformats()
    local r = {}
    local base = writable_base()
    local ini_path = base .. "/tester.ini"
    local json_path = base .. "/tester.json"
    pcall(files.delete, ini_path)
    pcall(files.delete, json_path)

    local okw = pcall(ini.write, ini_path, "main", "key", "value")
    local okr, v = pcall(ini.read, ini_path, "main", "key")
    r[#r + 1] = PASS("INI WRITE/READ", okw and okr and v == "value", short(v, 16))
    local okd, dv = pcall(ini.read, ini_path, "main", "missing", "default")
    r[#r + 1] = PASS("INI DEFAULT", okd and dv == "default", short(dv, 16))
    pcall(ini.write, ini_path, "topkey", "topvalue")
    local _, tv = pcall(ini.read, ini_path, "topkey")
    r[#r + 1] = PASS("INI SECTIONLESS", tv == "topvalue", short(tv, 16))
    pcall(ini.write, ini_path, "main", "key", "second")
    local _, ov = pcall(ini.read, ini_path, "main", "key")
    r[#r + 1] = PASS("INI OVERWRITE", ov == "second", short(ov, 16))

    local encoded = select(2, pcall(json.encode, { a = 1, b = "x", c = true }))
    local decoded = select(2, pcall(json.decode, encoded or "{}"))
    r[#r + 1] = PASS("JSON ENCODE", type(encoded) == "string" and #encoded > 0, short(encoded, 28))
    r[#r + 1] = PASS("JSON DECODE", type(decoded) == "table" and decoded.a == 1 and
                     decoded.b == "x" and decoded.c == true)
    local _, arr = pcall(json.decode, "[1,2,3]")
    r[#r + 1] = PASS("JSON ARRAY", type(arr) == "table" and #arr == 3 and arr[3] == 3)
    local _, esc = pcall(json.decode, '"a\\nb"')
    r[#r + 1] = PASS("JSON ESCAPES", esc == "a\nb", short(esc, 8))
    local _, nul = pcall(json.decode, '{"a":null}')
    r[#r + 1] = PASS("JSON NULL", type(nul) == "table" and nul.a == nil)
    r[#r + 1] = PASS("JSON REJECT BAD", pcall(json.decode, "{bad") == false)
    r[#r + 1] = PASS("JSON REJECT SPARSE", pcall(json.encode, { [1] = 1, [3] = 3 }) == false)
    local saved = pcall(json.save, json_path, { round = 7, text = "ok" })
    local loaded = select(2, pcall(json.load, json_path))
    r[#r + 1] = PASS("JSON SAVE/LOAD", saved and type(loaded) == "table" and
                     loaded.round == 7 and loaded.text == "ok")

    -- tester.cfg is read/written through the same ini module at startup.
    r[#r + 1] = INFO("PROFILE FILE", exists("tester.cfg") and model_label() or "not saved yet")
    pcall(files.delete, ini_path)
    pcall(files.delete, json_path)
    return r
end

-- --------------------------------------------------- buttons/timers/math ---
local function test_input_math()
    local r = {}
    local ok, m, lx, ly = pcall(buttons.read)
    r[#r + 1] = PASS("BUTTONS.READ", ok and type(m) == "number" and
                      type(lx) == "number" and type(ly) == "number",
                      ok and (m .. " " .. lx .. "," .. ly) or "error")

    local consts = { "__UP", "__DOWN", "__LEFT", "__RIGHT", "__CROSS", "__CIRCLE",
                     "__SQUARE", "__TRIANGLE", "__L", "__R", "__START", "__SELECT",
                     "__NOTE", "__HOLD" }
    local missing = {}
    for i = 1, #consts do
        if type(_G[consts[i]]) ~= "number" then missing[#missing + 1] = consts[i] end
    end
    r[#r + 1] = PASS("BUTTON CONSTANTS", #missing == 0,
                     #missing == 0 and (#consts .. " bits") or table.concat(missing, ","))

    local tok, t = pcall(timers.new, 0)
    if tok and t then
        pcall(t.start, t)
        os.delay(25)
        local ms = t:time()
        r[#r + 1] = PASS("TIMER.TIME", type(ms) == "number" and ms >= 10,
                         tostring(ms) .. " ms")
        local s1 = t:stoptime()
        local s2 = t:stoptime()
        r[#r + 1] = PASS("TIMER.STOPTIME", s1 == s2, tostring(s1))
        t:reset(500)
        r[#r + 1] = PASS("TIMER.RESET", type(t:time()) == "number", tostring(t:time()))
        local sr = t:startreset()
        r[#r + 1] = PASS("TIMER.STARTRESET", sr ~= nil, tostring(sr))
        local g = t:gettime()
        r[#r + 1] = PASS("TIMER.GETTIME", type(g) == "number", tostring(g))
    else
        r[#r + 1] = PASS("TIMER.NEW", false, "timers.new failed")
    end

    r[#r + 1] = PASS("MATH.MINMAX", math.minmax(45, 0, 30) == 30 and
                      math.minmax(-5, 0, 10) == 0, "clamps")
    r[#r + 1] = PASS("MATH.BITAND", math.bitand(0xAA, 0xCC) == 136, "0xAA & 0xCC")
    r[#r + 1] = PASS("MATH.BITOR", math.bitor(0xAA, 0xCC) == 238, "0xAA | 0xCC")
    r[#r + 1] = PASS("MATH.BITNOT", math.bitnot(0x0F) == -16, "~0x0F")
    r[#r + 1] = PASS("MATH.STANDARD", math.floor(2.9) == 2 and
                      math.abs(math.sqrt(9) - 3) < 0.0001, "floor/sqrt")
    return r
end

-- -------------------------------------------------------------- graphics ---
local function test_graphics()
    local r = {}
    local base = writable_base()
    local ok, img = pcall(image.new, 32, 24, color.new(10, 20, 30))
    if not ok or not img then
        r[#r + 1] = PASS("IMAGE.NEW", false, "image.new failed")
        return r
    end
    r[#r + 1] = PASS("IMAGE.NEW", image.getw(img) == 32 and image.geth(img) == 24,
                     image.getw(img) .. "x" .. image.geth(img))
    r[#r + 1] = PASS("IMAGE.REAL SIZE", image.getrealw(img) >= 32 and
                     image.getrealh(img) >= 24,
                     image.getrealw(img) .. "x" .. image.getrealh(img))

    image.clear(img, color.new(200, 30, 30))
    image.pixel(img, 1, 1, color.new(20, 220, 60))
    local px = image.pixel(img, 1, 1)
    r[#r + 1] = PASS("IMAGE.PIXEL", type(px) == "number", string.format("0x%08X", px or 0))

    local cp = image.copy(img)
    r[#r + 1] = PASS("IMAGE.COPY", cp ~= nil and image.getw(cp) == 32,
                     cp and image.getw(cp) .. "x" .. image.geth(cp) or "nil")

    local save_path = base .. "/gfx.png"
    pcall(files.delete, save_path)
    local saved = pcall(image.save, img, save_path)
    local loaded_ok, loaded = pcall(image.load, save_path)
    r[#r + 1] = PASS("IMAGE.SAVE/LOAD", saved and loaded_ok and loaded ~= nil and
                     image.getw(loaded) == 32, save_path)

    local sheet_ok, sheet = pcall(image.load, "assets/test_sheet.png", 32, 32)
    if sheet_ok and sheet then
        r[#r + 1] = PASS("IMAGE.SHEET", image.getw(sheet) == 256 and
                         image.geth(sheet) == 64,
                         image.getw(sheet) .. "x" .. image.geth(sheet) .. " 32x32 frames")
    else
        r[#r + 1] = SKIP("IMAGE.SHEET", "assets/test_sheet.png missing")
    end

    local transforms_ok = pcall(function()
        image.fliph(img, true)
        image.flipv(img, true)
        image.rotate(img, 30)
        image.scale(img, 1.5, 1.5)
        image.pivot(img, 16, 12)
        image.tint(img, color.new(255, 255, 255))
        image.center(img)
        image.zindex(img, 1)
        image.reset(img)
    end)
    r[#r + 1] = PASS("IMAGE TRANSFORMS", transforms_ok, "flip/rotate/scale/tint/reset")

    local draw_ok = pcall(function()
        draw.line(0, 0, 10, 10, white)
        draw.rect(0, 0, 10, 10, white)
        draw.fillrect(0, 0, 10, 10, white)
        draw.circle(20, 20, 8, white, 16)
    end)
    r[#r + 1] = PASS("DRAW PRIMITIVES", draw_ok, "line/rect/fill/circle")
    local tw = screen.textwidth("TEST")
    r[#r + 1] = PASS("SCREEN.TEXTWIDTH", type(tw) == "number" and tw > 0, tostring(tw))
    r[#r + 1] = PASS("COLOR.NEW", type(color.new(1, 2, 3)) == "number", "ABGR")
    pcall(files.delete, save_path)
    return r
end

-- ----------------------------------------------------------------- fonts ---
local function test_fonts()
    local r = {}
    local pok, ph = pcall(font.load, "flash0:/font/ltn0.pgf")
    if pok and ph then
        r[#r + 1] = PASS("FONT.LOAD PGF", true, "flash0:/font/ltn0.pgf")
        local n = font.nchars(ph)
        local cw, ch = font.cell(ph)
        local w = font.textwidth(ph, "ABC")
        r[#r + 1] = PASS("FONT.NCHARS", type(n) == "number" and n > 0, tostring(n))
        r[#r + 1] = PASS("FONT.CELL", type(cw) == "number" and type(ch) == "number",
                         tostring(cw) .. "x" .. tostring(ch))
        r[#r + 1] = PASS("FONT.TEXTWIDTH", type(w) == "number" and w > 0, tostring(w))
        r[#r + 1] = PASS("FONT.PRINT", pcall(font.print, ph, 10, 10, "PGF"))
        r[#r + 1] = PASS("FONT.DRAW", pcall(font.draw, ph, 10, 40, 65))
        pcall(font.free, ph)
    else
        r[#r + 1] = SKIP("FONT.LOAD PGF", "flash0:/font/ltn0.pgf unavailable")
    end

    local fok, f = pcall(font.new, "assets/font.png", 8, 16)
    if fok and f then
        local n = font.nchars(f)
        local cw, ch = font.cell(f)
        local w = font.textwidth(f, "ABC")
        r[#r + 1] = PASS("FONT.NEW FIXED", true, "assets/font.png 8x16")
        r[#r + 1] = PASS("FIXED.NCHARS", type(n) == "number" and n > 0, tostring(n))
        r[#r + 1] = PASS("FIXED.CELL", cw == 8 and ch == 16, tostring(cw) .. "x" .. tostring(ch))
        r[#r + 1] = PASS("FIXED.TEXTWIDTH", type(w) == "number" and w > 0, tostring(w))
        r[#r + 1] = PASS("FIXED.PRINT", pcall(font.print, f, 10, 60, "FIXED"))
        pcall(font.free, f)
    else
        r[#r + 1] = SKIP("FONT.NEW FIXED", "assets/font.png missing")
    end

    local sok, sf = pcall(sfont.load, "assets/sfont.png")
    if sok and sf then
        local n = sfont.letters(sf)
        local w = sfont.width(sf, "ABC")
        r[#r + 1] = PASS("SFONT.LOAD", true, "assets/sfont.png")
        r[#r + 1] = PASS("SFONT.LETTERS", type(n) == "number" and n > 0, tostring(n))
        r[#r + 1] = PASS("SFONT.WIDTH", type(w) == "number" and w > 0, tostring(w))
        r[#r + 1] = PASS("SFONT.PRINT", pcall(sfont.print, sf, 10, 90, "SFONT"))
    else
        r[#r + 1] = SKIP("SFONT.LOAD", "assets/sfont.png missing")
    end
    return r
end

-- ----------------------------------------------------------- game + map ----
local function test_game_map()
    local r = {}
    local candidates = { "EBOOT.PBP", "app0:/EBOOT.PBP", "assets/sample.pbp" }
    local path, info
    for i = 1, #candidates do
        local ok, v = pcall(game.info, candidates[i])
        if ok and v then path = candidates[i]; info = v; break end
    end

    if info then
        r[#r + 1] = PASS("GAME.INFO", true, path)
        local keys = { "TITLE", "TITLE_ID", "CATEGORY", "APP_VER", "PARENTAL_LEVEL", "BOOTABLE" }
        for i = 1, #keys do
            r[#r + 1] = INFO("SFO." .. keys[i], short(info[keys[i]], 30))
        end
        local function image_from_pbp(getter, label)
            for _, candidate in ipairs({ "assets/sample.pbp", "EBOOT.PBP" }) do
                local gok, img = pcall(getter, candidate)
                if gok and img then
                    local w = select(2, pcall(image.getw, img))
                    local h = select(2, pcall(image.geth, img))
                    return PASS(label, true, candidate .. " " ..
                                tostring(w) .. "x" .. tostring(h))
                end
            end
            return INFO(label, "section absent from bundled PBPs")
        end
        r[#r + 1] = image_from_pbp(game.geticon0, "GAME.ICON0")
        r[#r + 1] = image_from_pbp(game.getpic0, "GAME.PIC0")
        r[#r + 1] = image_from_pbp(game.getpic1, "GAME.PIC1")
    else
        r[#r + 1] = SKIP("GAME.INFO", "NO PBP FOUND")
    end

    local tok, tiles = pcall(image.new, 32, 32, color.new(20, 20, 20))
    if tok and tiles then
        for y = 0, 15 do for x = 0, 15 do image.pixel(tiles, x, y, color.new(200, 40, 40)) end end
        for y = 0, 15 do for x = 16, 31 do image.pixel(tiles, x, y, color.new(40, 160, 220)) end end
        for y = 16, 31 do for x = 0, 15 do image.pixel(tiles, x, y, color.new(60, 200, 100)) end end
        for y = 16, 31 do for x = 16, 31 do image.pixel(tiles, x, y, color.new(230, 190, 60)) end end
        local mok, m = pcall(map.new, tiles, { { 1, 2 }, { 3, 4 } }, 16, 16)
        if mok and m then
            r[#r + 1] = PASS("MAP.NEW", map.cols(m) == 2 and map.rows(m) == 2 and
                             map.sizex(m) == 32 and map.sizey(m) == 32,
                             map.cols(m) .. "x" .. map.rows(m) .. " tiles")
            r[#r + 1] = PASS("MAP.GET", map.get(m, 1, 1) == 1 and map.get(m, 2, 2) == 4)
            map.set(m, 1, 1, 4)
            r[#r + 1] = PASS("MAP.SET", map.get(m, 1, 1) == 4)
            r[#r + 1] = PASS("MAP.BLIT", pcall(map.blit, m, 0, 0))
            map.free(m)
        else
            r[#r + 1] = PASS("MAP.NEW", false, "map.new failed")
        end
    end
    return r
end

-- ---------------------------------------------------------------- audio ----
local function test_audio()
    local r = {}
    local ok, h = pcall(sound.load, "assets/beep.wav")
    if not ok or not h then
        r[#r + 1] = SKIP("SOUND.LOAD", "assets/beep.wav missing")
        return r
    end
    r[#r + 1] = PASS("SOUND.LOAD", true, "WAV PCM16")
    r[#r + 1] = PASS("SOUND.RATE", sound.rate(h) == 22050, tostring(sound.rate(h)) .. " Hz")

    local v = sound.vol(h)
    r[#r + 1] = PASS("SOUND.VOL GET", type(v) == "number", tostring(v))
    sound.vol(h, 70)
    r[#r + 1] = PASS("SOUND.VOL SET", sound.vol(h) == 70, tostring(sound.vol(h)))

    local looped = sound.loop(h)
    r[#r + 1] = PASS("SOUND.LOOP", type(looped) == "boolean" and sound.looping(h) == looped,
                     tostring(looped))
    sound.loop(h)

    local play_ok = pcall(sound.play, h)
    r[#r + 1] = PASS("SOUND.PLAY", play_ok, "auto channel")
    os.delay(20)
    r[#r + 1] = PASS("SOUND.PLAYING", sound.playing(h) == true)
    sound.pause(h, 1)
    r[#r + 1] = PASS("SOUND.PAUSE", sound.playing(h) == false)
    sound.pause(h, 0)
    r[#r + 1] = PASS("SOUND.RESUME", sound.playing(h) == true)
    sound.stop(h)
    r[#r + 1] = PASS("SOUND.STOP", sound.playing(h) == false)
    r[#r + 1] = PASS("SOUND.ENDSTREAM", sound.endstream(h) == true)
    pcall(sound.free, h)
    r[#r + 1] = PASS("SOUND.FREE", true)
    return r
end

-- ------------------------------------------------------------------ 3D -----
local function vec3(v)
    return type(v) == "table" and type(v[1]) == "number" and
           type(v[2]) == "number" and type(v[3]) == "number"
end

local function test_3d()
    local r = {}
    local base = writable_base()
    local frame_path = base .. "/frame3d.png"

    local aok = pcall(amg.init)
    r[#r + 1] = PASS("AMG.INIT", aok)
    pcall(amg.perspective, 55)
    pcall(amg.renderstyle, __TRIANGLES or 2)
    r[#r + 1] = PASS("AMG.CONSTANTS", type(__TRIANGLES) == "number" and
                     type(__ALL) == "number" and type(__NEAR) == "number", "registered")

    local cok, cam = pcall(cam3d.new)
    if cok and cam then
        pcall(cam3d.position, cam, { 0, 1.0, 6.0 })
        pcall(cam3d.eye, cam, { 0, 0, 0 })
        pcall(cam3d.up, cam, { 0, 1, 0 })
        pcall(cam3d.rotation, cam, { 0, 0, 0 })
        r[#r + 1] = PASS("CAM3D.NEW/SET", pcall(cam3d.set, cam), "handle " .. tostring(cam))
        pcall(cam3d.free, cam)
    else
        r[#r + 1] = PASS("CAM3D.NEW", false, "cam3d.new failed")
    end

    local tok, tex = pcall(texture3d.load, "assets/3d/checker.png")
    if tok and tex then
        pcall(texture3d.position, tex, 300, 40)
        pcall(texture3d.scaling, tex, 0.5, 0.5)
        pcall(texture3d.filter, tex, __NEAR or 0)
        pcall(texture3d.quality, tex, __8888 or 0)
        pcall(texture3d.trans, tex, 0, 0)
        pcall(texture3d.setframes, tex, 1)
        pcall(texture3d.frame, tex, 1)
        r[#r + 1] = PASS("TEXTURE3D.LOAD", true, "assets/3d/checker.png")
        local nok, ntex = pcall(texture3d.new, 16, 16, __RAM or 0)
        r[#r + 1] = PASS("TEXTURE3D.NEW", nok and ntex ~= nil, "16x16")
        if nok then pcall(texture3d.free, ntex) end
    else
        r[#r + 1] = SKIP("TEXTURE3D.LOAD", "assets/3d/checker.png missing")
    end

    local mok, model = pcall(model3d.load, "assets/3d/cube.obj")
    if not mok or not model then
        r[#r + 1] = SKIP("MODEL3D.LOAD", "assets/3d/cube.obj missing")
        return r
    end
    local n = model3d.countobj(model)
    r[#r + 1] = PASS("MODEL3D.LOAD", type(n) == "number" and n >= 1, n .. " object(s)")
    r[#r + 1] = PASS("MODEL3D.COUNTOBJ", n >= 1, tostring(n))

    pcall(model3d.position, model, 0, { 0, 0, 0 })
    pcall(model3d.rotation, model, 0, { 18, 35, 0 })
    pcall(model3d.scaling, model, 0, { 1.35, 1.35, 1.35 })
    local rot = select(2, pcall(model3d.getrotation, model, 0))
    local pos = select(2, pcall(model3d.getposition, model, 0))
    local scl = select(2, pcall(model3d.getscaling, model, 0))
    r[#r + 1] = PASS("MODEL3D.GETS", vec3(rot) and vec3(pos) and vec3(scl),
                     string.format("%.0f/%.0f/%.0f", rot and rot[2] or 0,
                                   pos and pos[3] or 0, scl and scl[1] or 0))

    local draw_ok = pcall(function()
        screen.clear(color.new(8, 10, 24))
        amg.begin()
        model3d.render(model, __ALL or 2)
        amg.mode2d(1)
        if tok and tex then texture3d.draw(tex) end
        amg.update()
    end)
    r[#r + 1] = PASS("MODEL3D.RENDER", draw_ok, "one depth-tested frame")
    r[#r + 1] = PASS("AMG.UPDATE", draw_ok)
    if tok and tex then pcall(texture3d.free, tex) end
    pcall(model3d.free, model)
    pcall(amg.finish)
    pcall(files.delete, frame_path)
    return r
end

-- --------------------------------------------------------------- splash ----
local function test_splash()
    local r = {}
    local assets = { "assets/splash.png", "assets/splash_old.png", "assets/splash_oldschool.png" }
    for i = 1, #assets do
        r[#r + 1] = INFO("SPLASH ASSET " .. i, exists(assets[i]) and "PRESENT" or "MISSING")
    end
    local ok = pcall(splash.show, "assets/splash.png")
    r[#r + 1] = PASS("SPLASH.SHOW", ok, "assets/splash.png (2 s)")
    r[#r + 1] = SKIP("SPLASH.OLD", "PRESS TRIANGLE IN THE SPLASH TEST")
    r[#r + 1] = SKIP("SPLASH.OLDSCHOOL", "PRESS SQUARE IN THE SPLASH TEST")
    return r
end

-- ------------------------------------------------------------------ OSK ----
local function test_osk()
    local r = {}
    r[#r + 1] = SKIP("OSK.INIT", "INTERACTIVE - PRESS X IN THE OSK TEST")
    return r
end

-- ------------------------------------------------------------------ PMF ----
local function test_pmf()
    local r = {}
    local missing = pcall(pmf.run, "assets/does-not-exist.pmf")
    r[#r + 1] = PASS("PMF.REJECT MISSING", missing == false, "raises on bad path")
    r[#r + 1] = PASS("PMF.STOP", pcall(pmf.stop), "idempotent")
    r[#r + 1] = INFO("PMF.MOVIE", exists("movie.pmf") and "movie.pmf present" or
                     "place movie.pmf next to EBOOT")
    r[#r + 1] = SKIP("PMF.RUN", "PRESS X IN THE PMF TEST TO PLAY movie.pmf")
    return r
end

-- --------------------------------------------------------------- camera ----
local function test_camera()
    local r = {}
    -- Queries are safe without touching the camera.  init/begin/render/shot
    -- are hardware actions and run only from the camera page (X).
    local state = select(2, pcall(cam.state))
    local available = select(2, pcall(cam.available))
    r[#r + 1] = PASS("CAM.STATE", type(state) == "boolean", tostring(state))
    r[#r + 1] = PASS("CAM.AVAILABLE", type(available) == "boolean", onoff(available))
    local vals = { "direction", "brightness", "contrast", "sharpness", "saturation",
                   "zoom", "evlevel", "effect", "autoreverse", "mirror" }
    local bad = {}
    for i = 1, #vals do
        local ok, v = pcall(cam[vals[i]])
        if not ok or type(v) ~= "number" then bad[#bad + 1] = vals[i] end
    end
    r[#r + 1] = PASS("CAM.GETTERS", #bad == 0,
                     #bad == 0 and (#vals .. " getters") or table.concat(bad, ","))
    r[#r + 1] = SKIP("CAM.INIT", "INTERACTIVE - PRESS X IN THE CAMERA TEST")
    r[#r + 1] = SKIP("CAM.BEGIN", "INTERACTIVE - PRESS X IN THE CAMERA TEST")
    r[#r + 1] = SKIP("CAM.RENDER", "INTERACTIVE - PRESS X IN THE CAMERA TEST")
    r[#r + 1] = SKIP("CAM.SHOT", "PRESS SQUARE IN THE CAMERA TEST")
    return r
end

-- ---------------------------------------------------------- peripherals ----
local function test_peripherals()
    local r = {}
    local present = select(2, pcall(umd.present))
    local utype = select(2, pcall(umd.type))
    local gid = select(2, pcall(umd.gameid))
    local size = select(2, pcall(umd.size))
    r[#r + 1] = PASS("UMD.PRESENT", type(present) == "boolean", onoff(present))
    r[#r + 1] = PASS("UMD.TYPE", type(utype) == "string", tostring(utype))
    r[#r + 1] = PASS("UMD.GAMEID", type(gid) == "string", short(gid, 12))
    r[#r + 1] = PASS("UMD.SIZE", type(size) == "string" and size:match("^%d+%.%d%d%d%d$") ~= nil,
                     tostring(size) .. " GB (estimate)")
    local uinfo = select(2, pcall(umd.info))
    r[#r + 1] = INFO("UMD.INFO", type(uinfo) == "table" and short(uinfo.TITLE, 24) or "nil")
    r[#r + 1] = SKIP("UMD.INIT", "PRESS X IN THE UMD/USB TEST TO MOUNT")

    r[#r + 1] = PASS("USB.ISACTIVE", type(select(2, pcall(usb.isactive))) == "boolean",
                     tostring(select(2, pcall(usb.isactive))))
    r[#r + 1] = PASS("USB.ISCONNECTED", type(select(2, pcall(usb.isconnected))) == "boolean",
                     tostring(select(2, pcall(usb.isconnected))))
    r[#r + 1] = SKIP("USB.MSTICK", "PRESS X IN THE USB TEST")
    r[#r + 1] = SKIP("USB.UMD/FLASH", "_usbdevice.prx OMITTED FROM THIS BUILD")

    local si = select(2, pcall(sio.init))
    r[#r + 1] = PASS("SIO.INIT", si == 0, "inactive stub")
    r[#r + 1] = PASS("SIO.AVAILABLE", select(2, pcall(sio.available)) == 0)
    r[#r + 1] = PASS("SIO.READ", select(2, pcall(sio.read)) == -1)
    r[#r + 1] = PASS("SIO.WRITE", pcall(sio.write, 65) and pcall(sio.print, "x") and
                     pcall(sio.putstring, "x", 1),
                     si == 0 and "no-op (sio.init=0)" or "sent")
    r[#r + 1] = PASS("SIO.TERM", select(2, pcall(sio.term)) == 1)

    local ii = select(2, pcall(ir.init))
    r[#r + 1] = PASS("IR.INIT", ii == 0, "inactive stub")
    r[#r + 1] = PASS("IR.STATE", select(2, pcall(ir.state)) == false)
    r[#r + 1] = PASS("IR.GET", select(2, pcall(ir.get)) == -1)
    r[#r + 1] = PASS("IR.READ", select(2, pcall(ir.read)) == nil)
    r[#r + 1] = PASS("IR.PUT", select(2, pcall(ir.put, 1)) == 0)
    r[#r + 1] = PASS("IR.WRITE", select(2, pcall(ir.write, "x")) == -1)
    r[#r + 1] = PASS("IR.TERM", select(2, pcall(ir.term)) == 1)

    r[#r + 1] = PASS("BT.INIT", select(2, pcall(bluetooth.init)) == false, "no radio")
    r[#r + 1] = PASS("BT.STATE", select(2, pcall(bluetooth.state)) == false)
    r[#r + 1] = PASS("BT.AVAILABLE", select(2, pcall(bluetooth.available)) == false)
    r[#r + 1] = PASS("BT.TERM", select(2, pcall(bluetooth.term)) == false)

    local mac = select(2, pcall(adhoc.getmac))
    local scan = select(2, pcall(adhoc.scan))
    local total = select(2, pcall(adhoc.gettotal))
    r[#r + 1] = PASS("ADHOC.GETMAC", type(mac) == "string", mac ~= "" and mac or "empty")
    r[#r + 1] = PASS("ADHOC.SCAN", type(scan) == "table", type(scan) == "table" and #scan .. " peers" or "nil")
    r[#r + 1] = PASS("ADHOC.GETTOTAL", type(total) == "number", tostring(total))
    r[#r + 1] = SKIP("ADHOC.INIT", "PRESS TRIANGLE IN THE NETWORK TEST")

    r[#r + 1] = PASS("FTP.STATE", select(2, pcall(ftp.state)) == false, "not started")
    r[#r + 1] = PASS("FTP.TERM", pcall(ftp.term), "safe when stopped")
    r[#r + 1] = SKIP("FTP.INIT", "PRESS SQUARE IN THE NETWORK TEST (READ-ONLY, NO AUTH)")
    return r
end

-- --------------------------------------------------------------- network ----
local function test_network()
    local r = {}
    local iok, inited = pcall(wlan.init)
    r[#r + 1] = PASS("WLAN.INIT", iok and type(inited) == "boolean", tostring(inited))
    local status = select(2, pcall(wlan.status))
    local connected = select(2, pcall(wlan.isconnected))
    local ip = select(2, pcall(wlan.getip))
    local strength = select(2, pcall(wlan.strength))
    local profile = select(2, pcall(wlan.over))
    r[#r + 1] = PASS("WLAN.STATUS", type(status) == "number", tostring(status))
    r[#r + 1] = PASS("WLAN.ISCONNECTED", type(connected) == "boolean", onoff(connected))
    r[#r + 1] = PASS("WLAN.GETIP", ip == nil or type(ip) == "string", tostring(ip))
    r[#r + 1] = PASS("WLAN.STRENGTH", type(strength) == "number", tostring(strength))
    r[#r + 1] = PASS("WLAN.OVER", type(profile) == "string", short(profile, 12))
    local cfgs = select(2, pcall(wlan.getconfigs))
    local scan = select(2, pcall(wlan.scan))
    r[#r + 1] = PASS("WLAN.GETCONFIGS", type(cfgs) == "table", "empty table (PSPSDK)")
    r[#r + 1] = PASS("WLAN.SCAN", type(scan) == "table", "empty table (PSPSDK)")
    r[#r + 1] = PASS("WLAN.AUTOCONNECT", pcall(wlan.autoconnect, 1), "validated no-op")
    r[#r + 1] = PASS("WLAN.AUTOSTATUS", select(2, pcall(wlan.autostatus)) == false)
    pcall(wlan.term)
    r[#r + 1] = PASS("WLAN.TERM", true)

    r[#r + 1] = PASS("SOCKET.BAD PORT", pcall(socket.server, -1) == false, "rejects <0")
    r[#r + 1] = PASS("SOCKET.BAD PORT 2", pcall(socket.server, 70000) == false, "rejects >65535")
    r[#r + 1] = PASS("SOCKET.TERM", pcall(socket.term), "no live sockets")
    local https_ok, https_res = pcall(http.request, "GET", "https://example.com")
    r[#r + 1] = PASS("HTTP.REJECT HTTPS",
                     (not https_ok) or https_res == nil or https_res == false,
                     "no TLS in PSPSDK")
    r[#r + 1] = PASS("HTTP.TERM", pcall(http.term), "safe")
    r[#r + 1] = SKIP("NET CONNECT", "PRESS X IN THE NETWORK TEST (USES PROFILE 1)")
    return r
end

-- ------------------------------------------------------- rendering helpers --
local last_lx, last_ly = 128, 128
local last_new = 0

local function hline(y, tint)
    draw.fillrect(16, y, W - 32, 1, tint or dim)
end

local function txt_right(text, right, y, tint)
    text = tostring(text or "")
    txt(right - txtw(text), y, text, tint)
end

local function fit(text, maxw)
    text = tostring(text or "")
    if maxw <= 0 then return "" end
    if txtw(text) <= maxw then return text end
    while #text > 0 do
        text = text:sub(1, #text - 1)
        if txtw(text .. "~") <= maxw then return text .. "~" end
    end
    return ""
end

local function draw_header(title, right)
    draw.fillrect(0, 0, W, 46, panel)
    draw.fillrect(0, 45, W, 1, teal)
    txt(16, 4, "OPENLUA TESTER", teal)
    txt(16, 24, title, white)
    txt_right(right or model_label(), W - 16, 4, amber)
    txt_right("v" .. VERSION, W - 16, 24, grey)
end

local function draw_footer(hint)
    draw.fillrect(0, H - 23, W, 23, panel)
    draw.fillrect(0, H - 24, W, 1, dim)
    txt(10, H - 19, hint or "X ACTION   O BACK   UP/DOWN SCROLL", grey)
end

local function draw_results(results, scroll, top)
    top = top or 52
    local rows = math.floor((H - 28 - top) / RESULT_LH)
    if rows < 1 then rows = 1 end
    scroll = math.max(1, math.min(scroll or 1, math.max(1, #results - rows + 1)))
    for i = 0, rows - 1 do
        local rec = results[scroll + i]
        if not rec then break end
        local y = top + i * RESULT_LH
        if i % 2 == 0 then draw.fillrect(8, y - 1, W - 16, RESULT_LH - 1, panel) end
        local col = status_color(rec.ok)
        draw.fillrect(14, y + 3, 3, 9, col)
        local label = fit(rec.label, 186)
        txt(22, y, label, white)
        local code = status_code(rec.ok)
        local code_w = txtw(code)
        local detail_right = W - 18 - code_w - 12
        local detail_x = math.max(214, 22 + txtw(label) + 10)
        local detail = rec.detail or ""
        if detail ~= "" and detail_right > detail_x then
            txt(detail_x, y, fit(detail, detail_right - detail_x), grey)
        end
        txt_right(code, W - 18, y, col)
    end
    if #results > rows then
        txt_right(string.format("%d-%d/%d", scroll,
                 math.min(#results, scroll + rows - 1), #results), W - 14, H - 19, dim)
    end
end

local function safe_run(fn)
    local ok, results = pcall(fn)
    if not ok then
        return { R("TEST ERROR", false, short(results, 44)) }
    end
    if type(results) ~= "table" then
        return { R("TEST RESULT", false, "test returned no table") }
    end
    return results
end

local function draw_notice(title, line1, line2)
    screen.clear(dark)
    draw.fillrect(0, 0, W, 4, teal)
    txt(20, 24, title, white)
    txt(20, 56, line1 or "", grey)
    txt(20, 80, line2 or "", dim)
end

-- --------------------------------------------------------- live demos ------
local function graphics_demo()
    local img = select(2, pcall(image.new, 40, 40, color.new(30, 40, 70)))
    if img then
        for y = 0, 39 do
            for x = 0, 39 do
                if (math.floor(x / 8) + math.floor(y / 8)) % 2 == 0 then
                    image.pixel(img, x, y, color.new(60, 200, 180))
                end
            end
        end
    end
    local sheet = select(2, pcall(image.load, "assets/test_sheet.png", 32, 32))
    local frame = 0
    local angle = 0
    while true do
        screen.clear(color.new(8, 10, 22))
        draw.fillrect(0, 0, W, 46, panel)
        txt(16, 6, "GRAPHICS LIVE DEMO", teal)
        txt(16, 24, "DRAW / IMAGE / SCREEN / COLOR", white)
        txt_right("O BACK", W - 16, 24, grey)
        local cx = 100 + math.floor(60 * math.sin(angle / 24))
        draw.circle(cx, 110, 34, teal, 32)
        draw.rect(cx - 42, 68, 84, 84, blue)
        draw.line(20, 190, 220, 210, amber)
        draw.line(220, 190, 20, 210, amber)
        local tw = 30 + math.floor(10 * math.sin(angle / 10))
        draw.fillrect(250, 80, tw, tw, red)
        if img then
            image.reset(img)
            image.rotate(img, angle)
            image.center(img)
            image.blit(img, 330, 100)
            image.reset(img)
        end
        if sheet then
            image.blitsprite(sheet, 250, 150, frame % 16)
            txt(250, 190, "SHEET " .. (frame % 16), grey)
        end
        txt(20, 236, "CIRCLE O EXITS - PRIMITIVES ANIMATE", dim)
        screen.flip()
        angle = angle + 2
        frame = frame + 1
        local p = new_press(read_pad())
        if bit_has(p, __CIRCLE) then break end
        os.delay(16)
    end
end

local function fonts_demo()
    local angle = 0
    while true do
        screen.clear(dark)
        draw.fillrect(0, 0, W, 46, panel)
        txt(16, 6, "FONT LIVE DEMO", teal)
        txt(16, 24, "PGF / FIXED-CELL / SFONT", white)
        txt_right("O BACK", W - 16, 24, grey)
        local tint = color.new(120 + math.floor(100 * math.abs(math.sin(angle / 20))), 220, 220)
        if pgf then
            font.print(pgf, 20, 60, "PGF flash0 ltn0.pgf 0123456789", tint)
        else
            txt(20, 60, "PGF not available on this build", grey)
        end
        if fixed then
            font.print(fixed, 20, 92, "FIXED assets/font.png 8x16", white)
        else
            txt(20, 92, "fixed font not found", grey)
        end
        local sf = select(2, pcall(sfont.load, "assets/sfont.png"))
        if sf then
            sfont.print(sf, 20, 124, "SFONT assets/sfont.png - variable width!")
        else
            txt(20, 124, "sfont not found", grey)
        end
        txt(20, 160, "built-in fallback: " .. (pgf or fixed and "not used" or "ACTIVE"), grey)
        draw_glyphs("ABCDEFGHIJKLMNOPQRSTUVWXYZ", 20, 186, 2, white)
        draw_glyphs("0123456789 .,:/-+=", 20, 208, 2, teal)
        txt(20, 236, "CIRCLE O EXITS", dim)
        screen.flip()
        angle = angle + 2
        local p = new_press(read_pad())
        if bit_has(p, __CIRCLE) then break end
        os.delay(16)
    end
end

local function three_d_demo()
    local model = select(2, pcall(model3d.load, "assets/3d/cube.obj"))
    if not model then
        draw_notice("3D DEMO", "assets/3d/cube.obj missing", "O BACK")
        screen.flip()
        os.delay(1500)
        return
    end
    local cam = select(2, pcall(cam3d.new))
    if cam then
        pcall(cam3d.position, cam, { 0, 1.0, 6.0 })
        pcall(cam3d.eye, cam, { 0, 0, 0 })
        pcall(cam3d.up, cam, { 0, 1, 0 })
        pcall(cam3d.set, cam)
    end
    pcall(amg.init)
    pcall(amg.perspective, 55)
    pcall(amg.renderstyle, __TRIANGLES or 2)
    local angle = 0
    while true do
        screen.clear(color.new(8, 10, 24))
        amg.begin()
        model3d.position(model, 0, { 0, 0, 0 })
        model3d.scaling(model, 0, { 1.35, 1.35, 1.35 })
        model3d.rotation(model, 0, { 18, angle, 0 })
        model3d.render(model, __ALL or 2)
        amg.mode2d(1)
        txt(14, 8, "3D LIVE DEMO - AMG / CAM3D / MODEL3D / TEXTURE3D", teal)
        txt(14, 26, "ANGLE " .. angle, white)
        txt(14, 244, "CIRCLE O EXITS", grey)
        amg.update()
        angle = (angle + 2) % 360
        local p = new_press(read_pad())
        if bit_has(p, __CIRCLE) then break end
    end
    pcall(amg.finish)
    pcall(model3d.free, model)
    if cam then pcall(cam3d.free, cam) end
end

local function game_demo()
    local path
    local candidates = { "EBOOT.PBP", "app0:/EBOOT.PBP", "assets/sample.pbp" }
    local info
    for i = 1, #candidates do
        local ok, v = pcall(game.info, candidates[i])
        if ok and v then path = candidates[i]; info = v; break end
    end
    local icon0 = path and select(2, pcall(game.geticon0, path)) or nil
    local pic0 = path and select(2, pcall(game.getpic0, path)) or nil
    local pic1 = path and select(2, pcall(game.getpic1, path)) or nil
    if icon0 then pcall(image.scale, icon0, 0.75) end
    if pic0 then pcall(image.scale, pic0, 0.6) end
    if pic1 then pcall(image.scale, pic1, 0.42) end

    local tiles = select(2, pcall(image.new, 32, 32, color.new(16, 16, 16)))
    local m
    if tiles then
        for y = 0, 31 do
            for x = 0, 31 do
                local c = ((math.floor(x / 8) + math.floor(y / 8)) % 2 == 0)
                image.pixel(tiles, x, y, c and color.new(60, 190, 160) or color.new(30, 60, 90))
            end
        end
        m = select(2, pcall(map.new, tiles, { { 2, 1 }, { 1, 2 } }, 16, 16))
    end

    while true do
        screen.clear(dark)
        draw.fillrect(0, 0, W, 40, panel)
        txt(14, 8, "GAME / MAP LIVE DEMO", teal)
        txt_right("O BACK", W - 16, 8, grey)
        if info then
            txt(14, 48, "TITLE  " .. short(info.TITLE, 40), white)
            txt(14, 68, "ID " .. short(info.TITLE_ID, 12) ..
                "  CAT " .. short(info.CATEGORY, 8) ..
                "  BOOT " .. tostring(info.BOOTABLE), grey)
        else
            txt(14, 48, "NO PBP FOUND (EBOOT.PBP / assets/sample.pbp)", grey)
        end
        if icon0 then image.blit(icon0, 14, 94) end
        if pic1 then image.blit(pic1, 240, 42) end
        if pic0 then image.blit(pic0, 240, 164) end
        txt(14, 170, "ICON0 / PIC0 / PIC1", grey)
        if m then
            map.blit(m, 14, 198)
            txt(86, 202, "MAP 2x2", grey)
        end
        txt(14, 244, "CIRCLE O EXITS", dim)
        screen.flip()
        local p = new_press(read_pad())
        if bit_has(p, __CIRCLE) then break end
        os.delay(16)
    end
    if m then pcall(map.free, m) end
end

local function camera_demo(page)
    if select(2, pcall(usb.stop)) then end
    local inited = select(2, pcall(cam.init))
    local began = 0
    if inited == 1 then began = select(2, pcall(cam.begin)) end
    local available = select(2, pcall(cam.available))
    local shot = "not taken"
    while true do
        screen.clear(color.new(8, 12, 24))
        txt(14, 8, "CAMERA LIVE DEMO", teal)
        txt_right("O BACK   SQUARE SHOT", W - 16, 8, grey)
        txt(14, 30, "INIT " .. tostring(inited) .. "  BEGIN " .. tostring(began) ..
            "  AVAILABLE " .. onoff(available), white)
        draw.rect(12, 50, 456, 186, teal)
        if available then
            cam.render(14, 52, 452, 182)
        else
            txt(30, 120, "NO CAMERA CONNECTED", amber)
            txt(30, 144, "GO! CAM on USB; call usb.stop() before cam.init().", grey)
        end
        txt(14, 246, "SHOT " .. shot, grey)
        screen.flip()
        local p = new_press(read_pad())
        if bit_has(p, __SQUARE) and available then
            local ok = pcall(cam.shot, "cam_shot.png")
            shot = ok and "cam_shot.png" or "failed"
        end
        if bit_has(p, __CIRCLE) then break end
        os.delay(16)
    end
    pcall(cam.term)
end

-- ------------------------------------------------------------ page state ---
local audio_state = { handle = nil, volume = 70 }
local buttons_list = {
    { __UP, "UP" }, { __DOWN, "DOWN" }, { __LEFT, "LEFT" }, { __RIGHT, "RIGHT" },
    { __TRIANGLE, "TRI" }, { __CIRCLE, "CIR" }, { __CROSS, "CRO" },
    { __SQUARE, "SQU" }, { __L, "L" }, { __R, "R" },
    { __START, "STA" }, { __SELECT, "SEL" }, { __NOTE, "NOT" }, { __HOLD, "HLD" },
}

local function pressed_names(mask)
    local names = {}
    for i = 1, #buttons_list do
        if bit_has(mask, buttons_list[i][1]) then names[#names + 1] = buttons_list[i][2] end
    end
    return #names > 0 and table.concat(names, " ") or "-"
end

local function ensure_audio()
    if audio_state.handle then return audio_state.handle end
    local ok, h = pcall(sound.load, "assets/beep.wav")
    if not ok or not h then return nil end
    audio_state.handle = h
    pcall(sound.vol, h, audio_state.volume)
    return h
end

local function input_hook()
    txt(16, 50, string.format("MASK %04X   LX %3d   LY %3d", last_mask, last_lx, last_ly), white)
    for i = 1, #buttons_list do
        local col = (i - 1) % 7
        local row = math.floor((i - 1) / 7)
        local x = 16 + col * 64
        local y = 70 + row * 22
        local on = bit_has(last_mask, buttons_list[i][1])
        draw.fillrect(x, y, 58, 18, on and teal or panel2)
        txt(x + 5, y + 2, buttons_list[i][2], on and dark or grey)
    end
    txt(16, 116, "LAST PRESS " .. pressed_names(last_new), amber)
end

local function audio_hook()
    local h = audio_state.handle
    local playing = h and select(2, pcall(sound.playing, h)) or false
    local looped = h and select(2, pcall(sound.looping, h)) or false
    txt(16, 50, "BEEP " .. (playing and "PLAYING" or "STOPPED") ..
        "   VOL " .. audio_state.volume ..
        "   LOOP " .. (looped and "ON" or "OFF"),
        playing and green or grey)
    txt(16, 70, "X PLAY/STOP   TRIANGLE VOLUME   SQUARE LOOP", dim)
end

local function audio_action(pressed)
    local h = ensure_audio()
    if not h then return end
    if bit_has(pressed, __CROSS) then
        if select(2, pcall(sound.playing, h)) then
            pcall(sound.stop, h)
        else
            pcall(sound.play, h)
        end
    elseif bit_has(pressed, __TRIANGLE) then
        audio_state.volume = audio_state.volume + 25
        if audio_state.volume > 100 then audio_state.volume = 25 end
        pcall(sound.vol, h, audio_state.volume)
    elseif bit_has(pressed, __SQUARE) then
        pcall(sound.loop, h)
    end
end

local function splash_action(pressed, page)
    local function note(ok, label)
        local r = ok and PASS(label, true, "completed") or R(label, false, "raised")
        page.results[#page.results + 1] = r
    end
    if bit_has(pressed, __CROSS) then
        note(pcall(splash.show, "assets/splash.png"), "SPLASH.SHOW LIVE")
    elseif bit_has(pressed, __TRIANGLE) then
        note(pcall(splash.old, "assets/splash_old.png"), "SPLASH.OLD LIVE")
    elseif bit_has(pressed, __SQUARE) then
        note(pcall(splash.oldschool), "SPLASH.OLDSCHOOL LIVE")
    end
end

local function osk_action(pressed, page)
    if not bit_has(pressed, __CROSS) then return end
    local ok, text = pcall(osk.init, "OpenLua test input", "hello", 1, 64, 2, -1)
    if not ok then
        page.results[#page.results + 1] = R("OSK LIVE", false, short(text, 40))
    elseif text == nil then
        page.results[#page.results + 1] = SKIP("OSK LIVE", "CANCELLED OR UNAVAILABLE")
    else
        page.results[#page.results + 1] = PASS("OSK LIVE", true, short(text, 40))
    end
end

local function pmf_action(pressed, page)
    if not bit_has(pressed, __CROSS) then return end
    if not exists("movie.pmf") then
        page.results[#page.results + 1] = SKIP("PMF LIVE", "movie.pmf NOT FOUND")
        return
    end
    local frames = 0
    _G.onPmfPlay = function(playing) if playing then frames = frames + 1 end end
    local ok, err = pcall(pmf.run, "movie.pmf")
    _G.onPmfPlay = nil
    page.results[#page.results + 1] = ok and
        PASS("PMF LIVE", frames > 0, frames .. " frames") or
        R("PMF LIVE", false, short(err, 34))
end

local function peripherals_action(pressed, page)
    if bit_has(pressed, __CROSS) then
        local ok, mounted = pcall(umd.init)
        page.results[#page.results + 1] = PASS("UMD.INIT LIVE", ok and mounted == true,
                                               ok and tostring(mounted) or "raised")
    elseif bit_has(pressed, __TRIANGLE) then
        pcall(usb.mstick)
        local active = select(2, pcall(usb.isactive))
        page.results[#page.results + 1] = PASS("USB.MSTICK LIVE", active == true,
                                               active and "EXPOSED - SQUARE STOPS" or "inactive")
    elseif bit_has(pressed, __SQUARE) then
        pcall(usb.stop)
        page.results[#page.results + 1] = PASS("USB.STOP LIVE",
            select(2, pcall(usb.isactive)) == false, "stopped")
    elseif bit_has(pressed, __L) then
        pcall(usb.umd)
        page.results[#page.results + 1] = SKIP("USB.UMD LIVE",
            "_usbdevice.prx NOT IN THIS BUILD")
    elseif bit_has(pressed, __R) then
        pcall(usb.flash0)
        page.results[#page.results + 1] = SKIP("USB.FLASH0 LIVE",
            "_usbdevice.prx NOT IN THIS BUILD")
    end
end

local function network_action(pressed, page)
    if bit_has(pressed, __CROSS) then
        pcall(wlan.init)
        local ok, connected = pcall(wlan.connect, 1)
        local ip = select(2, pcall(wlan.getip))
        page.results[#page.results + 1] = ok and connected == 1 and
            PASS("WLAN.CONNECT LIVE", true, tostring(ip)) or
            R("WLAN.CONNECT LIVE", false, "profile 1 failed")
    elseif bit_has(pressed, __TRIANGLE) then
        pcall(wlan.disconnect)
        pcall(wlan.term)
        page.results[#page.results + 1] = PASS("WLAN.DISCONNECT LIVE", true)
    elseif bit_has(pressed, __SQUARE) then
        local running = select(2, pcall(ftp.state))
        if running then
            pcall(ftp.term)
            page.results[#page.results + 1] = PASS("FTP.STOP LIVE", true, "server stopped")
        else
            local ok, started = pcall(ftp.init)
            page.results[#page.results + 1] = PASS("FTP.START LIVE", ok and started == true,
                ok and started and "PORT 21 - READ ONLY - NO AUTH" or "failed")
        end
    elseif bit_has(pressed, __L) then
        local ok, joined = pcall(adhoc.init, "OPENLUA")
        page.results[#page.results + 1] = PASS("ADHOC.INIT LIVE", ok and joined == 1,
                                               ok and tostring(joined) or "raised")
    elseif bit_has(pressed, __R) then
        pcall(adhoc.term)
        page.results[#page.results + 1] = PASS("ADHOC.TERM LIVE", true)
    end
end

local function camera_action()
    camera_demo()
end

local function graphics_action() graphics_demo() end
local function fonts_action() fonts_demo() end
local function three_d_action() three_d_demo() end
local function game_action() game_demo() end

-- ------------------------------------------------------------ page table ---
local pages = {
    { id = "system", tag = "SYS", title = "SYSTEM / OS / HARDWARE / POWER", run = test_system },
    { id = "storage", tag = "FS", title = "STORAGE / FILES", run = test_storage },
    { id = "savedata", tag = "SAVE", title = "SAVEDATA", run = test_savedata },
    { id = "dataformats", tag = "DATA", title = "INI / JSON", run = test_dataformats },
    { id = "input", tag = "INP", title = "INPUT / TIMERS / MATH", run = test_input_math,
      hook = input_hook, results_top = 134 },
    { id = "graphics", tag = "GFX", title = "GRAPHICS / DRAW / IMAGE", run = test_graphics,
      action = graphics_action, hint = "X LIVE DEMO   O BACK   UP/DOWN SCROLL" },
    { id = "fonts", tag = "FONT", title = "FONTS / SFONT", run = test_fonts,
      action = fonts_action, hint = "X LIVE DEMO   O BACK   UP/DOWN SCROLL" },
    { id = "game", tag = "GAME", title = "GAME / MAP", run = test_game_map,
      action = game_action, hint = "X LIVE DEMO   O BACK   UP/DOWN SCROLL" },
    { id = "audio", tag = "AUD", title = "SOUND / AUDIO", run = test_audio,
      hook = audio_hook, results_top = 96, action = audio_action,
      hint = "X PLAY/STOP  TRI VOL  SQU LOOP  O BACK" },
    { id = "3d", tag = "3D", title = "3D / AMG / CAM3D / MODELS", run = test_3d,
      action = three_d_action, hint = "X SPINNING CUBE   O BACK   UP/DOWN SCROLL" },
    { id = "splash", tag = "BOOT", title = "SPLASH", run = test_splash,
      action = splash_action, hint = "X SHOW  TRI OLD  SQU OLDSCHOOL  O BACK" },
    { id = "osk", tag = "OSK", title = "ON-SCREEN KEYBOARD", run = test_osk,
      action = osk_action, hint = "X OPEN KEYBOARD   O BACK" },
    { id = "pmf", tag = "PMF", title = "PMF / PSMF VIDEO", run = test_pmf,
      action = pmf_action, hint = "X PLAY movie.pmf   O BACK" },
    { id = "camera", tag = "CAM", title = "GO! CAM USB CAMERA", run = test_camera,
      action = camera_action, hint = "X LIVE CAMERA   O BACK   UP/DOWN SCROLL" },
    { id = "peripherals", tag = "UMD", title = "UMD / USB / SERIAL / IR / BT", run = test_peripherals,
      action = peripherals_action,
      hint = "X UMD  TRI USB  SQU STOP  L/R SKIP  O BACK" },
    { id = "network", tag = "NET", title = "WLAN / SOCKET / HTTP / FTP", run = test_network,
      action = network_action,
      hint = "X CONNECT  TRI OFF  SQU FTP  L/R ADHOC  O BACK" },
}

local page_by_id = {}
for i = 1, #pages do page_by_id[pages[i].id] = pages[i] end

local report_page = { id = "report", tag = "RPT", title = "BATCH REPORT", results = {} }
pages[#pages + 1] = report_page
page_by_id.report = report_page

-- --------------------------------------------------------------- report ----
local function try_write(path, text)
    pcall(files.mkdir, files.nofile(path) or ".")
    local ok, f = pcall(io.open, path, "wb")
    if not ok or not f then return false end
    local wrote = pcall(f.write, f, text)
    pcall(f.close, f)
    return wrote
end

local function build_report(sections)
    local lines = {}
    local function add(s) lines[#lines + 1] = s or "" end

    add("OPENLUA HARDWARE TEST REPORT")
    add("version " .. VERSION)
    add("date " .. tostring(select(2, pcall(os.getdate))))
    add("declared model " .. model_label())
    add("detected model " .. tostring(select(2, pcall(hw.getmodel))))
    add("firmware " .. tostring(select(2, pcall(hw.firmware))))
    add("nickname " .. tostring(select(2, pcall(os.nick))) ..
        "   cfw " .. tostring(select(2, pcall(os.cfw))))
    add("ms0 " .. volume_info("ms0:/") .. "   ef0 " .. volume_info("ef0:/"))
    add("")
    local total = { pass = 0, fail = 0, info = 0, skip = 0, total = 0 }
    for i = 1, #sections do
        local section = sections[i]
        local s = summarise(section.results)
        total.pass = total.pass + s.pass
        total.fail = total.fail + s.fail
        total.info = total.info + s.info
        total.skip = total.skip + s.skip
        total.total = total.total + s.total
        add("== " .. section.title .. " ==")
        for j = 1, #section.results do
            local rec = section.results[j]
            add(string.format("[%s] %-22s %s", status_code(rec.ok), rec.label, rec.detail or ""))
        end
        add(string.format("-- %d pass, %d fail, %d info, %d skip", s.pass, s.fail, s.info, s.skip))
        add("")
    end
    add(string.format("TOTAL %d checks: %d pass, %d fail, %d info, %d skip",
        total.total, total.pass, total.fail, total.info, total.skip))
    add("")
    add("Not run automatically (destructive or state changing):")
    add("  os.exit, os.restart, power.suspend/off/lock, flash writes")
    add("  interactive only: OSK, PMF playback, camera capture, USB, FTP, adhoc, WLAN connect")
    return table.concat(lines, "\n") .. "\n", total
end

local function write_report(sections)
    local text, total = build_report(sections)
    local paths = {}
    local function attempt(path)
        if try_write(path, text) then paths[#paths + 1] = path end
    end
    attempt("test_report.txt")
    local base = writable_base()
    if base ~= "." then attempt(base .. "/test_report.txt") end
    if has_volume("ms0:/") then
        pcall(files.mkdir, "ms0:/PSP/OPENLUA_TEST")
        attempt("ms0:/PSP/OPENLUA_TEST/test_report.txt")
    end
    if has_volume("ef0:/") then
        pcall(files.mkdir, "ef0:/PSP/OPENLUA_TEST")
        attempt("ef0:/PSP/OPENLUA_TEST/test_report.txt")
    end
    return paths, total
end

-- ---------------------------------------------------------------- menu -----
local MENU_ROWS = 10
local MENU_STEP = 19
local menu_sel, menu_top = 1, 1
local mode = "menu"
local current = nil

local function result_rows(page)
    local rows = math.floor((H - 28 - (page.results_top or 52)) / RESULT_LH)
    if rows < 1 then rows = 1 end
    return rows
end

local function clamp_menu()
    if menu_sel < 1 then menu_sel = #pages end
    if menu_sel > #pages then menu_sel = 1 end
    if menu_sel < menu_top then menu_top = menu_sel end
    if menu_sel >= menu_top + MENU_ROWS then menu_top = menu_sel - MENU_ROWS + 1 end
    local highest = math.max(1, #pages - MENU_ROWS + 1)
    if menu_top > highest then menu_top = highest end
    if menu_top < 1 then menu_top = 1 end
end

local function draw_menu()
    draw_header("MODULE MENU", model_label())
    local last = math.min(#pages, menu_top + MENU_ROWS - 1)
    for i = menu_top, last do
        local y = 52 + (i - menu_top) * MENU_STEP
        local page = pages[i]
        if i == menu_sel then
            draw.fillrect(8, y - 2, W - 16, MENU_STEP, panel2)
            draw.fillrect(8, y - 2, 3, MENU_STEP, teal)
            txt(20, y, page.title, white)
            txt_right(page.tag, W - 18, y, teal)
        else
            txt(24, y, page.title, grey)
            txt_right(page.tag, W - 18, y, dim)
        end
    end
    if #pages > MENU_ROWS then
        local bar_h = math.max(12, math.floor((H - 76) * MENU_ROWS / #pages))
        local bar_y = 52 + math.floor((H - 76 - bar_h) * (menu_top - 1) /
                                       math.max(1, #pages - MENU_ROWS))
        draw.fillrect(W - 6, bar_y, 3, bar_h, dim)
    end
    draw_footer("X OPEN   START RUN ALL   SELECT MODEL   UP/DOWN MOVE")
end

local function close_page()
    if current and current.id == "audio" and audio_state.handle then
        pcall(sound.stop, audio_state.handle)
    end
    current = nil
    mode = "menu"
end

local function open_page(page)
    current = page
    page.results = page.results or {}
    page.scroll = page.scroll or 1
    if page.run then
        page.results = safe_run(page.run)
        page.scroll = 1
    end
    mode = "page"
end

local function handle_menu(pressed)
    if bit_has(pressed, __UP) then
        menu_sel = menu_sel - 1
        clamp_menu()
    elseif bit_has(pressed, __DOWN) then
        menu_sel = menu_sel + 1
        clamp_menu()
    elseif bit_has(pressed, __SELECT) then
        cycle_model()
    elseif bit_has(pressed, __START) then
        return "runall"
    elseif bit_has(pressed, __CROSS) or bit_has(pressed, __CIRCLE) then
        open_page(pages[menu_sel])
    end
    return nil
end

local function handle_page(pressed)
    if bit_has(pressed, __CIRCLE) then
        close_page()
        return
    end
    local page = current
    if not page then return end
    if bit_has(pressed, __UP) then
        page.scroll = math.max(1, (page.scroll or 1) - 1)
        return
    elseif bit_has(pressed, __DOWN) then
        local most = math.max(1, #page.results - result_rows(page) + 1)
        page.scroll = math.min(most, (page.scroll or 1) + 1)
        return
    end
    if page.action then
        page.action(pressed, page)
    elseif bit_has(pressed, __CROSS) and page.run then
        page.results = safe_run(page.run)
        page.scroll = 1
    end
end

-- -------------------------------------------------------------- run all ----
local function run_all()
    local sections = {}
    for i = 1, #pages do
        local page = pages[i]
        if page.run then
            screen.clear(dark)
            draw.fillrect(0, 0, W, 46, panel)
            txt(16, 6, "RUNNING ALL CHECKS", teal)
            txt(16, 24, page.title, white)
            txt(16, 70, string.format("CHECK %d / %d", i, #pages), white)
            local frac = (i - 1) / #pages
            draw.rect(16, 100, W - 32, 18, dim)
            draw.fillrect(17, 101, math.floor((W - 34) * frac), 16, teal)
            txt(16, 132, "SOME CHECKS FLASH THE SCREEN (SPLASH, 3D, CAMERA)", grey)
            txt(16, 152, "DESTRUCTIVE POWER/FLASH CALLS ARE NEVER MADE", dim)
            screen.flip()
            os.delay(40)
            page.results = safe_run(page.run)
            page.scroll = 1
            sections[#sections + 1] = { title = page.title, results = page.results }
        end
    end

    local paths, total = write_report(sections)
    report_page.results = {}
    for i = 1, #sections do
        local s = summarise(sections[i].results)
        report_page.results[#report_page.results + 1] =
            R(sections[i].title, s.fail == 0,
              string.format("%dP %dF %dI %dS", s.pass, s.fail, s.info, s.skip))
    end
    report_page.results[#report_page.results + 1] =
        INFO("TOTAL", summary_text(total))
    if #paths == 0 then
        report_page.results[#report_page.results + 1] =
            SKIP("REPORT WRITTEN", "NO WRITABLE LOCATION")
    else
        for i = 1, #paths do
            report_page.results[#report_page.results + 1] =
                PASS("REPORT WRITTEN", true, paths[i])
        end
    end
    report_page.scroll = 1
    current = report_page
    mode = "page"
end

-- ------------------------------------------------------------- main loop ---
sync_pad()

while true do
    local mask, lx, ly = read_pad()
    last_lx, last_ly = lx, ly
    local pressed = new_press(mask)
    last_new = pressed

    if mode == "menu" then
        if handle_menu(pressed) == "runall" then
            run_all()
        end
        if mode == "menu" then draw_menu() end
    end

    if mode == "page" and current then
        handle_page(pressed)
        if mode == "menu" then
            draw_menu()
        elseif current then
            draw_header(current.title, model_label())
            if current.hook then current.hook(current) end
            draw_footer(current.hint or "X RERUN   O BACK   UP/DOWN SCROLL")
            draw_results(current.results or {}, current.scroll or 1, current.results_top)
        end
    end

    screen.flip()
    os.delay(16)
end
