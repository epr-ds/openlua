-- 3D demo: rotating textured OBJ cube through the PSP GE.
--
-- This is an original checker texture and cube (see tools/gen_3d_assets.py).
-- The camera looks at the origin from +Z; the cube spins around Y and tilts
-- slightly on X so the texture and depth buffer are both visible.
--
-- State order matters:
--   1. screen.clear() only changes the next clear colour.
--   2. amg.begin() enables 3D depth state and starts the scene.
--   3. model3d.render() submits triangles.
--   4. amg.mode2d(1) switches to depth-disabled 2D for the HUD.
--   5. amg.update() presents the frame (OpenLua's 3D flip).
--
-- Assets: assets/3d/cube.obj, cube.mtl and checker.png.

local model = model3d.load("assets/3d/cube.obj")
if not model then
    screen.clear(color.new(40, 0, 0))
    screen.flip()
    return
end

local cam = cam3d.new()
cam3d.position(cam, {0, 1.0, 6.0})
cam3d.eye(cam, {0, 0, 0})
cam3d.up(cam, {0, 1, 0})
cam3d.set(cam)

amg.init()
amg.perspective(55)
amg.renderstyle(__TRIANGLES)

local font = font.new("assets/font.png", 8, 16)
if font then
    screen.font(font)
end

local angle = 0

while true do
    screen.clear(color.new(8, 10, 24))

    amg.begin()
    model3d.position(model, 0, {0, 0, 0})
    model3d.scaling(model, 0, {1.35, 1.35, 1.35})
    model3d.rotation(model, 0, {18, angle, 0})
    model3d.render(model, __ALL)

    -- Explicitly leave 3D state before drawing 2D text.
    amg.mode2d(1)
    if font then
        screen.print(8, 8, "OpenLua 3D: rotated textured OBJ cube")
        screen.print(8, 30, "checker.png + cube.mtl + cube.obj")
        screen.print(8, 52, string.format("Y rotation %03d deg", math.floor(angle)))
    end

    amg.update()
    angle = (angle + 1.5) % 360
end
