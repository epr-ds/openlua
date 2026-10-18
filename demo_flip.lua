-- One spritesheet used for both walk directions.
-- No need for separate left/right spritesheets.
-- Frame index order is preserved — frame 0 is the same frame, just mirrored.

img = image.load("/mnt/ps0/PSP/GAME/OpenLua/test_sheet.png", 64, 64)
facing_right = true

while true do
    screen.clear(color.new(10, 5, 80))  -- dark blue bg

    -- Top row: all 4 frames displayed with different flips
    image.blitsprite(img,    8,   8, 0)      -- frame 0 (red square): normal
    image.blitsprite(img,   72,   8, 1)      -- frame 1 (green circle): normal
    image.fliph(img, 1)                         -- flipX = true
    image.blitsprite(img,  136,   8, 2)      -- frame 2 (blue triangle): flipped
    image.fliph(img, 0)                         -- reset flip
    image.flipv(img, 1)                         -- flipY = true
    image.blitsprite(img,  200,   8, 3)      -- frame 3 (yellow X): flipped both ways
    image.flipv(img, 0)                         -- reset

    -- Middle row: frame grid sub-rect via blit (same sheet)
    image.blit(img, 8, 72, 64,   0, 64, 64)   -- explicit sub-rect (frame 0)
    image.blit(img, 8, 72 + 64, 128, 0, 64, 64) -- explicit sub-rect (frame 1)
    image.blit(img, 8, 72 + 128, 192, 0, 64, 64) -- explicit sub-rect (frame 2)

    -- Bottom row: the real use case — same sheet for both directions
    local x = 8
    image.blit(img, x, 72 + 200, 0, 0, img:getw(), 64)  -- whole sheet at bottom

    screen.flip()
end
