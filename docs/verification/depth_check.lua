-- Depth-order verification: the red square has a higher z-index but is drawn
-- first. The green square is drawn second with a lower z-index; in the overlap
-- the red square must remain visible.
local red = image.new(80, 80, color.new(255, 0, 0))
local green = image.new(80, 80, color.new(0, 255, 0))
local blue = image.new(80, 80, color.new(0, 0, 255))
while true do
    screen.clear(color.new(8, 10, 24))
    image.zindex(red, 10)
    image.zindex(green, 5)
    image.zindex(blue, 20)
    image.blit(red, 60, 60, 0, 0, 80, 80, 255)
    image.blit(green, 100, 100, 0, 0, 80, 80, 255)
    image.blit(blue, 300, 60, 0, 0, 80, 80, 255)
    screen.flip()
end
