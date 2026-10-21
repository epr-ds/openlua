-- OneLua math helper tests (run by test/host/host_main).

local ok, fails = 0, 0
local function check(cond, msg)
    if cond then ok = ok + 1
    else fails = fails + 1; print("  FAIL: " .. (msg or "?")) end
end

-- minmax keeps a value inside inclusive limits.
check(math.minmax(5, 0, 10) == 5, "minmax in range")
check(math.minmax(-1, 0, 10) == 0, "minmax below min")
check(math.minmax(11, 0, 10) == 10, "minmax above max")
check(math.minmax(0, 0, 10) == 0, "minmax lower edge")
check(math.minmax(10, 0, 10) == 10, "minmax upper edge")
check(math.minmax(4.5, 0, 10) == 4.5, "minmax keeps fractions")
check(math.minmax(-3.25, -4, -2) == -3.25, "minmax negatives")

-- 32-bit signed bitwise semantics.
check(math.bitnot(0) == -1, "bitnot(0)")
check(math.bitnot(-1) == 0, "bitnot(-1)")
check(math.bitnot(15) == -16, "bitnot(15)")
check(math.bitand(0xF0, 0x0F) == 0, "bitand disjoint")
check(math.bitand(0xFF, 0x0F) == 15, "bitand overlap")
check(math.bitand(-1, 0x12345678) == 0x12345678, "bitand all ones")
check(math.bitor(0xF0, 0x0F) == 0xFF, "bitor disjoint")
check(math.bitor(0x80000000, 1) == -2147483647, "bitor signed high bit")
check(math.bitnot(0x7FFFFFFF) == -2147483648, "bitnot signed high bit")
check(math.bitand(0xFFFFFFFF, 0x80000000) == -2147483648,
      "bitand 32-bit mask")

-- Standard math functions survive registration.
check(math.floor(3.9) == 3, "math.floor preserved")
check(math.sqrt(9) == 3, "math.sqrt preserved")
check(type(math.pi) == "number", "math.pi preserved")

-- Reject values that cannot be represented as a 32-bit bit pattern.
check(not pcall(math.bitand, 4294967296, 1), "bitand range error")
check(not pcall(math.bitor, -2147483649, 0), "bitor range error")
check(not pcall(math.bitnot, 1.5), "bitnot integer error")

print(string.format("\nRESULT: %d passed, %d failed", ok, fails))
if fails > 0 then os.exit(1) end
