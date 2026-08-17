local function clamp(value, lower, upper)
    if value < lower then return lower end
    if value > upper then return upper end
    return value
end

local function number_argument(index, name)
    local value = tonumber(arg[index])
    assert(value, name .. " must be numeric")
    return value
end

local function update(state, phi, observation, lambda, p_min, p_max, residual_limit)
    assert(lambda > 0.0 and lambda <= 1.0, "lambda must be in (0, 1]")
    assert(p_min > 0.0 and p_max >= p_min, "invalid covariance limits")
    assert(state.covariance >= p_min and state.covariance <= p_max, "state covariance outside limits")
    assert(residual_limit > 0.0, "residual_limit must be positive")

    local denominator = lambda + phi * state.covariance * phi
    assert(denominator >= lambda, "invalid RLS denominator")

    local residual = observation - phi * state.theta
    local gain = state.covariance * phi / denominator
    local raw_covariance = (state.covariance - gain * phi * state.covariance) / lambda
    local next_covariance = clamp(raw_covariance, p_min, p_max)
    local gain_bound = p_max * math.abs(phi) / lambda
    local accepted = math.abs(residual) <= residual_limit
    local next_theta = accepted and (state.theta + gain * residual) or state.theta
    local model_error_bound = accepted and math.abs(residual) or residual_limit
    local corridor_radius = math.abs(phi) * model_error_bound + gain_bound * residual_limit

    return {
        theta = next_theta,
        covariance = next_covariance,
        residual = residual,
        gain = gain,
        gain_bound = gain_bound,
        corridor_radius = corridor_radius,
        accepted = accepted
    }
end

if #arg < 8 or ((#arg - 6) % 2 ~= 0) then
    io.stderr:write(
        "usage: lua bounded_rls_corridor.lua <theta0> <P0> <lambda> <Pmin> <Pmax> <residual_limit> " ..
        "<phi> <y> [<phi> <y> ...]\n"
    )
    os.exit(64)
end

local ok, message = pcall(function()
    local state = {
        theta = number_argument(1, "theta0"),
        covariance = number_argument(2, "P0")
    }
    local lambda = number_argument(3, "lambda")
    local p_min = number_argument(4, "Pmin")
    local p_max = number_argument(5, "Pmax")
    local residual_limit = number_argument(6, "residual_limit")

    local step = 0
    for index = 7, #arg, 2 do
        local result = update(
            state,
            number_argument(index, "phi"),
            number_argument(index + 1, "y"),
            lambda,
            p_min,
            p_max,
            residual_limit
        )
        state.theta = result.theta
        state.covariance = result.covariance

        print(string.format(
            "step=%d residual=%.8f gain=%.8f gain_bound=%.8f corridor_radius=%.8f accepted=%s theta=%.8f P=%.8f",
            step,
            result.residual,
            result.gain,
            result.gain_bound,
            result.corridor_radius,
            tostring(result.accepted),
            state.theta,
            state.covariance
        ))
        step = step + 1
    end
end)

if not ok then
    io.stderr:write("error: " .. message .. "\n")
    os.exit(65)
end
