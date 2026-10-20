-- JSON host tests (run by test/host/host_main).

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

local function same(a, b)
    if type(a) ~= type(b) then return false end
    if type(a) ~= "table" then return a == b end
    for k, v in pairs(a) do
        if not same(v, b[k]) then return false end
    end
    for k in pairs(b) do
        if a[k] == nil then return false end
    end
    return true
end

local function write_file(path, body)
    local f = assert(io.open(path, "wb"))
    f:write(body)
    f:close()
end

local function read_file(path)
    local f = assert(io.open(path, "rb"))
    local body = f:read("*a")
    f:close()
    return body
end

-- Objects, arrays, nesting and scalar types.
local v = json.decode([=[{"name":"OpenLua","ok":true,"no":false,"missing":null,
  "nums":[0,-1,1.5,1000,-0.025],"nested":{"x":"y","deep":[1,[2,[3]]]}}]=])
check(v.name == "OpenLua", "decode string value")
check(v.ok == true, "decode true")
check(v.no == false, "decode false")
check(v.missing == nil, "decode null")
check(v.nums[1] == 0 and v.nums[2] == -1, "decode integer array")
check(v.nums[3] == 1.5 and v.nums[4] == 1000, "decode float and exponent")
check(v.nums[5] == -0.025, "decode negative exponent")
check(v.nested.x == "y", "decode nested object")
check(v.nested.deep[2][2][1] == 3, "decode deeply nested array")

check(json.decode('"hello"') == "hello", "decode top-level string")
check(json.decode("true") == true, "decode top-level true")
check(json.decode("false") == false, "decode top-level false")
check(select("#", json.decode("null")) == 1 and json.decode("null") == nil,
      "decode top-level null")
check(json.decode("[null,1,null]")[1] == nil, "decode array null")
check(json.decode("[null,1,null]")[3] == nil, "decode trailing null")
check(json.decode("  [ 1 , 2 ]  ")[2] == 2, "decode whitespace")

-- Duplicate keys: last one wins.
check(json.decode('{"a":1,"a":2}').a == 2, "duplicate key last wins")

-- Escapes and unicode.
check(json.decode([["a\"b\\c\/d\b\f\n\r\t"]]) == 'a"b\\c/d\b\f\n\r\t',
      "decode escapes")
check(json.decode([["\u0041\u00e9"]]) == "A\195\169", "decode unicode escapes")
check(json.decode([["\uD83D\uDE00"]]) == "\240\159\152\128",
      "decode surrogate pair")
check(json.decode([["\u0000"]]) == "\0", "decode NUL escape")
check(json.decode([["line\u0001"]]) == "line\1", "decode control escape")

-- Invalid input must fail with a json.decode error, never crash.
local bad = {
    "", "{", "[1,]", '{"a":}', '"unterminated', "tru", "01", "1.",
    "1e", "+1", "-", '{"a" 1}', '{"a":1}x', "NaN", "Infinity",
    [[ "\uD800" ]], [[ "\uDC00" ]], [[ "\q" ]], "nul",
    "[1" .. string.char(10) .. ",]", '"raw' .. string.char(10) .. 'control"',
}
for i, src in ipairs(bad) do
    local good, err = pcall(json.decode, src)
    check(not good and tostring(err):find("json.decode", 1, true) ~= nil,
          "reject malformed input #" .. i)
end

-- Encoder scalars and strings.
check(json.encode(nil) == "null", "encode nil")
check(json.encode(true) == "true", "encode true")
check(json.encode(false) == "false", "encode false")
check(json.encode(42) == "42", "encode integer")
check(json.encode(-7) == "-7", "encode negative integer")
check(json.encode(1.5) == "1.5", "encode float")
check(json.encode("plain") == '"plain"', "encode string")
check(json.encode('a"b\\c\nd\1') == '"a\\"b\\\\c\\nd\\u0001"',
      "encode string escapes")
check(json.encode("nul\0byte") == '"nul\\u0000byte"', "encode NUL")
check(json.encode("\195\169") == '"\195\169"', "encode UTF-8 passthrough")

-- Tables: arrays, objects, empty.
check(json.encode({1, 2, 3}) == "[1,2,3]", "encode array")
check(json.encode({{1}, {2}}) == "[[1],[2]]", "encode nested array")
check(json.encode({}) == "{}", "encode empty table as object")
local encoded = json.encode({a = 1, b = "x", c = true})
check(same(json.decode(encoded), {a = 1, b = "x", c = true}),
      "round-trip object")

local rich = {
    name = "OpenLua",
    version = 2,
    enabled = true,
    values = {1, 2.5, "three", false},
    nested = {a = {b = {c = "deep"}}, empty = {}},
}
check(same(json.decode(json.encode(rich)), rich), "round-trip nested value")
check(same(json.decode('{"x":[1,2],"y":{"z":true}}'),
           {x = {1, 2}, y = {z = true}}), "round-trip decoded input")

-- Numbers preserve integer formatting where practical.
check(json.encode(json.decode("1000")) == "1000", "integer formatting")
check(json.encode(json.decode("1000.0")) == "1000", "whole float formatting")
check(json.encode(json.decode("1e3")) == "1000", "exponent formatting")

-- Unsupported values fail cleanly.
local bad_values = {
    function() end,
    {[1] = 1, [3] = 3},
    {[1] = 1, a = 2},
    {[0] = 1},
    {[-1] = 1},
    {[1.5] = 1},
    0 / 0,
    1 / 0,
    -1 / 0,
}
for i, value in ipairs(bad_values) do
    local good, err = pcall(json.encode, value)
    check(not good and tostring(err):find("json.encode", 1, true) ~= nil,
          "reject unencodable value #" .. i)
end
check(not pcall(json.encode), "encode requires a value")

-- File helpers: load/save, overwrite, atomic temp cleanup and errors.
local ROOT = "jsonwork"
files.delete(ROOT)
files.mkdir(ROOT)
local path = ROOT .. "/config.json"

local saved = {name = "camera", speed = 1.5, keys = {"up", "down"},
               nested = {enabled = false, list = {1, 2, 3}}}
check(json.save(path, saved) == true, "save returns true")
check(files.exists(path), "save creates file")
check(not files.exists(path .. ".~oltmp"), "no temp file after save")
check(not files.exists(path .. ".~olbak"), "no backup file after save")
check(same(json.load(path), saved), "load round-trips saved table")
check(read_file(path):find('"name"', 1, true) ~= nil, "saved file is JSON")

check(json.save(path, {n = 2}) == true, "overwrite existing file")
check(read_file(path):find('"keys"', 1, true) == nil, "old content replaced")
check(json.decode(read_file(path)).n == 2, "overwritten value readable")
check(same(json.load(path), {n = 2}), "load overwritten value")

local badpath = ROOT .. "/bad.json"
write_file(badpath, '{"a":')
local good, err = pcall(json.load, badpath)
check(not good and tostring(err):find("json.load", 1, true) ~= nil,
      "load malformed file errors")
check(not pcall(json.load, ROOT .. "/missing.json"),
      "load missing file errors")

write_file(ROOT .. "/large.json", "[" .. string.rep(" ", 262145) .. "]")
check(not pcall(json.load, ROOT .. "/large.json"),
      "load enforces 256 KB cap")

check(not pcall(json.save, ROOT .. "/missing/out.json", {}),
      "save missing directory errors")
check(not pcall(json.save, path, function() end),
      "save invalid value errors")
check(not pcall(json.save, ROOT .. "/large-out.json",
                string.rep("x", 262144)),
      "save enforces 256 KB cap")
check(not files.exists(ROOT .. "/large-out.json"),
      "oversized save leaves no file")

files.delete(ROOT)

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
