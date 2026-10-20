-- Horizontal / vertical flip demo using one spritesheet.
--
-- Run from the repository root (or copy the assets next to the EBOOT):
--   assets/test_sheet.png  -> 4 frames of 64x64
--
-- image.fliph/flipv are draw-time state flags. Pass an explicit boolean to
-- set the state, or omit it to toggle. Remember to reset between frames.

local img = image.load("assets/test_sheet.png", 64, 64)
if not img then
    screen.clear(color.new(40, 0, 0))
    screen.flip()
    return
end

while true do
    screen.clear(color.new(10, 5, 80))

    -- Top row: normal, then horizontally flipped, then vertically flipped,
    -- then both.
    image.reset(img)
    image.blitsprite(img, 8, 8, 0)

    image.fliph(img, 1)
    image.blitsprite(img, 72, 8, 1)

    image.fliph(img, 0)
    image.flipv(img, 1)
    image.blitsprite(img, 136, 8, 2)

    image.fliph(img, 1)
    image.blitsprite(img, 200, 8, 3)

    -- Bottom: explicit source rectangles. The alpha argument is required in
    -- this form until #27 is fixed, even when it is fully opaque.
    image.reset(img)
    image.blit(img, 8, 80, 0, 0, 64, 64, 255)
    image.blit(img, 80, 80, 64, 0, 64, 64, 255)
    image.blit(img, 152, 80, 128, 0, 64, 64, 255)

    -- Whole sheet (module-call form; image userdata has no methods, see #27).
    image.blit(img, 8, 160, 0, 0, image.getw(img), image.geth(img), 255)

    screen.flip()
end
