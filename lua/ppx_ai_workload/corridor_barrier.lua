-- File: lua/ppx_ai_workload/corridor_barrier.lua
local barrier = {}

local function bounded(value)
    return type(value) == "number" and value >= 0.0 and value <= 1.0
end

local function minimum(a, b, c)
    return math.min(a, math.min(b, c))
end

function barrier.value(k, e, r, limits)
    if not bounded(k) or not bounded(e) or not bounded(r) then
        return nil, "invalid_ker"
    end
    if type(limits) ~= "table" or not bounded(limits.k_min)
        or not bounded(limits.e_min) or not bounded(limits.r_max) then
        return nil, "invalid_limits"
    end
    return minimum(k - limits.k_min, e - limits.e_min, limits.r_max - r)
end

function barrier.admit(record, limits)
    if type(record) ~= "table" or type(record.delta_vt) ~= "number"
        or type(record.max_delta_vt) ~= "number" or type(record.gamma) ~= "number"
        or type(record.residual_penalty) ~= "number" then
        return false, "invalid_record"
    end
    if record.gamma < 0.0 or record.gamma >= 1.0
        or record.max_delta_vt < 0.0 or record.residual_penalty < 0.0 then
        return false, "invalid_barrier_parameters"
    end

    local current, current_reason = barrier.value(record.k, record.e, record.r, limits)
    if not current then return false, current_reason end
    local future, future_reason = barrier.value(record.next_k, record.next_e, record.next_r, limits)
    if not future then return false, future_reason end
    if current <= 0.0 then return false, "outside_safe_corridor" end
    if record.delta_vt > record.max_delta_vt then return false, "residual_limit_exceeded" end

    local required_future =
        (1.0 - record.gamma) * current +
        record.residual_penalty * math.max(record.delta_vt, 0.0)

    if future < required_future then
        return false, "barrier_forward_invariance_not_certified"
    end
    return true, "admit_with_barrier_certificate"
end

return barrier
