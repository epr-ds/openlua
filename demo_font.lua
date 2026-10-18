-- OpenLua - Font rendering test
-- Demonstrates SFont bitmap fonts loaded from PNG tilemaps.
-- Characters map sequentially starting from space (ASCII 0x20 = index 0).

-- ================================================================
-- Setup: create a blank image surface for font rendering demo
-- ================================================================
local W, H = 320, 240                    -- PSP screen resolution
screen.clear(0xFF10106C)                 -- clear to dark blue-gray

-- Create a 100x60 test image where we'll paint with the font
local test_img = image.new(100, 60, 0x80202020)  -- translucent dark surface

-- ================================================================
-- Font loading (path to PNG spritesheet with cellW x cellH pixel glyphs)
-- A real SFont would have printable chars laid out as a grid:
--   - top-left = space (index 0)
--   - left-to-right, top-to-bottom = ! " # $ % & ... ~
-- ================================================================
local font = font.new("font.png", 8, 16)  -- cellW=8px, cellH=16px

if not font then
    -- Fallback: draw text without font using a simple ASCII art box
    screen.print(40, 80, "Font.lua not loaded")
else
    -- Draw text character by character onto our test surface using 
    -- the font:blt userdata method which takes (x, y, code).
    local sample_line = "Hello PSP!"
    
    for i = 1, #sample_line do
        local ch = string.byte(sample_line, i)
        if ch >= 32 then                     -- printable chars only 
            local code = ch - 32             -- index = char_code - space (0x20)
            font:blt(test_img,               -- destination surface
                       4 + (i - 1) * 8,       -- x: advance 8px per char  
                       4,                      -- y: start at top
                       code)                   -- glyph index (0=space, ...)
        end    
    end
        
    -- blit the test surface onto screen at center
    image.blit(test_img,
               (320 - 100) / 2,             -- x: center horizontally 
               60)                            -- y: vertical position on screen
    
    -- Draw colored lines below using draw primitives
    draw.line(40, 180, 280, 180, color.new(0xFF, 0xFF, 0xFF)
    draw.fillrect(40, 190, 60, 8,    color.new(0xFF, 0x20, 0x20)))
    -- Red stripe
    draw.fillrect(120, 190, 60, 8,    color.new(0x20, 0xFF, 0x20)))
    -- Green stripe
    draw.fillrect(200, 190, 60, 8,    color.new(0x20, 0x20, 0xFF))
    -- Blue stripe
    
    print("Font test complete!")
end

-- Present the final frame to the display  
screen.flip()
