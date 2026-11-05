-- OpenLua - OneLua peripheral module demo (umd, usb, sio, ir).
--
--   umd.present() / umd.init() / umd.type() / umd.gameid() / umd.info()
--   umd.size()        -- mounted-tree size in GB as a string
--   usb.mstick()      -- expose the memory stick over USB
--   usb.isactive() / usb.isconnected()
--   sio.init() / ir.init()
--
-- Everything is safe without hardware.  On the host and in most emulators the
-- UMD is absent, USB stays inactive, and the user-mode SIO/IR backends report
-- "not available"; the panel shows those results instead of failing.
--
-- The destructive OneLua calls (umd.dumpiso, umd.launch) are not exposed.

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

local function onoff(v)
    if v then
        return "YES"
    end
    return "NO"
end

local function text(v)
    if v == nil or v == "" then
        return "-"
    end
    return tostring(v)
end

local present = umd.present()
local mounted = umd.init()
local umdtype = umd.type()
local gameid = umd.gameid()
local info = umd.info()
local size = umd.size()

-- The camera and USB storage use the same port; this demo only claims USB.
local sio_ok = sio.init()
local ir_ok = ir.init()

usb.mstick()
local usb_active = usb.isactive()
local usb_connected = usb.isconnected()

local title = "-"
if info and info.TITLE then
    title = tostring(info.TITLE)
end

while true do
    screen.clear(dark)
    draw.fillrect(0, 0, 480, 4, teal)
    draw.fillrect(0, 268, 480, 4, amber)

    if f then
        screen.print(18, 12, "peripherals / UMD - USB - SIO - IR", f, white)

        screen.print(18, 44, "umd.present   " .. onoff(present), f, teal)
        screen.print(18, 64, "umd.init      " .. onoff(mounted), f, teal)
        screen.print(18, 84, "umd.type      " .. text(umdtype), f, muted)
        screen.print(18, 104, "umd.gameid    " .. text(gameid), f, muted)
        screen.print(18, 124, "umd.size      " .. text(size) .. " GB", f, muted)
        screen.print(18, 144, "umd.title     " .. text(title), f, muted)

        screen.print(18, 176, "usb.isactive    " .. onoff(usb_active), f, teal)
        screen.print(18, 196, "usb.isconnected " .. onoff(usb_connected), f, teal)
        screen.print(18, 216, "usb mode        memory stick", f, muted)

        screen.print(258, 176, "sio.init   " .. tostring(sio_ok), f,
                     sio_ok == 1 and teal or red)
        screen.print(258, 196, "sio.available " .. tostring(sio.available()), f, muted)
        screen.print(258, 216, "ir.init    " .. tostring(ir_ok), f,
                     ir_ok == 1 and teal or red)
        screen.print(258, 236, "ir.state   " .. onoff(ir.state()), f, muted)

        screen.print(18, 250, "UMD/USB emulated; SIO/IR need kernel drivers", f, muted)
    end

    screen.flip()
end
