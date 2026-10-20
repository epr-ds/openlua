-- Timer host tests (run by test/host/host_main).

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

local function spin(ms)
    local start = os.clock()
    while (os.clock() - start) * 1000 < ms do end
end

local t = timers.new()
check(type(t) == "userdata", "new returns userdata")
check(t:time() >= 0, "new time non-negative")
check(t:gettime() == t:time(), "gettime aliases time")

local started = t:start()
check(started == 0, "start resets to zero")
spin(20)
local running = t:time()
check(running >= 10, "time advances (got " .. running .. ")")

local stopped = t:stoptime()
check(stopped >= running, "stop >= last time")
spin(15)
check(t:stoptime() == stopped, "stoptime stable")
check(t:time() == stopped, "time frozen while stopped")

check(t:start() == 0, "restart resets to zero")
spin(20)
check(t:time() >= 10, "time advances after restart")

local offset = timers.new(1000)
check(offset:time() >= 1000, "new(1000) applies initial value")
spin(10)
check(offset:time() > 1000, "new(1000) keeps advancing")
local offset_stop = offset:stoptime()
check(offset_stop >= 1000, "new(1000) stop keeps offset")
spin(10)
check(offset:stoptime() == offset_stop, "new(1000) stop stable")

check(offset:reset(500) == 500, "reset(500) returns base")
check(offset:time() >= 500, "reset(500) applies base")
check(offset:reset() == 0, "reset() returns zero")
check(offset:time() >= 0 and offset:time() < 500, "reset() clears base")

local clamped = timers.new(-50)
check(clamped:time() == 0, "negative initial clamps to zero")
check(clamped:reset(-10) == 0, "negative reset clamps to zero")

for i = 1, 200 do
    local tmp = timers.new(i)
    tmp:start()
end
collectgarbage("collect")
check(true, "timer GC completes")

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
