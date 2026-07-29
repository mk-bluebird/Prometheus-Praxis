-- filename: ecorestorationshard/cyboquatic_progress/20260728/lua/fog_router_workload.lua
-- purpose: Non-actuating Lua FOG-router predicates for cyboquatic workload frames (domain d)
-- domain: (d) Cyboquatic workload (energyreqJ, ΔVt)

local FOGRouter = {}

-- Simple predicate: allow only non-regressive residuals and energy within corridor
function FOGRouter.is_safe_workload(frame)
    -- frame fields: energyreqj, r_energy, vt_before, vt_after, k_metric, e_metric, r_metric
    if frame.energyreqj < 0.0 then
        return false
    end

    -- Corridor: r_energy must be <= 1.0
    if frame.r_energy > 1.0 then
        return false
    end

    -- Non-regression: ΔVt must be <= 0 to tighten corridors
    local delta_vt = frame.vt_after - frame.vt_before
    if delta_vt > 0.0 then
        return false
    end

    -- KER band: K and E high, R low
    if frame.k_metric < 0.80 then
        return false
    end
    if frame.e_metric < 0.80 then
        return false
    end
    if frame.r_metric > 0.20 then
        return false
    end

    return true
end

-- Lane classification into RESEARCH / PILOT / PRODUCTION
function FOGRouter.classify_lane(frame)
    if not FOGRouter.is_safe_workload(frame) then
        return "RESEARCH"
    end

    -- Tight KER bands for PRODUCTION
    if frame.k_metric >= 0.95 and frame.e_metric >= 0.95 and frame.r_metric <= 0.10 then
        return "PRODUCTION"
    end

    return "PILOT"
end

-- Example usage with a synthetic frame (non-actuating CLI-style diagnostic)
local example_frame = {
    energyreqj = 8.0e5,
    r_energy = 0.80,
    vt_before = 0.35,
    vt_after = 0.30,
    k_metric = 0.90,
    e_metric = 0.85,
    r_metric = 0.15
}

local lane = FOGRouter.classify_lane(example_frame)
print("lane=" .. lane)

return FOGRouter
