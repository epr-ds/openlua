-- OneLua splash module tests (run by test/host/host_main).
-- The host build validates argument handling and PNG loading; the actual
-- timed/skippable display is PPSSPP-verified.

local root = os.getenv("OPENLUA_ROOT") or ".."
local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

local new = root .. "/assets/splash.png"
local old = root .. "/assets/splash_old.png"
local school = root .. "/assets/splash_oldschool.png"
local missing = root .. "/assets/does-not-exist.png"

check(type(splash) == "table", "splash table registered")
check(type(splash.show) == "function", "splash.show is a function")
check(type(splash.old) == "function", "splash.old is a function")
check(type(splash.oldschool) == "function", "splash.oldschool is a function")
check(files.exists(new), "generated splash.png exists")
check(files.exists(old), "generated splash_old.png exists")
check(files.exists(school), "generated splash_oldschool.png exists")

local function stats()
    local s = splash._stats()
    return s.requested, s.loaded, s.missing, s.fallback
end

-- Defaults use the bundled assets.
check(select("#", splash.show()) == 0, "splash.show returns nothing")
local req, loaded, miss, fb = stats()
check(req == 1 and loaded == 1 and miss == 0 and fb == 0,
      "splash.show() loads the default image")

splash.old()
req, loaded, miss, fb = stats()
check(req == 1 and loaded == 1 and miss == 0 and fb == 0,
      "splash.old() loads the old default image")

splash.oldschool()
req, loaded, miss, fb = stats()
check(req == 1 and loaded == 1 and miss == 0 and fb == 0,
      "splash.oldschool() loads its default image")

-- Explicit paths and slideshow tables.
splash.show(new)
req, loaded, miss, fb = stats()
check(req == 1 and loaded == 1 and miss == 0 and fb == 0,
      "explicit path loads")

splash.show({ new, old, school })
req, loaded, miss, fb = stats()
check(req == 3 and loaded == 3 and miss == 0 and fb == 0,
      "table loads every image")

-- Missing entries are skipped without raising.
splash.show(missing)
req, loaded, miss, fb = stats()
check(req == 1 and loaded == 0 and miss == 1 and fb == 1,
      "missing image falls back")

splash.show({ missing, new })
req, loaded, miss, fb = stats()
check(req == 2 and loaded == 1 and miss == 1 and fb == 0,
      "missing entry in a slideshow is skipped")

splash.show({})
req, loaded, miss, fb = stats()
check(req == 0 and loaded == 0 and miss == 0 and fb == 1,
      "empty table uses the fallback")

-- Invalid arguments raise predictable errors.
check(not pcall(splash.show, 12), "numeric argument rejected")
check(not pcall(splash.show, true), "boolean argument rejected")
check(not pcall(splash.show, { new, 12 }), "non-string table entry rejected")

local too_many = {}
for i = 1, 33 do too_many[i] = new end
check(not pcall(splash.show, too_many), "more than 32 images rejected")

print(string.format("splash Lua: %d assertions, %d failure(s)", ok, fails))
if fails > 0 then error("splash host tests failed") end
