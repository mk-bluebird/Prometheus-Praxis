local LakeOxygenTemperatureBudget = {}

local function finite(value, name)
    assert(type(value) == "number" and value == value and value ~= math.huge and value ~= -math.huge,
        name .. " must be finite")
    return value
end

local function nonnegative(value, name)
    finite(value, name)
    assert(value >= 0.0, name .. " must be non-negative")
    return value
end

local function bounded(value, name, low, high)
    finite(value, name)
    assert(value >= low and value <= high, name .. " must be within allowed bounds")
    return value
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

function LakeOxygenTemperatureBudget.oxygen_saturation_mg_l(water_temperature_c)
    finite(water_temperature_c, "water_temperature_c")
    local value = 14.652
        - 0.41022 * water_temperature_c
        + 0.007991 * water_temperature_c * water_temperature_c
        - 0.000077774 * water_temperature_c * water_temperature_c * water_temperature_c
    return math.max(0.0, value)
end

function LakeOxygenTemperatureBudget.step(state, parameters, forcing, dt_seconds)
    assert(type(state) == "table", "state must be a table")
    assert(type(parameters) == "table", "parameters must be a table")
    assert(type(forcing) == "table", "forcing must be a table")
    nonnegative(dt_seconds, "dt_seconds")
    assert(dt_seconds > 0.0, "dt_seconds must be positive")

    nonnegative(state.dissolved_oxygen_mg_l, "state.dissolved_oxygen_mg_l")
    finite(state.water_temperature_c, "state.water_temperature_c")

    nonnegative(parameters.reaeration_per_second, "parameters.reaeration_per_second")
    nonnegative(parameters.benthic_oxygen_demand_mg_l_s, "parameters.benthic_oxygen_demand_mg_l_s")
    nonnegative(parameters.biota_respiration_mg_l_s, "parameters.biota_respiration_mg_l_s")
    nonnegative(parameters.algal_respiration_mg_l_s, "parameters.algal_respiration_mg_l_s")
    nonnegative(parameters.mixing_gain_per_second, "parameters.mixing_gain_per_second")

    nonnegative(forcing.photosynthesis_mg_l_s, "forcing.photosynthesis_mg_l_s")
    bounded(forcing.light_fraction, "forcing.light_fraction", 0.0, 1.0)
    finite(forcing.mixed_layer_oxygen_mg_l, "forcing.mixed_layer_oxygen_mg_l")

    local oxygen_saturation = LakeOxygenTemperatureBudget.oxygen_saturation_mg_l(
        state.water_temperature_c
    )

    local reaeration = parameters.reaeration_per_second
        * (oxygen_saturation - state.dissolved_oxygen_mg_l)

    local photosynthesis = forcing.photosynthesis_mg_l_s * forcing.light_fraction

    local mixing = parameters.mixing_gain_per_second
        * (forcing.mixed_layer_oxygen_mg_l - state.dissolved_oxygen_mg_l)

    local oxygen_rate = reaeration
        + photosynthesis
        - parameters.benthic_oxygen_demand_mg_l_s
        - parameters.biota_respiration_mg_l_s
        - parameters.algal_respiration_mg_l_s
        + mixing

    local next_do = math.max(
        0.0,
        state.dissolved_oxygen_mg_l + oxygen_rate * dt_seconds
    )

    return {
        dissolved_oxygen_mg_l = next_do,
        water_temperature_c = state.water_temperature_c,
        oxygen_saturation_mg_l = oxygen_saturation,
        oxygen_rate_mg_l_s = oxygen_rate,
        reaeration_mg_l_s = reaeration,
        photosynthesis_mg_l_s = photosynthesis,
        mixing_mg_l_s = mixing
    }
end

function LakeOxygenTemperatureBudget.assess(state, parameters, forcing, dt_seconds, do_minimum_mg_l)
    nonnegative(do_minimum_mg_l, "do_minimum_mg_l")

    local next_state = LakeOxygenTemperatureBudget.step(
        state,
        parameters,
        forcing,
        dt_seconds
    )

    local margin = next_state.dissolved_oxygen_mg_l - do_minimum_mg_l
    local harm_risk = clamp(
        do_minimum_mg_l <= 0.0 and 0.0 or -margin / do_minimum_mg_l,
        0.0,
        1.0
    )

    return {
        projected_state = next_state,
        dissolved_oxygen_margin_mg_l = margin,
        corridor_status = margin >= 0.0 and "safe" or "breached",
        knowledge_factor = clamp(parameters.evidence_quality or 0.0, 0.0, 1.0),
        eco_impact_value = margin >= 0.0 and 1.0 or 0.0,
        harm_risk = harm_risk
    }
end

return LakeOxygenTemperatureBudget
