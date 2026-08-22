local Metrics = {}

local function finite(value, name)
    assert(type(value) == "number" and value == value and value ~= math.huge and value ~= -math.huge,
        name .. " must be finite")
    return value
end

local function positive(value, name)
    finite(value, name)
    assert(value > 0.0, name .. " must be positive")
    return value
end

local function nonnegative(value, name)
    finite(value, name)
    assert(value >= 0.0, name .. " must be non-negative")
    return value
end

local function bounded(value, name, low, high)
    finite(value, name)
    assert(value >= low and value <= high, name .. " must be within its permitted interval")
    return value
end

local function clamp01(value)
    if value < 0.0 then
        return 0.0
    end
    if value > 1.0 then
        return 1.0
    end
    return value
end

function Metrics.ecological_heat_stress(input)
    assert(type(input) == "table", "input must be a table")

    local air_temperature_c = finite(input.air_temperature_c, "air_temperature_c")
    local radiant_temperature_c = finite(input.radiant_temperature_c, "radiant_temperature_c")
    local relative_humidity_percent = bounded(
        input.relative_humidity_percent,
        "relative_humidity_percent",
        0.0,
        100.0
    )
    local air_weight = nonnegative(input.air_weight or 1.0, "air_weight")
    local radiant_weight = nonnegative(input.radiant_weight or 0.5, "radiant_weight")
    local humidity_weight = nonnegative(input.humidity_weight or 0.1, "humidity_weight")
    local humidity_threshold_percent = bounded(
        input.humidity_threshold_percent or 50.0,
        "humidity_threshold_percent",
        0.0,
        100.0
    )

    local humidity_excess = math.max(0.0, relative_humidity_percent - humidity_threshold_percent)

    return air_weight * air_temperature_c
        + radiant_weight * radiant_temperature_c
        + humidity_weight * humidity_excess
end

function Metrics.calibrate_heat_weights(observations, candidate_weights)
    assert(type(observations) == "table" and #observations >= 3,
        "at least three observations are required")
    assert(type(candidate_weights) == "table" and #candidate_weights > 0,
        "candidate_weights must be non-empty")

    local best = nil

    for _, weights in ipairs(candidate_weights) do
        local squared_error = 0.0

        for _, observation in ipairs(observations) do
            assert(type(observation) == "table", "observation must be a table")
            local prediction = Metrics.ecological_heat_stress({
                air_temperature_c = observation.air_temperature_c,
                radiant_temperature_c = observation.radiant_temperature_c,
                relative_humidity_percent = observation.relative_humidity_percent,
                air_weight = weights.air_weight,
                radiant_weight = weights.radiant_weight,
                humidity_weight = weights.humidity_weight,
                humidity_threshold_percent = weights.humidity_threshold_percent
            })

            local observed_risk = bounded(
                observation.observed_ecological_risk,
                "observed_ecological_risk",
                0.0,
                1.0
            )
            local normalized_prediction = clamp01(
                prediction / positive(weights.normalization_c, "normalization_c")
            )
            local residual = normalized_prediction - observed_risk
            squared_error = squared_error + residual * residual
        end

        local rmse = math.sqrt(squared_error / #observations)
        if best == nil or rmse < best.rmse then
            best = {
                air_weight = weights.air_weight,
                radiant_weight = weights.radiant_weight,
                humidity_weight = weights.humidity_weight,
                humidity_threshold_percent = weights.humidity_threshold_percent,
                normalization_c = weights.normalization_c,
                rmse = rmse
            }
        end
    end

    return best
end

function Metrics.carbon_negative_workload_capacity(input)
    assert(type(input) == "table", "input must be a table")

    local verified_removal = nonnegative(input.verified_removal_kg_co2e, "verified_removal_kg_co2e")
    local embodied = nonnegative(input.embodied_carbon_kg_co2e, "embodied_carbon_kg_co2e")
    local maintenance = nonnegative(input.maintenance_carbon_kg_co2e, "maintenance_carbon_kg_co2e")
    local noncompute = nonnegative(input.noncompute_carbon_kg_co2e, "noncompute_carbon_kg_co2e")
    local uncertainty_deduction = nonnegative(input.uncertainty_deduction_kg_co2e,
        "uncertainty_deduction_kg_co2e")
    local lifecycle_energy_per_work_unit_kwh = positive(
        input.lifecycle_energy_per_work_unit_kwh,
        "lifecycle_energy_per_work_unit_kwh"
    )
    local clean_energy_available_kwh = nonnegative(
        input.clean_energy_available_kwh,
        "clean_energy_available_kwh"
    )
    local operational_energy_per_work_unit_kwh = positive(
        input.operational_energy_per_work_unit_kwh,
        "operational_energy_per_work_unit_kwh"
    )
    local safe_power_kw = nonnegative(input.safe_power_kw, "safe_power_kw")
    local operational_power_per_work_unit_kw = positive(
        input.operational_power_per_work_unit_kw,
        "operational_power_per_work_unit_kw"
    )

    local net_carbon_budget = verified_removal
        - embodied
        - maintenance
        - noncompute
        - uncertainty_deduction

    local carbon_capacity = math.max(
        0.0,
        net_carbon_budget / lifecycle_energy_per_work_unit_kwh
    )
    local clean_energy_capacity = clean_energy_available_kwh / operational_energy_per_work_unit_kwh
    local safe_power_capacity = safe_power_kw / operational_power_per_work_unit_kw

    local workload_capacity = math.min(
        carbon_capacity,
        clean_energy_capacity,
        safe_power_capacity
    )

    local annual_operational_carbon = nonnegative(
        input.annual_operational_carbon_kg_co2e,
        "annual_operational_carbon_kg_co2e"
    )
    local embodied_to_operational_ratio = annual_operational_carbon > 0.0
        and embodied / annual_operational_carbon
        or math.huge

    return {
        net_carbon_budget_kg_co2e = net_carbon_budget,
        carbon_capacity_work_units = carbon_capacity,
        clean_energy_capacity_work_units = clean_energy_capacity,
        safe_power_capacity_work_units = safe_power_capacity,
        allowed_workload_capacity = workload_capacity,
        embodied_to_annual_operational_carbon_ratio = embodied_to_operational_ratio,
        carbon_negative = net_carbon_budget > 0.0 and workload_capacity > 0.0
    }
end

function Metrics.hydrologic_connectivity(edges, lambda)
    assert(type(edges) == "table" and #edges > 0, "edges must be non-empty")
    nonnegative(lambda, "lambda")

    local total = 0.0
    local total_capacity = 0.0
    local limiting_edge = nil
    local limiting_score = math.huge

    for _, edge in ipairs(edges) do
        assert(type(edge) == "table", "edge must be a table")
        nonnegative(edge.flow_capacity, "flow_capacity")
        finite(edge.water_quality_value, "water_quality_value")
        finite(edge.water_quality_critical, "water_quality_critical")

        local deviation = edge.water_quality_value - edge.water_quality_critical
        local quality_factor = math.exp(-lambda * deviation * deviation)
        local contribution = edge.flow_capacity * quality_factor

        total = total + contribution
        total_capacity = total_capacity + edge.flow_capacity

        local normalized_score = edge.flow_capacity > 0.0
            and contribution / edge.flow_capacity
            or 0.0

        if normalized_score < limiting_score then
            limiting_score = normalized_score
            limiting_edge = edge.edge_id
        end
    end

    local hci = total / #edges
    local capacity_normalized_hci = total_capacity > 0.0 and total / total_capacity or 0.0

    return {
        hci = hci,
        capacity_normalized_hci = capacity_normalized_hci,
        limiting_edge_id = limiting_edge,
        limiting_edge_quality_factor = limiting_score
    }
end

function Metrics.assess_connectivity(edges, lambda, minimum_normalized_hci)
    bounded(minimum_normalized_hci, "minimum_normalized_hci", 0.0, 1.0)

    local result = Metrics.hydrologic_connectivity(edges, lambda)
    local safe = result.capacity_normalized_hci >= minimum_normalized_hci

    return {
        hci = result.hci,
        capacity_normalized_hci = result.capacity_normalized_hci,
        limiting_edge_id = result.limiting_edge_id,
        limiting_edge_quality_factor = result.limiting_edge_quality_factor,
        corridor_status = safe and "safe" or "breached",
        knowledge_factor = 0.0,
        eco_impact_value = safe and result.capacity_normalized_hci or 0.0,
        harm_risk = clamp01(1.0 - result.capacity_normalized_hci)
    }
end

return Metrics
