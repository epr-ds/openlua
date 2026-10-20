-- Sound host tests (run by test/host/host_main).

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

local ROOT = "soundwork"
files.delete(ROOT)
files.mkdir(ROOT)

local function write_file(path, body)
    local f = assert(io.open(path, "wb"))
    f:write(body)
    f:close()
end

local function le32(n)
    return string.char(n % 256, math.floor(n / 256) % 256,
                       math.floor(n / 65536) % 256,
                       math.floor(n / 16777216) % 256)
end

local function write_vma(path, samples)
    write_file(path, le32(samples) .. string.rep("\0", samples * 2))
end

local missing = ROOT .. "/missing.wav"
check(sound.load(missing) == nil, "missing file returns nil")

local short = ROOT .. "/short.wav"
write_file(short, "x")
check(sound.load(short) == nil, "short file returns nil")

local bad = ROOT .. "/bad.wav"
write_file(bad, "RIFF")
check(sound.load(bad) == nil, "short RIFF returns nil")

local p1 = ROOT .. "/one.vma"
local p2 = ROOT .. "/two.vma"
write_vma(p1, 4)
write_vma(p2, 4)

local h1 = sound.load(p1)
local h2 = sound.load(p2)
check(type(h1) == "number" and type(h2) == "number", "VMA fixtures load")
check(sound.rate(h1) == 22050, "VMA output rate")

check(pcall(sound.play, h1, 1), "explicit Lua channel 1 plays")
check(not pcall(sound.play, h2, 1), "busy explicit channel raises")
sound.stop(h1)
check(pcall(sound.play, h2, 1), "stop releases explicit channel")
check(pcall(sound.play, h1, 8), "explicit Lua channel 8 plays")
sound.stop(h1)
check(not pcall(sound.play, h1, 9), "Lua channel above 8 raises")
check(not pcall(sound.play, h1, -1), "Lua negative channel raises")
sound.stop(h2)
check(pcall(sound.play, h1, 0), "Lua channel 0 selects auto")
check(not pcall(sound.play, h2, 1), "auto channel is reserved")
sound.stop(h1)

sound.free(h2)
sound.free(h1)
files.delete(ROOT)

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
