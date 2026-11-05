-- OneLua peripheral module host tests: umd, usb, sio and ir.
--
-- The host has no UMD drive, USB controller or serial/IR hardware, so this
-- checks the documented absent-hardware contract and argument handling.  UMD
-- value helpers are covered by test_umd.c; the USB and UMD paths are
-- PPSSPP-verified where the emulator implements them.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

local function count(...)
    return select("#", ...)
end

-- ---------------------------------------------------------------- umd ----
check(type(umd) == "table", "umd table registered")
for _, name in ipairs({ "init", "present", "size", "type", "gameid",
                        "info", "term" }) do
    check(type(umd[name]) == "function", "umd." .. name .. " is a function")
end
check(umd.dumpiso == nil, "umd.dumpiso is not exposed")
check(umd.launch == nil, "umd.launch is not exposed")

check(umd.present() == false, "no disc is present")
check(umd.init() == false, "init fails without a disc")
check(umd.type() == "-", "type is '-' without a disc")
check(umd.gameid() == "", "gameid is empty without a disc")
check(umd.info() == nil, "info is nil without a disc")
check(umd.size() == "0.0000", "size is zero without a disc")
check(count(umd.term()) == 0, "term returns nothing")
check(count(umd.term()) == 0, "term is idempotent")

-- ---------------------------------------------------------------- usb ----
check(type(usb) == "table", "usb table registered")
for _, name in ipairs({ "mstick", "umd", "flash0", "flash1", "flash2",
                        "flash3", "stop", "isconnected", "isactive" }) do
    check(type(usb[name]) == "function", "usb." .. name .. " is a function")
end

check(usb.isconnected() == false, "not connected before a mode")
check(usb.isactive() == false, "not active before a mode")
for _, name in ipairs({ "mstick", "umd", "flash0", "flash1", "flash2",
                        "flash3" }) do
    check(count(usb[name]()) == 0, "usb." .. name .. " returns nothing")
    check(usb.isactive() == false,
          "usb." .. name .. " stays inactive without USB hardware")
end
check(count(usb.stop()) == 0, "usb.stop returns nothing")
check(usb.isconnected() == false, "still not connected after stop")
check(usb.isactive() == false, "still not active after stop")

-- ---------------------------------------------------------------- sio ----
check(type(sio) == "table", "sio table registered")
for _, name in ipairs({ "init", "baud", "available", "read", "write",
                        "print", "putstring", "term" }) do
    check(type(sio[name]) == "function", "sio." .. name .. " is a function")
end

check(sio.init() == 0, "sio init reports unsupported")
check(sio.available() == 0, "sio buffer is empty")
check(sio.read() == -1, "sio read reports empty")
check(count(sio.baud(9600)) == 0, "sio.baud returns nothing")
check(count(sio.write(65)) == 0, "sio.write returns nothing")
check(count(sio.print("hello")) == 0, "sio.print returns nothing")
check(count(sio.putstring("hello", 3)) == 0, "sio.putstring returns nothing")
check(count(sio.putstring("hi", 99)) == 0, "putstring clamps the length")
check(sio.term() == 1, "sio term succeeds")
check(sio.term() == 1, "sio term is idempotent")

-- ---------------------------------------------------------------- ir -----
check(type(ir) == "table", "ir table registered")
for _, name in ipairs({ "init", "baud", "state", "get", "put", "read",
                        "write", "term" }) do
    check(type(ir[name]) == "function", "ir." .. name .. " is a function")
end

check(ir.init() == 0, "ir init reports unsupported")
check(ir.state() == false, "ir state is false")
check(ir.get() == -1, "ir get reports empty")
check(ir.read() == nil, "ir read reports nil")
check(ir.put(7) == 0, "ir put reports failure")
check(ir.write("hello") == -1, "ir write reports failure")
check(count(ir.baud(9600)) == 0, "ir.baud returns nothing")
check(ir.term() == 1, "ir term succeeds")
check(ir.term() == 1, "ir term is idempotent")

-- ---------------------------------------------------------- arguments ----
check(pcall(sio.baud) == false, "sio.baud requires a value")
check(pcall(sio.baud, "x") == false, "sio.baud requires a number")
check(pcall(sio.write) == false, "sio.write requires a value")
check(pcall(sio.print, {}) == false, "sio.print rejects a table")
check(pcall(sio.putstring, "hi") == false, "sio.putstring requires a length")
check(pcall(ir.put) == false, "ir.put requires a value")
check(pcall(ir.write, {}) == false, "ir.write rejects a table")

print(string.format("peripherals Lua: %d assertions, %d failure(s)", ok, fails))
if fails > 0 then error("peripheral tests failed") end
