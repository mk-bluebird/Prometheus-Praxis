-- Filename: fog_router_predicates/lyapunov_guard.lua
-- Scriptable predicate for the FOG router; loaded from Rust.
-- Lua 5.4+ is assumed and must exist in the runtime environment.

-- lyapunovok(previous_v, current_v) returns true iff V_{t+1} <= V_t.
function lyapunovok(previous_v, current_v)
    return current_v <= previous_v
end

-- can_route(rcoords_prev, rcoords_curr, weights, diagnostic_only)
-- rcoords_* are Lua tables with scalar fields matching the Rust RiskCoords.
function can_route(rcoords_prev, rcoords_curr, weights, diagnostic_only)
    local function residual(rc)
        local v = 0.0
        v = v + weights.whydraulic * rc.rhydraulic * rc.rhydraulic
        v = v + weights.wenergy * rc.renergy * rc.renergy
        v = v + weights.wbio * rc.rbio * rc.rbio
        v = v + weights.wtox * rc.rtox * rc.rtox
        v = v + weights.wmicro * rc.rmicro * rc.rmicro
        v = v + weights.wmaterials * rc.rmaterials * rc.rmaterials
        v = v + weights.wcarbon * rc.rcarbon * rc.rcarbon
        v = v + weights.wcalib * rc.rcalib * rc.rcalib
        v = v + weights.wsigma * rc.rsigma * rc.rsigma
        return v
    end

    local previous_v = residual(rcoords_prev)
    local current_v = residual(rcoords_curr)

    if not lyapunovok(previous_v, current_v) then
        return "DENY", previous_v, current_v
    end

    if diagnostic_only then
        return "SUGGEST_ONLY", previous_v, current_v
    else
        return "ALLOW", previous_v, current_v
    end
end
