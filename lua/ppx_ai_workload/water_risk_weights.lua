-- File: lua/ppx_ai_workload/water_risk_weights.lua
local weights = require("ppx_ai_workload.water_risk_config")

local function clamp(value)
    return math.max(0.0, math.min(1.0, value))
end

return function(water_quality_index, turbidity_norm, dissolved_oxygen_norm)
    local risk =
        weights.w1 * (1.0 - water_quality_index) +
        weights.w2 * turbidity_norm +
        weights.w3 * (1.0 - dissolved_oxygen_norm)
    return clamp(risk)
end
