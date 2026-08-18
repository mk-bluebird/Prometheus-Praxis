local function clamp(value, lower, upper)
    if value < lower then return lower end
    if value > upper then return upper end
    return value
end

local function numeric(index, label)
    local value = tonumber(arg[index])
    assert(value and value == value and value ~= math.huge and value ~= -math.huge, label .. " must be finite numeric")
    return value
end

local function update(state, elapsed_days, concentration_mg_l, reference_concentration_mg_l, flow_m3_s,
                      forgetting_factor, covariance_min, covariance_max, maximum_flow_drift_fraction)
    assert(elapsed_days > 0.0, "elapsed_days must be positive")
    assert(concentration_mg_l > 0.0, "concentration_mg_l must be positive")
    assert(reference_concentration_mg_l > 0.0, "reference_concentration_mg_l must be positive")
    assert(flow_m3_s >= 0.0, "flow_m3_s must be non-negative")
    assert(forgetting_factor > 0.0 and forgetting_factor <= 1.0, "forgetting_factor must be in (0, 1]")
    assert(covariance_min > 0.0 and covariance_max >= covariance_min, "invalid covariance bounds")
    assert(state.covariance >= covariance_min and state.covariance <= covariance_max, "state covariance outside bounds")
    assert(maximum_flow_drift_fraction >= 0.0, "maximum_flow_drift_fraction must be non-negative")

    local phi = -elapsed_days
    local y = math.log(concentration_mg_l / reference_concentration_mg_l)
    local reference_flow = state.reference_flow_m3_s > 0.0 and state.reference_flow_m3_s or math.max(flow_m3_s, 1.0e-12)
    local flow_drift = math.abs(flow_m3_s - reference_flow) / reference_flow
    local accepted = flow_drift <= maximum_flow_drift_fraction
    local denominator = forgetting_factor + phi * state.covariance * phi
    local gain = state.covariance * phi / denominator
    local residual = y - phi * state.k_per_day
    local raw_covariance = (state.covariance - gain * phi * state.covariance) / forgetting_factor
    local next_covariance = clamp(raw_covariance, covariance_min, covariance_max)
    local next_k = accepted and (state.k_per_day + gain * residual) or state.k_per_day

    return {
        k_per_day = next_k,
        covariance = next_covariance,
        reference_flow_m3_s = reference_flow,
        phi = phi,
        y = y,
        residual = residual,
        gain = gain,
        flow_drift = flow_drift,
        accepted = accepted
    }
end

if #arg < 11 or ((#arg - 8) % 3 ~= 0) then
    io.stderr:write(
        "usage: lua decay_constant_rls.lua <k0_per_day> <P0> <reference_flow_m3_s> <lambda> <Pmin> <Pmax> " ..
        "<max_flow_drift_fraction> <C0_mg_l> <elapsed_days> <C_mg_l> <flow_m3_s> [...]\n"
    )
    os.exit(64)
end

local ok, message = pcall(function()
    local state = {
        k_per_day = numeric(1, "k0_per_day"),
        covariance = numeric(2, "P0"),
        reference_flow_m3_s = numeric(3, "reference_flow_m3_s")
    }
    local lambda = numeric(4, "lambda")
    local p_min = numeric(5, "Pmin")
    local p_max = numeric(6, "Pmax")
    local max_flow_drift = numeric(7, "maximum_flow_drift_fraction")
    local c0 = numeric(8, "C0_mg_l")

    local step = 0
    for index = 9, #arg, 3 do
        local result = update(
            state,
            numeric(index, "elapsed_days"),
            numeric(index + 1, "concentration_mg_l"),
            c0,
            numeric(index + 2, "flow_m3_s"),
            lambda,
            p_min,
            p_max,
            max_flow_drift
        )
        state = result

        print(string.format(
            "step=%d accepted=%s k_per_day=%.8f P=%.8f phi=%.8f y=%.8f residual=%.8f gain=%.8f flow_drift=%.8f",
            step,
            tostring(result.accepted),
            result.k_per_day,
            result.covariance,
            result.phi,
            result.y,
            result.residual,
            result.gain,
            result.flow_drift
        ))
        step = step + 1
    end
end)

if not ok then
    io.stderr:write("error: " .. message .. "\n")
    os.exit(65)
end
