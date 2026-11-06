-- OneLua networking module host tests: wlan, socket, http, ftp, adhoc and
-- bluetooth.
--
-- The host has no WiFi or adhoc hardware, so this checks the documented
-- absence contract, Lua lifecycle/argument handling and the socket userdata
-- API.  Live loopback socket/HTTP/FTP transfers are covered by test_net.c.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

local function count(...)
    return select("#", ...)
end

-- --------------------------------------------------------------- wlan ----
check(type(wlan) == "table", "wlan table registered")
for _, name in ipairs({ "init", "term", "connect", "disconnect",
                        "isconnected", "status", "getip", "strength",
                        "over", "getconfigs", "scan", "autoconnect",
                        "autostatus" }) do
    check(type(wlan[name]) == "function", "wlan." .. name .. " is a function")
end
check(wlan.add == nil, "wlan.add is not exposed")
check(wlan.copy == nil, "wlan.copy is not exposed")
check(wlan.delete == nil, "wlan.delete is not exposed")
check(wlan.backup == nil, "wlan.backup is not exposed")
check(wlan.restore == nil, "wlan.restore is not exposed")

check(wlan.init() == true, "host network stack initialises")
check(wlan.isconnected() == false, "host is not connected to WiFi")
check(wlan.status() == 0, "host reports disconnected")
check(wlan.getip() == nil, "host has no WiFi address")
check(wlan.strength() == 0, "host has no signal strength")
check(wlan.over() == "", "host has no connection name")
check(#wlan.getconfigs() == 0, "host has no saved network profiles")
check(#wlan.scan() == 0, "host has no access-point scan")
check(wlan.connect(1) == 0, "connect fails without WiFi")
check(count(wlan.autoconnect(1)) == 0, "autoconnect call is accepted")
check(wlan.autostatus() == false, "autoconnect thread is not running")
check(count(wlan.disconnect()) == 0, "disconnect returns nothing")
check(count(wlan.term()) == 0, "wlan.term returns nothing")
check(pcall(function() wlan.connect({}) end) == false,
      "wlan.connect validates the index")

-- ------------------------------------------------------------- socket ----
check(type(socket) == "table", "socket table registered")
for _, name in ipairs({ "server", "accept", "connect", "send", "recv",
                        "close", "term" }) do
    check(type(socket[name]) == "function", "socket." .. name .. " is a function")
end

local server = socket.server(0)
check(type(server) == "userdata", "server returns a socket userdata")
check(tostring(server):find("OL.Socket", 1, true) ~= nil,
      "socket has a readable type name")
check(count(socket.close(server)) == 0, "close returns nothing")
check(pcall(function() socket.close(server) end) == false,
      "closing a closed socket raises")
check(socket.connect("127.0.0.1", 1) == nil, "refused connection returns nil")
check(pcall(function() socket.connect() end) == false,
      "socket.connect validates the host")
check(pcall(function() socket.server("x") end) == false,
      "socket.server validates the port")

-- --------------------------------------------------------------- http ----
check(type(http) == "table", "http table registered")
for _, name in ipairs({ "getfile", "postfile", "request", "term" }) do
    check(type(http[name]) == "function", "http." .. name .. " is a function")
end

check(http.getfile("http://127.0.0.1:1/nope", "out/net-nope.bin") == false,
      "download failure returns false")
local response, err = http.request("GET", "http://127.0.0.1:1/nope")
check(response == nil and type(err) == "string",
      "request failure returns nil + message")
check(http.request("GET", "not-a-url") == nil,
      "malformed request URL fails safely")
check(pcall(function() http.getfile("http://127.0.0.1/") end) == false,
      "http.getfile validates the path")
check(count(http.term()) == 0, "http.term returns nothing")

-- ---------------------------------------------------------------- ftp ----
check(type(ftp) == "table", "ftp table registered")
for _, name in ipairs({ "init", "term", "state" }) do
    check(type(ftp[name]) == "function", "ftp." .. name .. " is a function")
end
check(ftp.state() == false, "ftp is stopped initially")
check(ftp.term() == true, "ftp.term reports success when already stopped")
check(ftp.state() == false, "ftp stays stopped after term")

-- -------------------------------------------------------------- adhoc ----
check(type(adhoc) == "table", "adhoc table registered")
for _, name in ipairs({ "init", "term", "getmac", "scan", "gettotal",
                        "sendrequest", "getrequest", "accept", "reject",
                        "send", "recv" }) do
    check(type(adhoc[name]) == "function", "adhoc." .. name .. " is a function")
end
check(adhoc.init() == 0, "adhoc init fails without hardware")
check(adhoc.getmac() == "", "adhoc has no MAC without hardware")
check(#adhoc.scan() == 0, "adhoc scan is empty without hardware")
check(adhoc.gettotal() == 0, "adhoc has no peers without hardware")
check(adhoc.sendrequest(1) == false, "adhoc request is not sent")
check(#adhoc.getrequest() == 0, "adhoc has no incoming request")
check(count(adhoc.accept(1)) == 0, "adhoc.accept returns nothing")
check(count(adhoc.reject(1)) == 0, "adhoc.reject returns nothing")
check(adhoc.send(1, "x") == 0, "adhoc.send reports zero bytes")
check(adhoc.recv(1) == nil, "adhoc.recv reports no data")
check(count(adhoc.term()) == 0, "adhoc.term returns nothing")
check(pcall(function() adhoc.sendrequest({}) end) == false,
      "adhoc.sendrequest validates the index")

-- ----------------------------------------------------------- bluetooth ----
check(type(bluetooth) == "table", "bluetooth table registered")
check(bluetooth.init() == false, "bluetooth is unavailable")
check(bluetooth.state() == false, "bluetooth state is false")
check(bluetooth.available() == false, "bluetooth availability is false")
check(bluetooth.term() == false, "bluetooth term reports unavailable")

print(string.format("net Lua: %d assertions, %d failure(s)", ok, fails))
if fails > 0 then error("net Lua tests failed") end
