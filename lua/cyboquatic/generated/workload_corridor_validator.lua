-- File: lua/cyboquatic/generated/workload_corridor_validator.lua
-- Generated from aln/cyboquatic/workload_corridor_2026_08_09.aln; do not edit manually.
local M = {}

local function bounded(value)
  return type(value) == "number" and value == value
      and value >= 0.0 and value <= 1.0
end

function M.validate_workload_frame(frame)
  if type(frame) ~= "table" then return false, "frame must be a table" end
  if type(frame.owner_did) ~= "string" or frame.owner_did == "" then
    return false, "owner_did missing"
  end
  if type(frame.canal_node) ~= "string" or frame.canal_node == "" then
    return false, "canal_node missing"
  end
  if type(frame.energyreqJ) ~= "number" or frame.energyreqJ < 0.0 then
    return false, "energyreqJ invalid"
  end
  if not bounded(frame.deltaVt) or not bounded(frame.K)
      or not bounded(frame.E) or not bounded(frame.R) then
    return false, "KER or residual invalid"
  end
  if not bounded(frame.fog_confidence) or not bounded(frame.eco_impact_value) then
    return false, "FOG or impact invalid"
  end
  if frame.deltaVt > 0.35 then return false, "deltaVt corridor breached" end
  if frame.K * frame.E <= frame.R then return false, "KER corridor breached" end
  if frame.fog_confidence < 0.75 then return false, "FOG confidence corridor breached" end
  if frame.eco_impact_value < 0.60 then return false, "eco-impact corridor breached" end
  return true, nil
end

return M
