-- ECS host tests (run by test/host/host_main).
--
-- The ECS world uses generational entity handles: once an entity is
-- destroyed, every old handle keeps its slot/generation pair and must stay
-- dead even when the slot is reused.  These tests pin that contract down
-- before the implementation exists.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

local function collect(iter)
    local out = {}
    for e in iter do out[#out + 1] = e end
    return out
end

-- ---- module surface ----------------------------------------------------

check(type(ecs) == "table", "ecs module registered")
check(type(ecs.world) == "function", "ecs.world is a function")
check(type(ecs.new) == "function", "ecs.new alias is a function")

local w = ecs.world()
check(type(w) == "userdata", "world is userdata")
check(w:count() == 0, "new world is empty")
check(ecs.new():count() == 0, "ecs.new creates another empty world")

-- ---- entities and generations ------------------------------------------

local e = w:spawn()
check(type(e) == "userdata", "spawn returns a handle")
check(w:alive(e) == true, "spawned entity is alive")
check(w:count() == 1, "count tracks spawns")
check(tostring(e):match("^ecs%.entity%(") ~= nil, "entity tostring is readable")

local e2 = w:spawn()
check(e2 ~= e and w:alive(e2), "spawn returns distinct handles")
check(w:count() == 2, "count tracks multiple spawns")

check(w:destroy(e) == true, "destroy reports success")
check(w:alive(e) == false, "destroyed entity is not alive")
check(w:count() == 1, "destroy decrements count")
check(w:destroy(e) == false, "destroy twice is a no-op")

-- The next spawn reuses the freed slot. The old handle must stay dead and
-- must not alias the new entity even though both describe the same slot.
local e3 = w:spawn()
check(e3 ~= e, "reused slot produces a new handle")
check(tostring(e3) ~= tostring(e), "reused slot has a new generation")
check(w:alive(e) == false, "stale handle stays dead after reuse")
check(w:alive(e3) == true, "reused slot is alive under the new handle")
check(w:count() == 2, "reuse keeps the count correct")

-- `create` is a convenience alias for `spawn`.
local e4 = w:create()
check(w:alive(e4) and e4 ~= e3, "create aliases spawn")
w:destroy(e4)

-- ---- components --------------------------------------------------------

check(w:set(e3, "position", { x = 1, y = 2 }) == true, "set stores a component")
check(w:has(e3, "position") == true, "has sees the component")
check(w:get(e3, "position").x == 1, "get returns the component value")

local shared = { x = 9 }
check(w:set(e3, "target", shared) == true, "set accepts any Lua value")
check(w:get(e3, "target") == shared, "get preserves table identity")

check(w:set(e3, "flag", false) == true, "false is a valid component value")
check(w:has(e3, "flag") == true, "has treats false as present")
check(w:get(e3, "flag") == false, "get returns false")

check(w:set(e3, "position", nil) == true, "setting nil removes the component")
check(w:has(e3, "position") == false, "removed component is absent")
check(w:get(e3, "position") == nil, "removed component reads nil")

check(w:set(e3, "score", 10) == true, "set numeric component")
check(w:remove(e3, "score") == true, "remove reports an existing component")
check(w:remove(e3, "score") == false, "remove is idempotent")
check(w:has(e3, "score") == false, "remove clears the component")

-- Stale handles must never create or mutate components.
check(w:set(e, "position", { x = 99 }) == false, "set on stale handle is refused")
check(w:get(e, "position") == nil, "get on stale handle returns nil")
check(w:has(e, "position") == false, "has on stale handle is false")
check(w:remove(e, "position") == false, "remove on stale handle is false")

-- Components are cleared when an entity dies and are not inherited by the
-- generation that reuses its slot.
local r1 = w:spawn()
w:set(r1, "a", 1)
w:set(r1, "b", 2)
check(w:destroy(r1) == true, "destroy entity with components")
local r2 = w:spawn()
check(r2 ~= r1, "reused slot after destroy allocates a new handle")
check(tostring(r1) ~= tostring(r2), "reused slot bumps the generation")
check(w:get(r2, "a") == nil and w:get(r2, "b") == nil,
      "reused slot inherits no components")
check(w:has(r2, "a") == false, "reused slot has no stale component")

-- Entity handles from another world are invalid, not resolvable pointers.
local wa, wb = ecs.world(), ecs.world()
local ea = wa:spawn()
wa:set(ea, "x", 1)
check(wb:alive(ea) == false, "entity is foreign to another world")
check(wb:get(ea, "x") == nil, "foreign entity reads nil")
check(wb:set(ea, "x", 2) == false, "foreign entity cannot be mutated")
check(wb:remove(ea, "x") == false, "foreign entity cannot be removed")
check(wb:destroy(ea) == false, "foreign entity cannot be destroyed")
check(wa:get(ea, "x") == 1, "foreign world did not mutate the owner")

-- ---- queries -----------------------------------------------------------

local qw = ecs.world()
local a = qw:spawn()
local b = qw:spawn()
local c = qw:spawn()
qw:set(a, "position", { x = 1 })
qw:set(a, "velocity", { x = 2 })
qw:set(b, "position", { x = 3 })
qw:set(b, "velocity", false)          -- present, but false
qw:set(c, "velocity", { x = 4 })

local pv = collect(qw:query("position", "velocity"))
check(#pv == 2, "query requires every named component")
check(pv[1] == a and pv[2] == b, "query preserves slot order and accepts false")

local pos = collect(qw:query("position"))
check(#pos == 2 and pos[1] == a and pos[2] == b, "query with one component")

local all = collect(qw:query())
check(#all == 3 and all[1] == a and all[2] == b and all[3] == c,
      "empty query lists every live entity")

check(#collect(qw:query("nope")) == 0, "unknown component matches nothing")
check(#collect(qw:query("position", "position")) == 2,
      "duplicate filter names are harmless")
check(pcall(function() return qw:query({}) end) == false,
      "query rejects non-string component names")

-- Destroying the current entity during iteration must not resurrect it or
-- loop forever.
local dw = ecs.world()
for i = 1, 5 do
    local d = dw:spawn()
    dw:set(d, "tag", true)
end
local seen = 0
for d in dw:query("tag") do
    seen = seen + 1
    dw:destroy(d)
end
check(seen == 5 and dw:count() == 0, "destroy inside query")

-- A slot reused behind the iterator cursor must not be visited twice.
local mw = ecs.world()
local m1 = mw:spawn(); mw:set(m1, "tag", 1)
local m2 = mw:spawn(); mw:set(m2, "tag", 1)
local m3 = mw:spawn(); mw:set(m3, "tag", 1)
local iter = mw:query("tag")
check(iter() == m1, "iterator first entity")
mw:destroy(m1)
check(iter() == m2, "iterator skips destroyed entity")
local m4 = mw:spawn(); mw:set(m4, "tag", 1)   -- reuses m1's slot
check(iter() == m3, "iterator does not revisit a reused earlier slot")
check(iter() == nil, "iterator terminates")
check(mw:alive(m4), "reused slot is alive for future queries")
check(#collect(mw:query("tag")) == 3, "future query sees reused slot")

-- A query closure keeps its world alive even if the caller drops the world.
local query
do
    local gw = ecs.world()
    local ge = gw:spawn()
    gw:set(ge, "kept", true)
    query = gw:query("kept")
end
collectgarbage("collect")
collectgarbage("collect")
check(#collect(query) == 1, "query keeps its world alive")

-- ---- systems -----------------------------------------------------------

local s = ecs.world()
local se = s:spawn()
s:set(se, "position", { x = 0 })
s:set(se, "velocity", { x = 5 })
local seen_dt, seen_extra
local sys = s:system("move", { "position", "velocity" },
    function(ent, dt, extra)
        local p = s:get(ent, "position")
        local v = s:get(ent, "velocity")
        p.x = p.x + v.x * dt
        seen_dt, seen_extra = dt, extra
    end)
check(type(sys) == "table" and sys.name == "move", "system returns a table")
check(sys.enabled == true, "systems are enabled by default")
check(s:update(2, "turbo") == 1, "update returns invocation count")
check(s:get(se, "position").x == 10, "system mutates components")
check(seen_dt == 2 and seen_extra == "turbo", "update forwards arguments")

-- Systems only visit entities that currently match the filter.
local nomatch = s:spawn()
s:set(nomatch, "position", { x = 0 })
check(s:update(1) == 1, "system skips entities missing a component")

-- Disabling and re-enabling a system controls whether it runs.
sys.enabled = false
check(s:update(1) == 0, "disabled system does not run")
sys.enabled = true
check(s:update(1) == 1, "re-enabled system runs again")

-- System registration order is execution order.
local ow = ecs.world()
local oe = ow:spawn()
local order = {}
ow:system("first", {}, function() order[#order + 1] = "first" end)
ow:system("second", {}, function() order[#order + 1] = "second" end)
check(ow:update(0) == 2, "two systems run once each")
check(order[1] == "first" and order[2] == "second", "systems run in order")
check(ow:alive(oe), "empty filter matches the live entity")

-- The filter is copied, so later edits to the caller's table do not change
-- which entities a system visits.
local fw = ecs.world()
local fe = fw:spawn()
fw:set(fe, "position", true)
local filter = { "position" }
local fhits = 0
fw:system("copied", filter, function() fhits = fhits + 1 end)
filter[1] = "velocity"
check(fw:update(1) == 1 and fhits == 1, "system copies its filter")

-- Auto-named systems are still addressable.
local auto = fw:system({ "position" }, function() end)
check(type(auto.name) == "string" and auto.name ~= "", "auto system is named")
check(fw:remove_system(auto.name) == true, "auto system can be removed")

-- Removing a system stops it; unknown names return false.
check(fw:remove_system("copied") == true, "remove_system reports success")
check(fw:update(1) == 0, "removed system does not run")
check(fw:remove_system("copied") == false, "remove_system is idempotent")
check(fw:remove_system("missing") == false, "unknown system returns false")

-- A system may destroy the current entity; later systems must not see it.
local dw2 = ecs.world()
local victim = dw2:spawn()
dw2:set(victim, "alive", true)
local later = 0
dw2:system("kill", { "alive" }, function(ent) dw2:destroy(ent) end)
dw2:system("observe", { "alive" }, function() later = later + 1 end)
check(dw2:update(0) == 1, "later system does not run for a destroyed entity")
check(later == 0 and dw2:count() == 0, "destroy from a system is visible")

-- Errors propagate out of update and leave the world usable.
local xw = ecs.world()
xw:spawn()
xw:system("boom", {}, function() error("boom!") end)
local pok, perr = pcall(xw.update, xw, 0.1)
check(pok == false, "system errors propagate")
check(tostring(perr):match("boom!") ~= nil, "system error keeps its message")
check(xw:remove_system("boom") == true, "failed system can be removed")
check(xw:update(0.1) == 0, "world still updates after an error")

-- ---- clear -------------------------------------------------------------

local cw = ecs.world()
local c1 = cw:spawn()
local c2 = cw:spawn()
cw:set(c1, "x", 1)
cw:set(c2, "x", 2)
local cleared_calls = 0
cw:system("counted", { "x" }, function() cleared_calls = cleared_calls + 1 end)
check(cw:clear() == 2, "clear reports destroyed entities")
check(cw:count() == 0, "clear empties the world")
check(cw:alive(c1) == false and cw:alive(c2) == false, "clear kills handles")
check(#collect(cw:query("x")) == 0, "clear empties components")
local c3 = cw:spawn()
check(c3 ~= c1 and c3 ~= c2, "spawn after clear is a new generation")
check(cw:get(c3, "x") == nil, "spawn after clear has no components")
cw:set(c3, "x", 3)
check(cw:update(0) == 1 and cleared_calls == 1, "clear keeps systems")
check(cw:clear() == 1 and cw:count() == 0, "clear again empties the world")

-- ---- validation --------------------------------------------------------

local vw = ecs.world()
local ve = vw:spawn()
check(pcall(vw.get, vw, 5, "x") == false, "get rejects non-entity arguments")
check(pcall(vw.set, vw, ve, 7, 1) == false, "set rejects non-string names")
check(pcall(vw.has, vw, false, "x") == false, "has rejects non-entity arguments")
check(pcall(vw.spawn, {}) == false, "spawn requires a world receiver")
check(pcall(function() return vw:system("bad", "x", function() end) end) == false,
      "system rejects a non-table filter")
check(pcall(function() return vw:system("bad", {}, 5) end) == false,
      "system rejects a non-function callback")

-- ---- growth and collection --------------------------------------------

local gw = ecs.world()
local handles = {}
for i = 1, 600 do
    local g = gw:spawn()
    gw:set(g, "v", i)
    handles[i] = g
end
check(gw:count() == 600, "world grows past the initial capacity")
for i = 3, 600, 3 do
    gw:destroy(handles[i])
end
check(gw:count() == 400, "bulk destroy keeps count accurate")
check(#collect(gw:query("v")) == 400, "query after bulk destroy")
for i = 1, 100 do
    local g = gw:spawn()
    gw:set(g, "v", -i)
end
check(gw:count() == 500, "spawns reuse freed slots")
check(#collect(gw:query("v")) == 500, "query after slot reuse")
check(gw:get(handles[1], "v") == 1, "surviving handle keeps its component")
check(gw:get(handles[3], "v") == nil, "destroyed handle stays dead after reuse")

for round = 1, 50 do
    local tw = ecs.world()
    for i = 1, 20 do
        local t = tw:spawn()
        tw:set(t, "x", i)
        if i % 2 == 0 then tw:destroy(t) end
    end
    collectgarbage("collect")
end
check(true, "world churn completes")

-- Destroyed handles (and their components) must become collectable once the
-- world is the only thing that used to reference them.
local weak_entity = setmetatable({}, { __mode = "v" })
local function make_destroyed()
    local ww = ecs.world()
    local we = ww:spawn()
    ww:set(we, "blob", { 1, 2, 3 })
    weak_entity[1] = we
    ww:destroy(we)
end
make_destroyed()
collectgarbage("collect")
collectgarbage("collect")
check(weak_entity[1] == nil, "destroyed handle is collectable")

-- A handle whose world was collected must still be safe to pass around: the
-- world id no longer matches, so it is dead instead of a dangling reference.
local saved_get, saved_entity
do
    local old = ecs.world()
    saved_entity = old:spawn()
    old:set(saved_entity, "x", 1)
    saved_get = old.get
end
collectgarbage("collect")
collectgarbage("collect")
local other = ecs.world()
check(saved_get(other, saved_entity, "x") == nil,
      "handle from a collected world is dead")

local orphan
do
    local ow2 = ecs.world()
    orphan = ow2:spawn()
    ow2:set(orphan, "x", 1)
end
collectgarbage("collect")
collectgarbage("collect")
check(tostring(orphan):match("^ecs%.entity%(") ~= nil,
      "entity handle outlives its world safely")

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
