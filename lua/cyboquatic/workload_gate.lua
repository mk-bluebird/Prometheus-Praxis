-- File: lua/cyboquatic/workload_gate.lua
local M = {}

function M.evaluate(frame)
  assert(type(frame) == "table" and type(frame.energyreqJ) == "number")
  assert(type(frame.deltaVt) == "number" and type(frame.ecoImpactValue) == "number")
  local safe = frame.energyreqJ >= 0 and frame.deltaVt >= 0 and frame.deltaVt <= 0.35
  local beneficial = frame.ecoImpactValue >= 0.60
  return {accepted = safe and beneficial, route = safe and beneficial and "canal-restoration" or "human-review"}
end

return M
