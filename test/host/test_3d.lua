-- OneLua 3D module API tests (run by test/host/host_main).
-- The GE draw is a no-op on the host; parsing, handles, transforms and error
-- paths are all exercised here.  PPSSPP verifies the actual rendering.

local root = os.getenv("OPENLUA_ROOT") or ".."
local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

check(type(amg) == "table", "amg table registered")
check(type(cam3d) == "table", "cam3d table registered")
check(type(model3d) == "table", "model3d table registered")
check(type(texture3d) == "table", "texture3d table registered")

check(__8888 == 0 and __5551 == 2, "texture quality constants")
check(__RAM == 0 and __VRAM == 1, "texture location constants")
check(__POINTS == 0 and __LINES == 1 and __TRIANGLES == 2,
      "render style constants")
check(__FRONT == 0 and __TRANS == 1 and __ALL == 2, "draw type constants")
check(__NEAR == 0 and __LINEAR == 1, "filter constants")

-- Scene control.
amg.init()
amg.quality(__8888)
amg.perspective(60.0)
amg.renderstyle(__TRIANGLES)
check(not pcall(amg.perspective, 5), "FOV below range rejected")
check(not pcall(amg.perspective, 200), "FOV above range rejected")
amg.fog(2, 20, 0xFF000000)
check(not pcall(amg.fog, 20, 2, 0), "reversed fog range rejected")
amg.fog()
amg.begin()
amg.mode2d(1)
amg.mode2d(0)
amg.update()
amg.finish()

-- Camera handles.
local cam = cam3d.new()
check(type(cam) == "number" and cam > 0, "cam3d.new returns a handle")
cam3d.position(cam, {0, 0, 6})
cam3d.rotation(cam, {0, 0, 0})
cam3d.eye(cam, {x = 0, y = 0, z = 0})
cam3d.up(cam, {0, 1, 0})
cam3d.set(cam)
check(not pcall(cam3d.position, cam, 1), "camera position requires a table")
cam3d.free(cam)
check(not pcall(cam3d.set, cam), "freed camera handle rejected")

-- Texture handles and state.
local tex = texture3d.new(16, 16, __RAM)
check(type(tex) == "number" and tex > 0, "texture3d.new returns a handle")
texture3d.quality(tex, __8888)
texture3d.position(tex, 12, 34)
texture3d.scaling(tex, 1.5, 0.5)
texture3d.setframes(tex, 2)
texture3d.frame(tex, 1)
texture3d.frame(tex, 99)
texture3d.trans(tex, 0.25, 0.75)
texture3d.draw(tex)
check(not pcall(texture3d.new, 0, 16, __RAM), "zero texture rejected")
check(not pcall(texture3d.new, 513, 16, __RAM), "oversize texture rejected")

local checker = texture3d.load(root .. "/assets/3d/checker.png", __RAM)
check(checker ~= nil, "checker texture loads")
check(not pcall(texture3d.load, root .. "/assets/3d/missing.png", __RAM),
      "missing texture raises")

-- Model loading and object transforms.
local model = model3d.load(root .. "/assets/3d/cube.obj")
check(type(model) == "number" and model > 0, "model3d.load returns a handle")
check(model3d.countobj(model) == 1, "cube has one object")

local r = model3d.getrotation(model, 0)
check(r.x == 0 and r[2] == 0 and r.z == 0, "default rotation")
local p = model3d.getposition(model, 0)
check(p.x == 0 and p[1] == 0 and p.z == 0, "default position")
local s = model3d.getscaling(model, 0)
check(s.x == 1 and s[2] == 1 and s.z == 1, "default scaling")

model3d.position(model, 0, {0, 0, 0})
model3d.rotation(model, 0, {10, 20, 30})
model3d.scaling(model, 0, {2, 2, 2})
r = model3d.getrotation(model, 0)
p = model3d.getposition(model, 0)
s = model3d.getscaling(model, 0)
check(r.x == 10 and r.y == 20 and r.z == 30, "rotation round trip")
check(p.x == 0 and p.y == 0 and p.z == 0, "position round trip")
check(s.x == 2 and s.y == 2 and s.z == 2, "scaling round trip")

texture3d.setcustom(model, 0, 0, tex)
texture3d.filter(model, __LINEAR)
texture3d.filter(model, 0, __NEAR)
model3d.render(model, __ALL)
model3d.render(model, __FRONT, 0)
model3d.render(model, __TRANS, -1)
check(not pcall(model3d.render, model, __ALL, 2), "bad object rejected")
check(not pcall(texture3d.setcustom, model, 0, 0, 0), "bad custom texture rejected")

model3d.free(model)
check(not pcall(model3d.countobj, model), "freed model handle rejected")
check(not pcall(texture3d.load, root .. "/assets/3d/cube.obj", __RAM),
      "non-image texture raises")

texture3d.finish()
check(not pcall(texture3d.draw, tex), "texture3d.finish releases handles")

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
