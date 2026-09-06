-- Smoke test for the koppi-os Lua port. Run with:  start rd/lua /rd/t.lua
print("Lua port self-test  --  " .. _VERSION)

-- numbers: 64-bit integers and IEEE doubles
assert(math.maxinteger == 0x7fffffffffffffff, "64-bit integers")
assert(1 + 2 == 3 and 2^10 == 1024.0 and 7 // 2 == 3 and 7 % 3 == 1)
assert(1e15 == 1000000000000000)
local s = 0
for i = 1, 100000 do s = s + i end
assert(s == 5000050000, "large integer arithmetic")

-- math library (x87 FPU)
assert(math.abs(math.sqrt(2) - 1.4142135623730951) < 1e-15)
assert(math.floor(3.7) == 3 and math.ceil(3.2) == 4 and math.floor(-1.5) == -2)
assert(math.abs(math.sin(math.pi / 6) - 0.5) < 1e-12)
assert(math.abs(math.exp(math.log(5)) - 5) < 1e-9)
assert(2 ^ 0.5 == math.sqrt(2))

-- strings and patterns
assert(("Hello, World"):gsub("o", "0") == "Hell0, W0rld")
local parts = {}
for w in ("the quick brown fox"):gmatch("%a+") do parts[#parts + 1] = w:upper() end
assert(table.concat(parts, "-") == "THE-QUICK-BROWN-FOX")
assert(string.format("%d/%s/%.3f", 42, "x", math.pi) == "42/x/3.142")

-- tables, sorting, metatables
local t = { 5, 3, 9, 1, 7 }
table.sort(t)
assert(table.concat(t, ",") == "1,3,5,7,9")
table.sort(t, function(a, b) return a > b end)
assert(table.concat(t, ",") == "9,7,5,3,1")
local mt = setmetatable({}, { __index = function(_, k) return k .. "!" end })
assert(mt.hi == "hi!")

-- error handling (setjmp/longjmp)
local ok, err = pcall(function() error("boom") end)
assert(not ok and err:match("boom$"))

-- coroutines
local co = coroutine.wrap(function() for i = 1, 3 do coroutine.yield(i * i) end end)
assert(co() == 1 and co() == 4 and co() == 9)

-- utf8
assert(utf8.char(72, 105) == "Hi" and utf8.len("Hej") == 3)

-- require a Lua module off the FAT volume (LUA_PATH = /rd/?.lua)
local mod = require("mod")
assert(mod.greet("koppi") == "hello, koppi" and mod.answer == 42)

-- os (trimmed): time / date / clock
assert(math.type(os.time()) == "integer")
assert(type(os.clock()) == "number")
print("wall clock (UTC): " .. os.date("!%Y-%m-%d %H:%M:%S"))

print("all checks passed")
