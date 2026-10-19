-- OpenLua - sound demo (script.lua)
--
-- Demonstrates the native PSP audio module (OneLua-style sound table):
--   sound.load(path)          -> handle (0 if the file can't be decoded)
--   sound.play(h [, channel]) -> start (channel 1..8, omitted = auto)
--   sound.loop(h)             -> toggle loop (returns new state)
--   sound.vol(h, pct)         -> set volume 0..100
--   sound.pause(h [, mode])   -> -1 toggle / 0 resume / 1 pause
--   sound.stop(h)             -> stop & mark endstream
--   (audio is pumped automatically by screen.flip — no manual call needed)
--   sound.playing(h) / endstream(h) / looping(h) / rate(h)
--
-- Pumping is automatic: screen.flip() feeds the audio each frame.
--
-- Requires "beep.wav" (16-bit PCM, any sample rate, mono or stereo)
-- next to this script, or generate one with:  python3 tools/gen_tone.py

local beep = sound.load("beep.wav")
if not beep then
    -- without a tone file we just show a status splash for 60 frames
    screen.clear(color.new(0x20, 0x20, 0x30))
    local n = 0
    while n < 60 do
        n = n + 1
        screen.flip()
    end
    return
end

-- start the tone on the auto channel, 80% volume, looping
sound.loop(beep)
sound.vol(beep, 80)
sound.play(beep)

-- simple visual metronome: white dot pulses on every flip
local x = 20
for i = 1, 1200 do            -- ~20 s at 60 fps: let the tone keep looping
    screen.clear(color.new(0x10, 0x10, 0x18))
    local w = 4 + math.floor(2 * math.abs(math.sin(i / 12)))
    draw.fillrect(x, 120 - w, w * 2, w * 2, color.new(255, 255, 255))
    if i % 120 == 1 then
        draw.fillrect(190, 60, 60, 40, color.new(0x40, 0x20, 0x20))
    end
    x = x + 2
    if x > 340 then x = 20 end
    screen.flip()             -- <- pumps the audio each frame
end

-- stop when done
sound.stop(beep)
sound.free(beep)
