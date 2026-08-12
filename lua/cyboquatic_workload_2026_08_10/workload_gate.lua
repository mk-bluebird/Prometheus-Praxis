-- File: lua/cyboquatic_workload_2026_08_10/workload_gate.lua
local workload_gate = {}

function workload_gate.accept(frame)
    if type(frame) ~= "table" or type(frame.node_id) ~= "string" then
        return false, "invalid_frame"
    end
    local required = {"energyreqJ", "deltaVt", "knowledge_factor", "eco_impact_value"}
    for _, key in ipairs(required) do
        if type(frame[key]) ~= "number" then
            return false, "missing_" .. key
        end
    end
    if frame.energyreqJ < 0 or frame.deltaVt < 0 or frame.deltaVt > 0.20 then
        return false, "residual_corridor_breach"
    end
    if frame.knowledge_factor < 0.60 or frame.eco_impact_value < 0.55 then
        return false, "insufficient_eco_evidence"
    end
    return true, "low_impact_maintenance"
end

return workload_gate
