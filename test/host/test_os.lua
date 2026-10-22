-- OneLua os module tests (run by test/host/host_main).
-- The console-only queries use fixed host mocks.  os.exit(), os.restart() and
-- the dialog/PRX entry points are never invoked here: they are either
-- destructive or intentionally unsupported in this build.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

-- The stock Lua os table must still be there.
check(type(os) == "table", "os table present")
check(type(os.time) == "function", "stock os.time preserved")
check(type(os.date) == "function", "stock os.date preserved")
check(type(os.exit) == "function", "stock os.exit preserved")
check(type(os.time()) == "number" and os.time() > 0, "os.time works")
check(type(os.date("%Y")) == "string" and #os.date("%Y") == 4,
      "os.date works")
check(type(os.date("*t")) == "table", "os.date table form works")

local names = {
    "delay", "getdate", "nick", "mac", "language", "password",
    "ram", "totalram", "cpu", "bus", "infoms0", "infoef0",
    "cfw", "versiontxt", "message", "messagebox", "restart",
    "initprx", "stopprx", "requiere"
}
for _, name in ipairs(names) do
    check(type(os[name]) == "function", name .. " registered")
end

-- Safe blocking helper.
check(os.delay(1) == nil, "delay returns nil")

-- Date format used by OneLua.
local date = os.getdate()
check(type(date) == "string" and
      date:match("^%d%d/%d%d/%d%d%d%d @ %d%d:%d%d:%d%d$") ~= nil,
      "getdate format")

-- Host system-parameter mocks.
check(os.nick() == "OpenLua", "nick host mock")
check(os.language() == "ENGLISH", "language host mock")
check(os.password() == "", "password documented empty result")
check(os.mac():match("^%x%x:%x%x:%x%x:%x%x:%x%x:%x%x$") ~= nil,
      "mac format")
check(os.cfw() == "HOST", "cfw host value")
check(os.versiontxt() == "HOST", "versiontxt host value")

-- Memory and clock mocks.
check(os.ram() == 24 * 1024 * 1024, "ram host mock")
check(os.totalram() == 32 * 1024 * 1024, "totalram host mock")
check(os.ram() <= os.totalram(), "ram <= totalram")
check(os.cpu() == 333, "cpu host mock")
check(os.bus() == 166, "bus host mock")
check(os.cpu(222) == nil, "cpu setter returns nil")
check(os.cpu() == 222, "cpu setter updates host mock")
check(os.cpu(0) == nil, "cpu rejects zero")
check(os.cpu() == 222, "cpu invalid setter is a no-op")
check(os.cpu(999) == nil, "cpu rejects over maximum")
check(os.cpu() == 222, "cpu over maximum is a no-op")
check(os.cpu(333) == nil, "cpu reset returns nil")
check(os.cpu() == 333, "cpu restored")

-- Filesystem info: host ms0: maps to the test working directory.  ef0: is
-- intentionally absent on the host, matching a non-PSP-Go machine.
local ms = os.infoms0()
check(type(ms) == "table", "infoms0 returns a table")
check(type(ms.max) == "number" and ms.max > 0, "infoms0 max")
check(type(ms.free) == "number" and ms.free >= 0, "infoms0 free")
check(type(ms.used) == "number" and ms.used >= 0, "infoms0 used")
check(ms.used == ms.max - ms.free, "infoms0 used = max - free")
check(os.infoef0() == nil, "infoef0 empty result on host")

-- Predictable unsupported dialog/PRX behavior.
check(os.message("test") == nil, "message default result")
check(os.message("test", 1) == -1, "message mode result")
check(os.messagebox("title", "body") == 0, "messagebox unsupported result")
check(os.initprx("plugin.prx") == 0, "initprx unsupported result")
check(os.initprx("plugin.prx", "") == 0, "initprx args unsupported result")
check(os.stopprx(1) == false, "stopprx unsupported result")
check(os.requiere("plugin.prx") == 0, "requiere unsupported result")

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
