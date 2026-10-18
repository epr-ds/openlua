-- Phase 1 boot smoke test: clears the screen blue forever.
-- If you see blue, the interpreter booted and ran script.lua successfully.
local blue = color.new(0, 0, 200)

screen.clear(blue)
while true do
    screen.flip()
end
