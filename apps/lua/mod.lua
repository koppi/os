-- Demo module for `require`. Staged as /hda/mod.lua; test.lua pulls it in.
local M = {}

function M.greet(who)
  return "hello, " .. (who or "world")
end

M.answer = 42

return M
