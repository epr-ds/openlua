-- OneLua hardware module tests (run by test/host/host_main).
-- The host stubs model a PPSSPP-like console: Emulator model, America region,
-- firmware 6.60, sound-registry volume 15, speakers selected, no headphones,
-- and writable in-memory LEDs.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

check(type(hw) == "table", "hw table registered")

local names = {
    "baryon", "tachyon", "pommel", "fuseid", "fusecfg", "kirk", "spock",
    "nandseed", "ofwinitial", "board", "gen", "region", "getmodel",
    "firmware", "pandora", "nand", "volume", "mute", "headphone",
    "audioout", "led"
}
for _, name in ipairs(names) do
    check(type(hw[name]) == "function", name .. " registered")
end

-- Restricted identity fields are predictable placeholders, never crashes.
check(hw.baryon() == "UNK", "baryon fallback")
check(hw.tachyon() == "UNK", "tachyon fallback")
check(hw.pommel() == 0, "pommel fallback")
check(hw.fuseid() == "UNK", "fuseid fallback")
check(hw.fusecfg() == "UNK", "fusecfg fallback")
check(hw.kirk() == "UNK", "kirk fallback")
check(hw.spock() == "UNK", "spock fallback")
check(hw.nandseed() == "UNK", "nandseed fallback")
check(hw.ofwinitial() == "UNK", "ofwinitial fallback")
check(hw.board() == "UNK", "board fallback")
check(hw.gen() == "UNK", "gen fallback")
check(hw.nand() == "UNK", "nand fallback")
check(hw.pandora() == false, "pandora fallback")

check(hw.region() == "America", "region host mock")
check(hw.getmodel() == "Emulator", "model host mock")
check(hw.firmware() == "6.60", "firmware host mock")

-- System sound registry: volume is clamped to 1..30.
check(hw.volume() == 15, "volume host default")
check(hw.volume(20) == 20, "volume setter returns level")
check(hw.volume() == 20, "volume setter updates registry")
check(hw.volume(0) == 1, "volume zero clamps to 1")
check(hw.volume() == 1, "volume zero stored as 1")
check(hw.volume(99) == 30, "volume over maximum clamps to 30")
check(hw.volume() == 30, "volume maximum stored as 30")
check(hw.volume(15) == 15, "volume reset")

-- Mute getter returns 0/1; setter returns nil and clamps to a boolean state.
check(hw.mute() == 0, "mute host default")
check(hw.mute(1) == nil, "mute setter returns nil")
check(hw.mute() == 1, "mute enabled")
check(hw.mute(99) == nil, "mute over one accepted")
check(hw.mute() == 1, "mute over one stays enabled")
check(hw.mute(0) == nil, "mute disable returns nil")
check(hw.mute() == 0, "mute disabled")

-- Headphone detection and audio routing.
check(hw.headphone() == 0, "headphone host mock")
check(hw.audioout() == 1, "audioout host default speakers")
check(hw.audioout(0) == 0, "audioout headphones succeeds")
check(hw.audioout() == 0, "audioout switched to headphones")
check(hw.audioout(1) == 0, "audioout speakers succeeds")
check(hw.audioout() == 1, "audioout switched to speakers")
check(hw.audioout(5) == 0, "audioout clamps nonzero to speakers")
check(hw.audioout() == 1, "audioout clamp stored as speakers")

-- LED calls validate the OneLua indices 0..3.  Host mocks accept them; the
-- PSP build returns -1 because sceSysconCtrlLED is kernel-only.
check(hw.led(0, 1) == 0, "MS LED valid")
check(hw.led(1, 0) == 0, "WLAN LED valid")
check(hw.led(2, 99) == 0, "LED state clamps to on")
check(hw.led(3, 0) == 0, "BT LED valid")
check(hw.led(-1, 1) == -1, "negative LED rejected")
check(hw.led(4, 1) == -1, "over-range LED rejected")

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
