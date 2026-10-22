-- OneLua battery module tests (run by test/host/host_main).
-- The host battery mocks report a healthy 76% pack at 4.02 V, 31 C, with
-- 850/1200 mAh remaining and no charger connected.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

check(type(batt) == "table", "batt table registered")
check(batt.exists() == true, "exists")
check(batt.charging() == false, "charging")
check(batt.low() == false, "low")
check(batt.lifepercent() == 76, "lifepercent")
check(batt.lifetimemin() == 95, "lifetimemin")
check(batt.lifetime() == "1:35", "lifetime format")
check(batt.temp() == 31, "temp")
check(batt.volt() == "4.02", "volt format")
check(batt.remaincap() == "850", "remaincap format")
check(batt.fullcap() == "1200", "fullcap format")

-- Read-only values are stable across calls.
check(batt.lifepercent() == batt.lifepercent(), "lifepercent stable")
check(batt.lifetime() == batt.lifetime(), "lifetime stable")

-- No user-mode serial/mode API: documented fallbacks.
check(batt.serial() == "-", "serial fallback")
check(batt.mode() == "-", "mode fallback")

-- Mutating calls are inert and report failure.
check(batt.tonormal() == 0, "tonormal unsupported")
check(batt.topandora() == 0, "topandora unsupported")
check(batt.toautoboot() == 0, "toautoboot unsupported")
check(batt.backupbat("ms0:/eeprom.bin") == 0, "backupbat unsupported")
check(batt.restorebat("ms0:/eeprom.bin") == 0, "restorebat unsupported")

-- The OneLua signatures still require a path, but the path is never opened.
check(not pcall(batt.backupbat), "backupbat needs a path")
check(not pcall(batt.restorebat), "restorebat needs a path")

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
