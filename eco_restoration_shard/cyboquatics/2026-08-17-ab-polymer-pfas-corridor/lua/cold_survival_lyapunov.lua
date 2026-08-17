local function number_argument(index, name)
    local value = tonumber(arg[index])
    assert(value, name .. " must be numeric")
    return value
end

if #arg < 5 then
    io.stderr:write(
        "usage: lua cold_survival_lyapunov.lua <lambda_per_day> <epsilon_per_day> <initial_V> <temperature_C> [<temperature_C> ...]\n"
    )
    os.exit(64)
end

local ok, message = pcall(function()
    local lambda = number_argument(1, "lambda_per_day")
    local epsilon = number_argument(2, "epsilon_per_day")
    local initial_v = number_argument(3, "initial_V")

    assert(lambda > 0.0, "lambda_per_day must be positive")
    assert(epsilon >= 0.0 and initial_v >= 0.0, "epsilon_per_day and initial_V must be non-negative")

    local ultimate_bound = epsilon / lambda
    local previous_v = initial_v
    local worst_violation = 0.0
    local cold_samples = 0

    for index = 4, #arg do
        local temperature_c = number_argument(index, "temperature_C")
        if temperature_c < 0.0 then
            cold_samples = cold_samples + 1
        end

        local thermal_disturbance = math.max(0.0, -temperature_c) * 0.01
        local next_v = math.max(0.0, previous_v - lambda * previous_v + epsilon + thermal_disturbance)
        local residual = math.max(0.0, next_v - ((1.0 - lambda) * previous_v + epsilon))
        worst_violation = math.max(worst_violation, residual)

        print(string.format(
            "temperature_c=%.3f V_previous=%.8f V_next=%.8f corridor_bound=%.8f thermal_residual=%.8f",
            temperature_c, previous_v, next_v, ultimate_bound, residual
        ))
        previous_v = next_v
    end

    local knowledge_factor = math.min(1.0, 0.35 + 0.10 * (#arg - 3))
    local harm_risk = math.min(1.0, 0.20 + 0.10 * cold_samples + (worst_violation > epsilon and 0.40 or 0.0))
    local eco_impact_value = math.max(0.0, knowledge_factor * (1.0 - harm_risk))

    print(string.format("ultimate_V_bound=%.8f", ultimate_bound))
    print(string.format("knowledge_factor=%.8f", knowledge_factor))
    print(string.format("eco_impact_value=%.8f", eco_impact_value))
    print(string.format("harm_risk=%.8f", harm_risk))
    print("decision=" .. (worst_violation <= epsilon and "CORRIDOR_REVIEW_PASSED" or "THERMAL_DISTURBANCE_REVIEW_REQUIRED"))
end)

if not ok then
    io.stderr:write("error: " .. message .. "\n")
    os.exit(65)
end
