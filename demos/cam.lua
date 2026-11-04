-- OpenLua - OneLua cam module demo (GO! CAM USB camera).
--
--   cam.init()                 -- load the USB camera driver
--   cam.begin()                -- configure 480x272 video
--   cam.state()                -- driver loaded?
--   cam.available()            -- camera connected?
--   cam.render(x, y [, w, h])  -- draw the current frame
--   cam.toimage()              -- copy the current frame as an ordinary image
--   cam.shot(path)             -- save a PNG snapshot
--
-- The camera is optional.  Every call is safe when the accessory is absent:
-- init/begin return 0, state/available are false, getters return -1 and
-- render/toimage do nothing.  PPSSPP supplies a dummy camera frame, so this
-- demo can exercise the active path without real hardware.

local f = font.new("assets/font.png", 8, 16)
if f then
    screen.font(f)
end

local white = color.new(238, 240, 246)
local muted = color.new(120, 132, 154)
local teal  = color.new(48, 180, 160)
local amber = color.new(235, 180, 70)
local dark  = color.new(10, 14, 26)

local inited = cam.init()
local available = cam.available()
local began = 0
local snapshot = nil
local shot = 0

if inited == 1 then
    began = cam.begin()
end

if began == 1 then
    cam.brightness(128)
    cam.contrast(64)
    cam.sharpness(0)
    cam.saturation(125)
    cam.zoom(1)
    cam.evlevel(8)
    cam.effect(1)
    cam.autoreverse(1)
    cam.mirror(0)

    snapshot = cam.toimage()
    shot = cam.shot("cam_snapshot.png")
end

local function val(v)
    if v == -1 then
        return "n/a"
    end
    return tostring(v)
end

local function onoff(v)
    if v then
        return "YES"
    end
    return "NO"
end

while true do
    screen.clear(dark)
    draw.fillrect(0, 0, 480, 4, teal)
    draw.fillrect(0, 268, 480, 4, amber)

    if f then
        screen.print(18, 12, "cam / GO! CAM USB camera", f, white)
        screen.print(18, 38, "init      " .. onoff(inited == 1), f, teal)
        screen.print(18, 58, "available " .. onoff(available), f, teal)
        screen.print(18, 78, "begin     " .. onoff(began == 1), f, teal)
        screen.print(18, 98, "state     " .. onoff(cam.state()), f, muted)
        screen.print(18, 118, "direction " .. val(cam.direction()), f, muted)

        screen.print(258, 38, "brightness " .. val(cam.brightness()), f, muted)
        screen.print(258, 58, "contrast   " .. val(cam.contrast()), f, muted)
        screen.print(258, 78, "saturation " .. val(cam.saturation()), f, muted)
        screen.print(258, 98, "zoom       " .. val(cam.zoom()), f, muted)
        screen.print(258, 118, "effect     " .. val(cam.effect()), f, muted)

        screen.print(264, 142, "live frame", f, teal)
        screen.print(264, 170, "toimage " .. (snapshot and "OK" or "nil"), f, muted)
        screen.print(264, 190, "shot    " .. onoff(shot == 1), f, muted)
        screen.print(264, 218, "PPSSPP provides", f, muted)
        screen.print(264, 238, "a dummy camera", f, muted)
    end

    draw.rect(18, 138, 236, 130, teal)
    cam.render(20, 140, 232, 126)

    screen.flip()
end
