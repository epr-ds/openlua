-- Minimal boot smoke test.
-- Copy this file to script.lua (or the EBOOT app directory) and run it.
-- Expected result: a dark background with a green bar, white outline and red line.
-- If nothing appears, check the boot log and the rendering issues linked in
-- docs/ROADMAP.md (#19, #20).

while true do
    screen.clear(color.new(16, 24, 48))
    draw.fillrect(80, 56, 320, 64, color.new(0, 180, 90))
    draw.rect(80, 56, 320, 64, color.new(255, 255, 255))
    draw.line(80, 144, 400, 144, color.new(255, 80, 80))
    screen.flip()
end
