-- Lua 5.4
-- Phoenix restoration decision-support utilities for:
-- 1) soil-carbon trajectories under temperature stress,
-- 2) water-constrained nighttime heat-risk allocation, and
-- 3) cool-pavement albedo degradation estimation.
--
-- Inputs must be locally measured or independently validated. Outputs are
-- scenario estimates, not experimental proof or operational instructions.

local M = {}

local function finite(value)
    return type(value) == "number"
        and value == value
        and value ~= math.huge
        and value ~= -math.huge
end

local function clamp(value, low, high)
    if value < low then
        return low
    end
    if value > high then
        return high
    end
    return value
end

local function assert_positive(value, name)
    assert(finite(value) and value > 0, name .. " must be positive")
end

local function assert_nonnegative(value, name)
    assert(finite(value) and value >= 0, name .. " must be non-negative")
end

local function assert_fraction(value, name)
    assert(finite(value) and value >= 0 and value <= 1, name .. " must be within 0..1")
end

local function mean(values)
    assert(type(values) == "table" and #values > 0, "values must be non-empty")
    local sum = 0
    for index, value in ipairs(values) do
        assert(finite(value), "values[" .. index .. "] must be finite")
        sum = sum + value
    end
    return sum / #values
end

function M.temperature_adjusted_decay(k0_per_day, q10, temperature_c, reference_c)
    assert_nonnegative(k0_per_day, "k0_per_day")
    assert_positive(q10, "q10")
    assert(finite(temperature_c), "temperature_c must be finite")
    assert(finite(reference_c), "reference_c must be finite")

    return k0_per_day * q10 ^ ((temperature_c - reference_c) / 10)
end

function M.soil_carbon_step(carbon_kg_m2, input_kg_m2_day, k_per_day, dt_days)
    assert_nonnegative(carbon_kg_m2, "carbon_kg_m2")
    assert_nonnegative(input_kg_m2_day, "input_kg_m2_day")
    assert_nonnegative(k_per_day, "k_per_day")
    assert_positive(dt_days, "dt_days")

    local equilibrium = input_kg_m2_day / math.max(k_per_day, 1e-18)
    local remaining_fraction = math.exp(-k_per_day * dt_days)
    local next_carbon = equilibrium + (carbon_kg_m2 - equilibrium) * remaining_fraction

    return {
        carbon_kg_m2 = math.max(0, next_carbon),
        decay_loss_kg_m2 = math.max(
            0,
            carbon_kg_m2 + input_kg_m2_day * dt_days - next_carbon
        ),
        equilibrium_carbon_kg_m2 = equilibrium,
    }
end

function M.simulate_soil_carbon(scenario)
    assert(type(scenario) == "table", "scenario must be a table")
    assert_nonnegative(scenario.initial_carbon_kg_m2, "initial_carbon_kg_m2")
    assert_nonnegative(scenario.input_kg_m2_day, "input_kg_m2_day")
    assert_nonnegative(scenario.k0_per_day, "k0_per_day")
    assert_positive(scenario.q10, "q10")
    assert_positive(scenario.reference_temperature_c, "reference_temperature_c")
    assert(type(scenario.daily_temperature_c) == "table"
        and #scenario.daily_temperature_c > 0,
        "daily_temperature_c must be non-empty")

    local carbon = scenario.initial_carbon_kg_m2
    local total_input = 0
    local total_decay = 0
    local decay_rates = {}

    for day, temperature_c in ipairs(scenario.daily_temperature_c) do
        local k = M.temperature_adjusted_decay(
            scenario.k0_per_day,
            scenario.q10,
            temperature_c,
            scenario.reference_temperature_c
        )

        local result = M.soil_carbon_step(
            carbon,
            scenario.input_kg_m2_day,
            k,
            1
        )

        carbon = result.carbon_kg_m2
        total_input = total_input + scenario.input_kg_m2_day
        total_decay = total_decay + result.decay_loss_kg_m2
        decay_rates[day] = k
    end

    local net_change = carbon - scenario.initial_carbon_kg_m2
    local heat_exposure_factor = mean(decay_rates) / math.max(scenario.k0_per_day, 1e-18)

    return {
        final_carbon_kg_m2 = carbon,
        net_carbon_change_kg_m2 = net_change,
        total_input_kg_m2 = total_input,
        modeled_decay_loss_kg_m2 = total_decay,
        mean_temperature_adjusted_k_per_day = mean(decay_rates),
        heat_accelerated_decay_factor = heat_exposure_factor,
        compost_accrual_exceeds_modeled_loss = net_change > 0,
        knowledge_factor = clamp(
            0.45 * (scenario.temperature_measurement_coverage or 0)
                + 0.35 * (scenario.carbon_sampling_coverage or 0)
                + 0.20 * (scenario.replicate_plot_fraction or 0),
            0,
            1
        ),
        eco_impact_value = clamp(
            math.max(0, net_change) / math.max(total_input, 1e-12),
            0,
            1
        ),
        harm_risk = clamp(
            0.50 * (1 - (scenario.temperature_measurement_coverage or 0))
                + 0.30 * (1 - (scenario.carbon_sampling_coverage or 0))
                + 0.20 * clamp(heat_exposure_factor / 4, 0, 1),
            0,
            1
        ),
        limitation = "Q10 is a scenario approximation. Soil moisture, substrate quality, microbial acclimation, and extreme heat can alter decomposition responses.",
    }
end

function M.allocate_heat_recovery_water(tracts, municipal_water_budget_m3_day)
    assert(type(tracts) == "table" and #tracts > 0, "tracts must be non-empty")
    assert_nonnegative(municipal_water_budget_m3_day, "municipal_water_budget_m3_day")

    local candidates = {}

    for index, tract in ipairs(tracts) do
        assert(type(tract) == "table", "tract must be a table")
        assert(type(tract.id) == "string" and #tract.id > 0, "tract id required")
        assert_nonnegative(tract.maximum_water_m3_day, "maximum_water_m3_day")
        assert_nonnegative(tract.night_heat_risk_population, "night_heat_risk_population")
        assert_nonnegative(tract.canopy_cooling_c_per_m3, "canopy_cooling_c_per_m3")
        assert_nonnegative(tract.albedo_cooling_c, "albedo_cooling_c")
        assert_nonnegative(tract.water_penalty_c_per_m3, "water_penalty_c_per_m3")
        assert_fraction(tract.equity_priority, "equity_priority")
        assert_fraction(tract.ecological_suitability, "ecological_suitability")

        local net_cooling_per_m3 = math.max(
            0,
            tract.canopy_cooling_c_per_m3 - tract.water_penalty_c_per_m3
        )

        local risk_reduction_per_m3 = net_cooling_per_m3
            * tract.night_heat_risk_population
            * (0.70 + 0.30 * tract.equity_priority)
            * tract.ecological_suitability

        candidates[index] = {
            tract = tract,
            net_cooling_per_m3 = net_cooling_per_m3,
            risk_reduction_per_m3 = risk_reduction_per_m3,
        }
    end

    table.sort(candidates, function(left, right)
        return left.risk_reduction_per_m3 > right.risk_reduction_per_m3
    end)

    local remaining = municipal_water_budget_m3_day
    local allocation = {}
    local total_modeled_risk_reduction = 0

    for _, candidate in ipairs(candidates) do
        local tract = candidate.tract
        local water_m3_day = math.min(remaining, tract.maximum_water_m3_day)
        local canopy_delta = water_m3_day * tract.canopy_cooling_c_per_m3
        local total_delta_t = canopy_delta
            + tract.albedo_cooling_c
            - water_m3_day * tract.water_penalty_c_per_m3

        allocation[#allocation + 1] = {
            id = tract.id,
            allocated_water_m3_day = water_m3_day,
            unallocated_capacity_m3_day = tract.maximum_water_m3_day - water_m3_day,
            modeled_canopy_cooling_c = canopy_delta,
            modeled_albedo_cooling_c = tract.albedo_cooling_c,
            modeled_net_delta_t_c = total_delta_t,
            modeled_night_heat_risk_reduction = math.max(0, total_delta_t)
                * tract.night_heat_risk_population
                * (0.70 + 0.30 * tract.equity_priority),
            priority_per_m3 = candidate.risk_reduction_per_m3,
        }

        total_modeled_risk_reduction = total_modeled_risk_reduction
            + allocation[#allocation].modeled_night_heat_risk_reduction

        remaining = remaining - water_m3_day
        if remaining <= 0 then
            break
        end
    end

    return {
        allocation = allocation,
        unused_water_m3_day = remaining,
        modeled_total_night_heat_risk_reduction = total_modeled_risk_reduction,
        knowledge_factor = 0.55,
        eco_impact_value = clamp(
            total_modeled_risk_reduction / math.max(
                total_modeled_risk_reduction + 1,
                1
            ),
            0,
            1
        ),
        harm_risk = 0.35,
        limitation = "Replace tract coefficients with locally calibrated nighttime air-temperature, water-use, canopy-survival, and vulnerability data before policy use.",
    }
end

function M.estimate_albedo_asymptote(observations)
    assert(type(observations) == "table" and #observations >= 3,
        "at least three observations required")

    local minimum = observations[1].albedo
    for index, observation in ipairs(observations) do
        assert(type(observation) == "table", "observation must be a table")
        assert_nonnegative(observation.days_since_installation,
            "days_since_installation")
        assert_fraction(observation.albedo, "albedo")
        minimum = math.min(minimum, observation.albedo)
        assert(index == 1 or observation.days_since_installation
            >= observations[index - 1].days_since_installation,
            "observations must be time sorted")
    end

    return minimum
end

function M.estimate_pavement_lambda(observations, alpha_infinity)
    assert(type(observations) == "table" and #observations >= 3,
        "at least three observations required")
    assert_fraction(alpha_infinity, "alpha_infinity")

    local first = observations[1]
    assert_nonnegative(first.days_since_installation, "first observation time")
    assert_fraction(first.albedo, "first albedo")
    assert(first.albedo > alpha_infinity,
        "initial albedo must exceed alpha_infinity")

    local alpha0 = first.albedo
    local numerator = 0
    local denominator = 0
    local usable = 0

    for _, observation in ipairs(observations) do
        assert_nonnegative(observation.days_since_installation,
            "days_since_installation")
        assert_fraction(observation.albedo, "albedo")

        local residual = observation.albedo - alpha_infinity
        if observation.days_since_installation > 0 and residual > 0 then
            local x = observation.days_since_installation
            local y = math.log(residual / (alpha0 - alpha_infinity))
            numerator = numerator + x * y
            denominator = denominator + x * x
            usable = usable + 1
        end
    end

    assert(usable >= 2 and denominator > 0,
        "insufficient above-asymptote observations for lambda estimation")

    local lambda_per_day = -numerator / denominator
    assert(lambda_per_day >= 0,
        "estimated lambda is negative; inspect dates, masks, and alpha_infinity")

    local squared_error = 0
    local observed_values = {}

    for index, observation in ipairs(observations) do
        local predicted = alpha_infinity
            + (alpha0 - alpha_infinity)
            * math.exp(-lambda_per_day * observation.days_since_installation)

        squared_error = squared_error + (observation.albedo - predicted) ^ 2
        observed_values[index] = observation.albedo
    end

    local rmse = math.sqrt(squared_error / #observations)
    local observation_mean = mean(observed_values)

    return {
        alpha0 = alpha0,
        alpha_infinity = alpha_infinity,
        lambda_per_day = lambda_per_day,
        lambda_per_year = lambda_per_day * 365.2425,
        half_life_days = math.log(2) / math.max(lambda_per_day, 1e-18),
        rmse_albedo = rmse,
        observation_mean_albedo = observation_mean,
        knowledge_factor = clamp(
            0.45 * math.min(1, #observations / 12)
                + 0.30 * math.max(0, 1 - rmse / 0.05)
                + 0.25,
            0,
            1
        ),
        eco_impact_value = clamp(
            (alpha0 - alpha_infinity) / math.max(alpha0, 1e-12),
            0,
            1
        ),
        harm_risk = clamp(
            0.55 * math.min(1, rmse / 0.05)
                + 0.25 * (1 - math.min(1, #observations / 12))
                + 0.20,
            0,
            1
        ),
        limitation = "Use surface-reflectance products, strict roadway masks, cloud/shadow filtering, and consistent viewing-season windows. Satellite pixels may mix pavement, vehicles, markings, trees, and adjacent roofs.",
    }
end

return M
