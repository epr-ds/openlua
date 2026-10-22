-- OneLua power module tests (run by test/host/host_main).
-- The host power stubs report AC online and an unlocked console.  The
-- suspend/off stubs are inert no-ops: no real sleep or shutdown is possible on
-- the host, and the PPSSPP demo does not call them either.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

check(type(power) == "table", "power table registered")
check(type(power.plugged) == "function", "plugged registered")
check(type(power.suspend) == "function", "suspend registered")
check(type(power.lock) == "function", "lock registered")
check(type(power.unlock) == "function", "unlock registered")
check(type(power.tick) == "function", "tick registered")
check(type(power.off) == "function", "off registered")

check(power.plugged() == true, "plugged")
check(power.lock() == true, "lock returns success")
check(power.unlock() == true, "unlock returns success")
check(power.tick() == nil, "tick returns nil")
check(power.tick() == nil, "tick is repeatable")

-- These are no-op host stubs.  They return nil as documented, without
-- suspending or powering off the test machine.
check(power.suspend() == nil, "suspend returns nil")
check(power.off() == nil, "off returns nil")

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
