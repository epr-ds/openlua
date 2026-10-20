-- OpenLua - native sound demo (WAV / VMA).
--
--   sound.load(path)          -> handle or nil
--   sound.play(h [, channel]) -> channel 1..8, omit for auto
--   sound.loop(h)             -> toggle loop
--   sound.vol(h, 0..100)
--   sound.pause(h [, mode])   -> -1 toggle / 0 resume / 1 pause
--   sound.stop(h) / sound.free(h)
--
-- Audio is pumped automatically by screen.flip(), once per frame.
-- Asset: assets/beep.wav (regenerate with python3 tools/gen_tone.py).

local beep = sound.load("assets/beep.wav")
if not beep then
    screen.clear(color.new(0x20, 0x20, 0x30))
    for _ = 1, 60 do
        screen.flip()
    end
    return
end

sound.loop(beep)
sound.vol(beep, 80)
sound.play(beep)

local x = 20
for i = 1, 1200 do
    screen.clear(color.new(0x10, 0x10, 0x18))
    local w = 4 + math.floor(2 * math.abs(math.sin(i / 12)))
    draw.fillrect(x, 120 - w, w * 2, w * 2, color.new(255, 255, 255))
    if i % 120 == 1 then
        draw.fillrect(190, 60, 60, 40, color.new(0x40, 0x20, 0x20))
    end
    x = x + 2
    if x > 340 then x = 20 end
    screen.flip()
end

sound.stop(beep)
sound.free(beep)
