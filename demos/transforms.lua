-- Sprite transform demo: rotation, scale and tint.
--
-- Transforms are per-surface state, so reset what you change between draws.
-- Exercises rotation, scaling and tint through the public API.
--
-- Asset: assets/test_sheet.png (4 frames of 64x64).

local img = image.load("assets/test_sheet.png", 64, 64)
if not img then
    screen.clear(color.new(40, 0, 0))
    screen.flip()
    return
end

local angle = 0

while true do
    screen.clear(color.new(8, 10, 24))

    -- Normal frame.
    image.reset(img)
    image.pivot(img, 32, 32)
    image.scale(img, 1.0, 1.0)
    image.tint(img, 0xFFFFFFFF)
    image.blitsprite(img, 40, 100, 0)

    -- Rotating around the frame center.
    image.pivot(img, 32, 32)
    image.rotate(angle)
    image.blitsprite(img, 150, 100, 1)

    -- Scaled 1.5x.
    image.rotate(0)
    image.scale(img, 1.5, 1.5)
    image.blitsprite(img, 300, 90, 2)

    -- Tinted warm orange.
    image.scale(img, 1.0, 1.0)
    image.tint(img, color.new(255, 120, 40))
    image.blitsprite(img, 390, 100, 3)

    screen.flip()
    angle = (angle + 2) % 360
end
