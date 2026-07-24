-- ecorestorationshard/cyboquaticprogress/20260723-d-cyboquaticworkload/lua/cybo_workload_router.lua
-- Lua FOG-router predicates for non-actuating workload routing across lanes.
-- Designed for lightweight controllers and CLI tools.

local M = {}

-- Compute a simple safety score from KER and residual.
local function safety_score(k, e, r, delta_vt)
    if k < 0.0 or k > 1.0 then return 0.0 end
    if e < 0.0 or e > 1.0 then return 0.0 end
    if r < 0.0 or r > 1.0 then return 0.0 end
    if delta_vt > 0.0 then
        return 0.0
    end
    return k * e * (1.0 - r)
end

-- Decide lane for a workload frame: "RESEARCH", "PILOT", "PRODUCTION".
function M.decide_lane(frame)
    local k = frame.k or 0.0
    local e = frame.e or 0.0
    local r = frame.r or 1.0
    local delta_vt = frame.delta_vt or 0.0

    local score = safety_score(k, e, r, delta_vt)

    if score >= 0.8 then
        return "PRODUCTION"
    elseif score >= 0.5 then
        return "PILOT"
    else
        return "RESEARCH"
    end
end

-- Predicate: true only if frame is safe enough for production lane.
function M.is_safe_for_production(frame)
    return M.decide_lane(frame) == "PRODUCTION"
end

return M
