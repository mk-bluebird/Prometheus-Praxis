-- File: lua/cyboquatic/workload_gate.lua
local M = {}

local function is_finite(n)
  return type(n) == "number" and n == n and n ~= math.huge and n ~= -math.huge
end

function M.evaluate(frame)
  -- Check all five expected numeric fields are present and finite
  local required_fields = {"energyreqJ", "deltaVt", "knowledgeFactor", "ecoImpactValue", "fogConfidence"}
  for _, field in ipairs(required_fields) do
    if not is_finite(frame[field]) then
      return {
        accepted = false,
        route = "human-review",
        reason = string.format("missing or non-finite field: %s", field)
      }
    end
  end
  
  local safe = frame.energyreqJ >= 0 and frame.deltaVt >= 0 and frame.deltaVt <= 0.35
  local beneficial = frame.ecoImpactValue >= 0.60
  local knowledge_ok = frame.knowledgeFactor >= 0.75
  local fog_ok = frame.fogConfidence >= 0.75
  
  local accepted = safe and beneficial and knowledge_ok and fog_ok
  
  if not accepted then
    local reasons = {}
    if not (frame.energyreqJ >= 0 and frame.deltaVt >= 0 and frame.deltaVt <= 0.35) then
      table.insert(reasons, "deltaVt out of range")
    end
    if frame.ecoImpactValue < 0.60 then
      table.insert(reasons, "ecoImpactValue below threshold")
    end
    if frame.knowledgeFactor < 0.75 then
      table.insert(reasons, "knowledgeFactor below threshold")
    end
    if frame.fogConfidence < 0.75 then
      table.insert(reasons, "fogConfidence below threshold")
    end
    return {
      accepted = false,
      route = "human-review",
      reason = table.concat(reasons, "; ")
    }
  end
  
  return {accepted = true, route = "canal-restoration", reason = ""}
end

return M
