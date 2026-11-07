-- OpenLua - storage / PSP Go compatibility demo.
--
--   files.cdir()                     app directory after boot
--   os.infoms0() / os.infoef0()      M2 and PSP Go internal-storage capacity
--   data.save/load/delete            PSP/SAVEDATA probing
--   files.mkdir/list/copy/move/delete
--
-- The file probe only touches PSP/OPENLUA_COMPAT on each writable volume and
-- removes it again.  On a PSP Go the internal volume is ef0: and the M2 slot
-- is ms0:.  Volume presence is detected with os.infoms0()/os.infoef0(), not by
-- stat()ing a volume root: real PSP firmware rejects a stat on "ms0:/" itself,
-- while a child path (or statvfs) works.
--
-- PPSSPP does not emulate the PSP Go's ef0: volume, so it reports that volume
-- as absent while the app directory and ms0: paths still run normally.

local f = font.new("assets/font.png", 8, 16)
if f then
    screen.font(f)
end

local white = color.new(238, 240, 246)
local muted = color.new(120, 132, 154)
local teal  = color.new(48, 180, 160)
local amber = color.new(235, 180, 70)
local red   = color.new(230, 96, 96)
local dark  = color.new(10, 14, 26)

local function yes(ok)
    return ok and "YES" or "NO"
end

local function tint(ok)
    return ok and teal or red
end

local function mb(info)
    if type(info) ~= "table" or type(info.max) ~= "number" then
        return "-"
    end
    return string.format("%.1f/%.1f MB", info.used / (1024 * 1024),
                         info.max / (1024 * 1024))
end

local cwd = files.cdir()
local model = tostring(hw.getmodel())

-- statvfs on a device root is supported on real firmware; stat() on it is not.
-- Use the capacity queries first, then a child directory (a formatted volume
-- always has PSP/ because the EBOOT lives there) for emulators that do not
-- implement statvfs.
local ms0_info = os.infoms0()
local ef0_info = os.infoef0()
local ms0_present = ms0_info ~= nil or files.exists("ms0:/PSP")
local ef0_present = ef0_info ~= nil or files.exists("ef0:/PSP")

-- app0: resolves to the EBOOT directory.  Fall back to the current directory
-- because some loaders only expose the app directory through cwd.
local app_present = files.exists("app0:/script.lua") or
                    files.exists("script.lua")

-- Savedata: probes every root, creates PSP/SAVEDATA when missing and keeps a
-- new game on the first writable volume.
local GAME = "OPENLUA1"
data.delete(GAME)
local save_ok = data.save("pspgo", "OpenLua", GAME, "Storage", "compat", 1)
local load_ok = save_ok and (data.load(GAME, 1) == "pspgo")
local delete_ok = data.delete(GAME)

-- Per-volume write/list probe.  Returns the probe directory so the
-- cross-volume copy/move can reuse the file, or nil when the volume is absent.
local function probe_volume(root, present)
    if not present then
        return nil, false, false
    end
    local dir = root .. "/PSP/OPENLUA_COMPAT"
    files.delete(dir)
    files.mkdir(dir)                 -- returns no value; verify by stat
    if not files.exists(dir) then
        return nil, false, false
    end

    local wrote = false
    local fh = io.open(dir .. "/probe.txt", "wb")
    if fh then
        fh:write("openlua storage probe\n")
        fh:close()
        wrote = true
    end

    local listed = false
    if wrote then
        local entries = files.list(dir)
        listed = type(entries) == "table" and #entries >= 1
    end
    return dir, wrote, listed
end

local ms_dir, ms_write, ms_list = probe_volume("ms0:", ms0_present)
local ef_dir, ef_write, ef_list = probe_volume("ef0:", ef0_present)

-- Cross-volume copy/move needs both volumes and a source file.  pcall keeps a
-- read-only or full volume from stopping the demo.
local copy_ok = false
local move_ok = false
if ms_dir and ef_dir and ms_write then
    copy_ok = pcall(function()
        files.copy(ms_dir .. "/probe.txt", ef_dir .. "/copy.txt")
    end)
    move_ok = pcall(function()
        files.move(ms_dir .. "/probe.txt", ef_dir .. "/moved.txt")
    end)
end

if ms_dir then files.delete(ms_dir) end
if ef_dir then files.delete(ef_dir) end

while true do
    screen.clear(dark)
    draw.fillrect(0, 0, 480, 4, teal)
    draw.fillrect(0, 268, 480, 4, amber)

    if f then
        screen.print(18, 10, "storage / PSP Go compatibility", f, white)
        screen.print(18, 30, "model      " .. model, f,
                     model == "Go" and teal or muted)
        screen.print(18, 46, "cwd        " .. tostring(cwd):sub(1, 44), f,
                     muted)

        screen.print(18, 70, "ms0: M2       " .. yes(ms0_present), f,
                     tint(ms0_present))
        screen.print(230, 70, mb(ms0_info), f, muted)
        screen.print(18, 86, "ef0: internal " .. yes(ef0_present), f,
                     tint(ef0_present))
        screen.print(230, 86, mb(ef0_info), f, muted)
        screen.print(18, 102, "app0: app     " .. yes(app_present), f,
                     tint(app_present))

        screen.print(18, 130, "savedata save/load/delete", f, white)
        screen.print(250, 130, yes(save_ok) .. "/" .. yes(load_ok) .. "/" ..
                     yes(delete_ok), f,
                     tint(save_ok and load_ok and delete_ok))

        screen.print(18, 154, "ms0: write/list", f, white)
        screen.print(250, 154, yes(ms_write) .. "/" .. yes(ms_list), f,
                     tint(ms_write and ms_list))
        screen.print(18, 170, "ef0: write/list", f, white)
        screen.print(250, 170, yes(ef_write) .. "/" .. yes(ef_list), f,
                     tint(ef_write and ef_list))

        screen.print(18, 194, "cross copy/move", f, white)
        screen.print(250, 194, yes(copy_ok) .. "/" .. yes(move_ok), f,
                     tint(copy_ok and move_ok))

        screen.print(18, 224, "PSP Go: ef0: + ms0:; PPSSPP has no ef0:", f,
                     muted)
        screen.print(18, 240, "Cross copy/move need both volumes", f, muted)
    end

    screen.flip()
end
