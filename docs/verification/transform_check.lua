-- Static visual verification for issue #20: rotation, scale, tint and flips.
local img = image.load("assets/test_sheet.png", 64, 64)
if not img then
    screen.clear(color.new(40, 0, 0)); screen.flip(); return
end

while true do
    screen.clear(color.new(8, 10, 24))

    -- Four copies of the asymmetric triangle at 0, 30, 60 and 90 degrees.
    for i = 0, 3 do
        image.reset(img)
        image.pivot(img, 32, 32)
        image.rotate(img, i * 30)
        image.scale(img, 1.0, 1.0)
        image.tint(img, 0xFFFFFFFF)
        image.blitsprite(img, 40 + i * 100, 70, 2)
    end

    -- Rotated triangle at 45 degrees with warm tint.
    image.reset(img)
    image.pivot(img, 32, 32)
    image.rotate(img, 45)
    image.scale(img, 1.5, 1.5)
    image.tint(img, color.new(255, 150, 30))
    image.blitsprite(img, 40, 160, 2)

    -- Scaled circle (2x) and flipped triangle.
    image.reset(img)
    image.pivot(img, 32, 32)
    image.rotate(img, 0)
    image.scale(img, 2.0, 2.0)
    image.tint(img, 0xFFFFFFFF)
    image.blitsprite(img, 190, 160, 1)

    image.reset(img)
    image.scale(img, 1.0, 1.0)
    image.fliph(img, 1)
    image.flipv(img, 1)
    image.blitsprite(img, 340, 160, 2)

    screen.flip()
end
