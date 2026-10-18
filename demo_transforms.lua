-- OpenLua - comprehensive transform demo
-- Exercises: scale, pivot/anchor, rotation, z-index, tint/color modulate
--
-- Usage: ./emulator EBOOT.PBP
-- See script output in PPSSPP log or debug console.

local img = image.load("demo_bidirectional.png")   -- or any spritesheet/PNG

local cx, cy = 240, 136                              -- screen center
local t = 0                                           -- time accumulator
local angle = 0                                       -- rotation angle
local dir = 1                                         -- movement direction
local frame = 0                                       -- animation frame
local fps_target = 60                                 -- target framerate
local frame_time = 1 / fps_target                     -- seconds per frame
local delta = 0

function main()
    while true do
        t = t + delta                                     -- accumulate wall-clock time
        angle = angle + 1.5                               -- degrees per frame (60fps)
        frame = (frame + 1) % 6                           -- cycle through frames

        -- ── Draw layered transforms (z-index ordering) ───────────
        --
        -- Layer 1: Background grid (z=0, full-screen)
        image.clear(0xFF002040)                            -- dark navy
        image.blit(image.new(1, 1), 0, 0, 480, 272,       -- tiny filled quad
                     1, 1, 1, 1, 1)                       -- full-screen blit
        image.flip_scene()                                 -- flush to screen

        -- Layer 2: Scale demo (z=50, scaled up 2x)
        image.blit(img, 100, 100, frame * 64, 0, 64, 64,
                     180)                                 -- semi-transparent
        image.scale(img, 2.0, 2.0)                       -- scale up 2x
        image.zindex(45)                                 -- behind main sprite
        image.flip_scene()

        -- Layer 3: Main rotating sprite (z=100, center pivot)
        image.pivot(32, 32)                              -- rotate around center (32,32)
        image.rotate(angle % 360)                        -- smooth rotation
        image.zindex(100)                                -- foreground
        -- image.scale(img, 1.0, 1.0)                   -- restore scale
        -- image.blit(img, cx - 32, cy - 32, frame * 64, 0, 64, 64, 255)
        image.flip_scene()

        -- Layer 4: Tint demo (z=150, colored variant)
        image.pivot(0, 0)                                -- reset pivot to top-left
        image.rotate(0)                                  -- no rotation for tint test
        image.tint(0xFFFF8800)                           -- warm orange overlay
        image.scale(img, 1.0, 1.0)                       -- restore scale
        image.zindex(150)
        -- image.blit(img, 350, 80, frame * 64, 0, 64, 64, 255)
        image.flip_scene()

        -- Layer 5: Pivot examples (z=200, rotating around different anchors)
        draw_pivot_example(60, dy + 160, "origin", {
            pivotX = 0, pivotY = 0
        })
        draw_pivot_example(140, dy + 160, "center", {
            pivotX = 32, pivotY = 32
        })
        draw_pivot_example(220, dy + 160, "corn er", {
            pivotX = 64, pivotY = 64
        })

        -- Wait for target framerate (60fps ≈ 16.67ms)
        local wait_ms = math.max(0, math.floor((frame_time - delta) * 1000))
        if wait_ms > 0 then
            image.wait_vblank()                          -- PSP vblank wait
        end

        -- Calculate delta time for next frame
        delta = 0                                         -- reset (actual timing via vblank)
    end
end

-- Helper: draw a spinning sprite at pivot examples
function draw_pivot_example(x, y, label, opts)
    local px = opts.pivotX or 32
    local py = opts.pivotY or 32

    image.pivot(px, py)
    image.rotate(angle % 360)
    image.scale(img, 0.8, 0.8)
    image.zindex(200)

    -- Use tint for visual distinction (subtle color shift per rotation)
    local hue = (angle * 0.5 % 360) / 360.0
    local cr = math.floor(hue * 255)
    local cg = math.floor((1 - math.abs(2 * hue - 1)) * 255)
    image.tint(0xFF400000 + cr * 0x101 + cg * 0x0101)
    -- Draw sprite centered at (x, y) with pivot at (px, py) relative to top-left
    image.blit(img, x - px, y - py, frame * 64, 0, 64, 64, 255)

    -- Reset state
    image.scale(img, 1.0, 1.0)
    image.rotate(0)
    image.pivot(32, 32)
    image.tint(0xFFFFFFFF)
end

main()