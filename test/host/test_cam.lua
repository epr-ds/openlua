-- OneLua cam module host tests.
-- No camera is attached to the host, so this verifies the documented
-- absent-hardware contract, argument handling and safe no-op setters. The
-- value converters are covered by test_cam.c; the active driver path is
-- PPSSPP-verified with the emulator's dummy camera frames.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

check(type(cam) == "table", "cam table registered")
for _, name in ipairs({
    "init", "begin", "state", "available", "render", "shot", "toimage",
    "direction", "brightness", "contrast", "sharpness", "saturation",
    "zoom", "evlevel", "effect", "autoreverse", "mirror", "term",
}) do
    check(type(cam[name]) == "function", "cam." .. name .. " is a function")
end

-- Fresh state and absent camera.
check(cam.state() == false, "state is false before init")
check(cam.available() == false, "available is false before init")
check(cam.init() == 0, "init reports no camera")
check(cam.state() == false, "failed init leaves state false")
check(cam.available() == false, "failed init leaves available false")
check(cam.begin() == 0, "begin fails without init")

-- All read accessors are safe and report the documented error value.
check(cam.direction() == 0, "direction is 0 without a camera")
check(cam.brightness() == -1, "brightness is -1 without a camera")
check(cam.contrast() == -1, "contrast is -1 without a camera")
check(cam.sharpness() == -1, "sharpness is -1 without a camera")
check(cam.saturation() == -1, "saturation is -1 without a camera")
check(cam.zoom() == -1, "zoom is -1 without a camera")
check(cam.evlevel() == -1, "evlevel is -1 without a camera")
check(cam.effect() == -1, "effect is -1 without a camera")
check(cam.mirror() == -1, "mirror is -1 without a camera")
check(cam.autoreverse() == 0, "autoreverse is 0 without a camera")

-- Rendering and image handoff do nothing rather than raising.
check(select("#", cam.render(0, 0)) == 0, "render returns nothing")
check(select("#", cam.render(10, 20, 160, 120)) == 0,
      "scaled render returns nothing")
check(cam.shot("out/never-written.png") == 0, "shot reports failure")
check(cam.toimage() == nil, "toimage returns nil")
check(io.open("out/never-written.png", "rb") == nil,
      "failed shot does not create a file")

-- Setters accept the documented ranges and clamp without hardware.
for _, value in ipairs({ 0, 64, 128, 255, -10, 999 }) do
    check(select("#", cam.brightness(value)) == 0,
          "brightness setter returns nothing")
    check(select("#", cam.contrast(value)) == 0,
          "contrast setter returns nothing")
    check(select("#", cam.sharpness(value)) == 0,
          "sharpness setter returns nothing")
    check(select("#", cam.saturation(value)) == 0,
          "saturation setter returns nothing")
end
for _, value in ipairs({ 1, 2, 7, 0, 99 }) do
    check(select("#", cam.effect(value)) == 0, "effect setter returns nothing")
end
for _, value in ipairs({ 1, 8, 17, 0, 99 }) do
    check(select("#", cam.evlevel(value)) == 0,
          "evlevel setter returns nothing")
end
for _, value in ipairs({ 1, 3, 5, 0, 99 }) do
    check(select("#", cam.zoom(value)) == 0, "zoom setter returns nothing")
end
for _, value in ipairs({ 0, 1, 2 }) do
    check(select("#", cam.autoreverse(value)) == 0,
          "autoreverse setter returns nothing")
    check(select("#", cam.mirror(value)) == 0,
          "mirror setter returns nothing")
end

-- term is idempotent and always restores the stopped state.
check(cam.term() == 1, "term succeeds when idle")
check(cam.state() == false, "term leaves state false")
check(cam.term() == 1, "term can be called twice")

-- Argument errors are still Lua errors, not silent success.
check(pcall(cam.shot) == false, "shot with no path raises")
check(pcall(cam.brightness, "x") == false,
      "brightness with a non-number raises")

print(string.format("cam Lua: %d assertions, %d failure(s)", ok, fails))
if fails > 0 then error("cam tests failed") end
