-- File: lua/cyboquatic/fog_energy_gate.lua
local known_media = {
  ["canal-water"] = true,
  ["sediment"] = true,
  ["bioswale"] = true,
  ["recycled-polymer"] = true
}

local function permit(frame)
  local renewable = math.max(0.0, math.min(1.0, tonumber(frame.renewable_fraction) or 0.0))
  local energy = tonumber(frame.energyreq_j) or -1.0
  local residual = tonumber(frame.delta_vt) or 2.0
  return known_media[string.lower(frame.media or "")] == true
     and renewable >= 0.70 and energy >= 0.0 and residual >= 0.0 and residual <= 0.25
end

return { permit = permit }
