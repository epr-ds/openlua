-- OpenLua - OneLua networking module demo (wlan, socket, http, ftp, adhoc,
-- bluetooth).
--
--   wlan.init() / wlan.isconnected() / wlan.status() / wlan.getip()
--   wlan.getconfigs() / wlan.scan()
--   socket.connect()      -- fails safely without a reachable peer
--   http.request()        -- fails safely without a reachable server
--   ftp.state() / ftp.term()
--   adhoc.init() / adhoc.getmac() / adhoc.scan() / adhoc.gettotal()
--   bluetooth.available()
--
-- Everything is safe without a WiFi connection or network hardware.  On the
-- host and in PPSSPP the queries report "not connected", the loopback requests
-- fail, and the adhoc/Bluetooth tables report their documented empty values.
-- No profile is added, changed or deleted.

local f = font.new("assets/font.png", 8, 16)
if f then
    screen.font(f)
end

local white = color.new(238, 240, 246)
local muted = color.new(120, 132, 154)
local teal  = color.new(48, 180, 160)
local amber = color.new(235, 180, 70)
local red   = color.new(230, 96, 96)
local dark  = color.new(10, 14, 26)

local function onoff(v)
    if v then
        return "YES"
    end
    return "NO"
end

local function text(v)
    if v == nil or v == "" then
        return "-"
    end
    return tostring(v)
end

local wlan_ok = wlan.init()
local connected = wlan.isconnected()
local status = wlan.status()
local ip = wlan.getip()
local strength = wlan.strength()
local profiles = #wlan.getconfigs()
local access_points = #wlan.scan()

-- A refused loopback connection is the documented failure path.  The socket
-- userdata is collected normally; socket.term() closes anything still open.
local sock, sock_err = socket.connect("127.0.0.1", 1)
if sock then
    socket.close(sock)
end
socket.term()

-- No TLS and no background thread: both requests run synchronously and report
-- nil + a message when the server is unreachable.
local response, http_err = http.request("GET", "http://127.0.0.1:1/")
local downloaded = http.getfile("http://127.0.0.1:1/nope", "net-nope.bin")
http.term()

local ftp_running = ftp.state()
ftp.term()

local adhoc_ok = adhoc.init()
local mac = adhoc.getmac()
local peers = #adhoc.scan()
local total = adhoc.gettotal()
adhoc.term()

local bluetooth_ok = bluetooth.available()

local function pick(ok, good, bad)
    if ok then
        return good, teal
    end
    return bad, red
end

while true do
    screen.clear(dark)
    draw.fillrect(0, 0, 480, 4, teal)
    draw.fillrect(0, 268, 480, 4, amber)

    if f then
        screen.print(18, 10, "networking / wlan - socket - http", f, white)
        screen.print(18, 28, "ftp - adhoc - bluetooth", f, white)

        screen.print(18, 52, "wlan.init        " .. onoff(wlan_ok), f,
                     wlan_ok and teal or red)
        screen.print(18, 68, "wlan.connected   " .. onoff(connected), f, muted)
        screen.print(18, 84, "wlan.status      " .. text(status), f, muted)
        screen.print(18, 100, "wlan.ip          " .. text(ip), f, muted)
        screen.print(18, 116, "wlan.strength    " .. text(strength), f, muted)
        screen.print(18, 132, "profiles / aps   " .. profiles .. " / " ..
                     access_points, f, muted)

        screen.print(250, 52, "connect   " .. text(sock_err), f, red)
        screen.print(250, 68, "term      ok", f, teal)
        screen.print(250, 84, "http      " .. text(http_err), f, red)
        screen.print(250, 100, "getfile   " .. onoff(downloaded), f, muted)
        screen.print(250, 116, "ftp state " .. onoff(ftp_running) ..
                     " / ok", f, muted)

        screen.print(18, 160, "adhoc.init       " .. text(adhoc_ok), f, red)
        screen.print(18, 176, "adhoc.mac        " .. text(mac), f, muted)
        screen.print(18, 192, "peers / total    " .. peers .. " / " .. total,
                     f, muted)
        screen.print(18, 208, "bluetooth        " .. onoff(bluetooth_ok), f, red)

        screen.print(18, 240, "Emulator: no WiFi, adhoc or Bluetooth hardware", f,
                     muted)
    end

    screen.flip()
end
