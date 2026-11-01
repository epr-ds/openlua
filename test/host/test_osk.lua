-- OneLua osk module host tests.
-- The host build validates arguments, defaults, clamping and UTF-8
-- conversion; the actual edit/confirm dialog is PPSSPP-verified. PPSSPP has no
-- OSK cancel control, so the nil/cancel branch is PSP-only.

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

check(type(osk) == "table", "osk table registered")
check(type(osk.init) == "function", "osk.init is a function")
check(type(osk._last) == "function", "host diagnostics available")

local function last()
    return osk._last()
end

-- Defaults: normal mode, 128 characters, one line, firmware language.
check(select("#", osk.init("d", "t")) == 1, "returns exactly one value")
local r = osk.init("Enter a name", "OpenLua")
check(r == "OpenLua", "returns the initial text when confirmed")
local s = last()
check(s.type == 1, "default type is normal")
check(s.textmax == 128, "default textmax is 128")
check(s.lines == 1, "default lines is 1")
check(s.mode == -1, "default mode is firmware default")
check(s.language == 0, "firmware mode maps to language 0")
check(s.inputtype == 0, "normal mode uses the all-input mask")
check(s.opened == 0, "host does not open a dialog")
check(s.cancelled == 0, "host result is not cancelled")
check(s.result == "OpenLua", "result records the echoed text")

-- The documented OneLua language codes map to PSP language values.
local languages = {
    [0] = 1,  [1] = 2,  [2] = 4,  [3] = 5,  [4] = 6,
    [5] = 7,  [6] = 8,  [7] = 9,  [8] = 10, [9] = 0,
    [10] = 0, [-1] = 0,
}
for mode, language in pairs(languages) do
    osk.init("d", "t", 1, 16, 1, mode)
    s = last()
    check(s.mode == mode and s.language == language,
          string.format("mode %d maps to language %d", mode, language))
end

-- Explicit values and numeric mode.
r = osk.init("PIN", "42", 2, 8, 3, 1)
check(r == "42", "numeric result echoes")
s = last()
check(s.type == 2 and s.textmax == 8 and s.lines == 3 and s.mode == 1,
      "explicit type/textmax/lines/mode are preserved")
check(s.language == 2 and s.inputtype == 1,
      "numeric mode uses the digit mask")

-- type: only 2 selects numeric; everything else is normal.
osk.init("d", "t", 0)
check(last().type == 1, "type 0 clamps to normal")
osk.init("d", "t", 3)
check(last().type == 1, "type 3 clamps to normal")
osk.init("d", "t", 2)
check(last().type == 2, "type 2 selects numeric")

-- textmax: clamp to 1..512 and truncate the initial text.
r = osk.init("d", "abcdef", 1, 3)
check(r == "abc" and last().textmax == 3, "textmax truncates the text")
r = osk.init("d", "abcdef", 1, 0)
check(r == "a" and last().textmax == 1, "textmax 0 clamps to 1")
r = osk.init("d", "abcdef", 1, -20)
check(r == "a" and last().textmax == 1, "negative textmax clamps to 1")
r = osk.init("d", "abcdef", 1, 9999)
check(r == "abcdef" and last().textmax == 512,
      "large textmax clamps to 512")

-- lines: clamp to 1..8.
osk.init("d", "t", 1, 16, 0)
check(last().lines == 1, "lines 0 clamps to 1")
osk.init("d", "t", 1, 16, 99)
check(last().lines == 8, "large lines clamps to 8")
osk.init("d", "t", 1, 16, 2)
check(last().lines == 2, "explicit lines preserved")

-- Unknown modes fall back to the firmware default.
osk.init("d", "t", 1, 16, 1, 11)
check(last().mode == -1 and last().language == 0,
      "mode 11 falls back to firmware default")
osk.init("d", "t", 1, 16, 1, -5)
check(last().mode == -1 and last().language == 0,
      "mode -5 falls back to firmware default")

-- UTF-8 conversion and safe truncation.
r = osk.init("d", "héllo", 1, 2)
check(r == "hé", "UTF-8 text is truncated by character")
r = osk.init("d", "🙂abc", 1, 2)
check(r == "🙂", "surrogate pair fits when two units are available")
r = osk.init("d", "🙂abc", 1, 1)
check(r == "", "surrogate pair is never split")
r = osk.init("d", "", 1, 16)
check(r == "", "empty input returns an empty string")

-- Invalid arguments raise predictable errors.
check(not pcall(osk.init), "missing desc and text rejected")
check(not pcall(osk.init, "d"), "missing text rejected")
check(not pcall(osk.init, "d", nil), "nil text rejected")
check(not pcall(osk.init, {}, "t"), "table desc rejected")
check(not pcall(osk.init, "d", 12), "numeric text rejected")
check(not pcall(osk.init, "d", "t", "2"), "string type rejected")
check(not pcall(osk.init, "d", "t", 1, {}), "table textmax rejected")
check(not pcall(osk.init, "d", "t", 1, 16, false), "boolean lines rejected")
check(not pcall(osk.init, "d", "t", 1, 16, 1, "1"), "string mode rejected")

print(string.format("osk Lua: %d assertions, %d failure(s)", ok, fails))
if fails > 0 then error("osk host tests failed") end
