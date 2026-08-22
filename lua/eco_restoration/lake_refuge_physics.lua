local LakeRefugePhysics = {}

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
    assert(value >= low and value <= high, name .. " must be within permitted bounds")
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

function LakeRefugePhysics.stratification_number(input)
    assert(type(input) == "table", "input must be a table")

    local g = positive(input.gravity_m_s2 or 9.80665, "gravity_m_s2")
    local alpha = positive(input.thermal_expansion_per_c, "thermal_expansion_per_c")
    local delta_t = nonnegative(input.temperature_difference_c, "temperature_difference_c")
    local depth = positive(input.mean_depth_m, "mean_depth_m")
    local viscosity = positive(input.kinematic_viscosity_m2_s, "kinematic_viscosity_m2_s")
    local diffusivity = positive(input.thermal_diffusivity_m2_s, "thermal_diffusivity_m2_s")

    return (g * alpha * delta_t * depth ^ 3) / (viscosity * diffusivity)
end

function LakeRefugePhysics.drawdown_ratio(initial_depth_m, current_depth_m)
    positive(initial_depth_m, "initial_depth_m")
    nonnegative(current_depth_m, "current_depth_m")
    assert(current_depth_m <= initial_depth_m,
        "current_depth_m must not exceed initial_depth_m")

    return current_depth_m / initial_depth_m
end

function LakeRefugePhysics.stratification_retention_ratio(initial_depth_m, current_depth_m)
    local z_ratio = LakeRefugePhysics.drawdown_ratio(initial_depth_m, current_depth_m)
    return z_ratio ^ 3
end

function LakeRefugePhysics.evaporation_rate(input)
    assert(type(input) == "table", "input must be a table")

    local base_rate_m_s = nonnegative(input.base_rate_m_s, "base_rate_m_s")
    local temperature_sensitivity_per_c = nonnegative(
        input.temperature_sensitivity_per_c,
        "temperature_sensitivity_per_c"
    )
    local water_temperature_c = finite(input.water_temperature_c, "water_temperature_c")
    local reference_temperature_c = finite(input.reference_temperature_c, "reference_temperature_c")
    local surface_area_m2 = positive(input.surface_area_m2, "surface_area_m2")

    local thermal_factor = math.exp(
        temperature_sensitivity_per_c * (water_temperature_c - reference_temperature_c)
    )

    return base_rate_m_s * thermal_factor * surface_area_m2
end

function LakeRefugePhysics.water_balance_step(state, forcing, dt_seconds)
    assert(type(state) == "table", "state must be a table")
    assert(type(forcing) == "table", "forcing must be a table")
    positive(dt_seconds, "dt_seconds")

    positive(state.volume_m3, "state.volume_m3")
    positive(state.heat_capacity_j_per_c, "state.heat_capacity_j_per_c")
    finite(state.temperature_c, "state.temperature_c")

    nonnegative(forcing.precipitation_inflow_m3_s, "precipitation_inflow_m3_s")
    nonnegative(forcing.managed_inflow_m3_s, "managed_inflow_m3_s")
    nonnegative(forcing.outflow_m3_s, "outflow_m3_s")
    finite(forcing.net_heat_flux_w, "net_heat_flux_w")
    nonnegative(forcing.evaporation_volume_m3_s, "evaporation_volume_m3_s")

    local inflow = forcing.precipitation_inflow_m3_s + forcing.managed_inflow_m3_s
    local d_volume_dt = inflow - forcing.evaporation_volume_m3_s - forcing.outflow_m3_s
    local next_volume = state.volume_m3 + d_volume_dt * dt_seconds

    assert(next_volume > 0.0, "projected lake volume is non-positive")

    local next_temperature = state.temperature_c
        + forcing.net_heat_flux_w * dt_seconds / state.heat_capacity_j_per_c

    return {
        volume_m3 = next_volume,
        temperature_c = next_temperature,
        volume_rate_m3_s = d_volume_dt
    }
end

function LakeRefugePhysics.community_refuge_volume(cells, species)
    assert(type(cells) == "table" and #cells > 0, "cells must be non-empty")
    assert(type(species) == "table" and #species > 0, "species must be non-empty")

    local refuge_volume_m3 = 0.0
    local limiting_species = nil
    local species_refuge_volume = {}

    for _, entry in ipairs(species) do
        assert(type(entry) == "table", "species entry must be a table")
        assert(type(entry.name) == "string" and entry.name:match("%S"),
            "species name must be non-empty")
        finite(entry.temperature_min_c, "temperature_min_c")
        finite(entry.temperature_max_c, "temperature_max_c")
        assert(entry.temperature_min_c <= entry.temperature_max_c,
            "species temperature range is invalid")
        species_refuge_volume[entry.name] = 0.0
    end

    for _, cell in ipairs(cells) do
        assert(type(cell) == "table", "refuge cell must be a table")
        positive(cell.volume_m3, "cell.volume_m3")
        finite(cell.temperature_c, "cell.temperature_c")
        nonnegative(cell.dissolved_oxygen_mg_l, "cell.dissolved_oxygen_mg_l")

        local all_species_safe = true

        for _, entry in ipairs(species) do
            local temperature_safe =
                cell.temperature_c >= entry.temperature_min_c
                and cell.temperature_c <= entry.temperature_max_c
            local oxygen_safe = cell.dissolved_oxygen_mg_l >= entry.minimum_do_mg_l

            if temperature_safe and oxygen_safe then
                species_refuge_volume[entry.name] =
                    species_refuge_volume[entry.name] + cell.volume_m3
            else
                all_species_safe = false
            end
        end

        if all_species_safe then
            refuge_volume_m3 = refuge_volume_m3 + cell.volume_m3
        end
    end

    local smallest_volume = math.huge
    for name, volume in pairs(species_refuge_volume) do
        if volume < smallest_volume then
            smallest_volume = volume
            limiting_species = name
        end
    end

    return {
        community_refuge_volume_m3 = refuge_volume_m3,
        species_refuge_volume_m3 = species_refuge_volume,
        limiting_species = limiting_species,
        limiting_species_refuge_volume_m3 = smallest_volume
    }
end

function LakeRefugePhysics.assess(input)
    assert(type(input) == "table", "input must be a table")

    local stratification = LakeRefugePhysics.stratification_number(input.stratification)
    local drawdown_ratio = LakeRefugePhysics.drawdown_ratio(
        input.initial_depth_m,
        input.current_depth_m
    )
    local retention_ratio = LakeRefugePhysics.stratification_retention_ratio(
        input.initial_depth_m,
        input.current_depth_m
    )
    local refuge = LakeRefugePhysics.community_refuge_volume(
        input.refuge_cells,
        input.species
    )

    local critical_refuge_volume_m3 = positive(
        input.critical_refuge_volume_m3,
        "critical_refuge_volume_m3"
    )

    local refuge_ratio = refuge.community_refuge_volume_m3 / critical_refuge_volume_m3
    local harm_risk = clamp01(1.0 - refuge_ratio)

    return {
        stratification_number = stratification,
        drawdown_depth_ratio = drawdown_ratio,
        stratification_retention_ratio = retention_ratio,
        community_refuge_volume_m3 = refuge.community_refuge_volume_m3,
        limiting_species = refuge.limiting_species,
        limiting_species_refuge_volume_m3 = refuge.limiting_species_refuge_volume_m3,
        refuge_ratio = refuge_ratio,
        corridor_status = refuge_ratio >= 1.0 and "safe" or "breached",
        knowledge_factor = clamp01(input.evidence_quality or 0.0),
        eco_impact_value = clamp01(refuge_ratio),
        harm_risk = harm_risk
    }
end

return LakeRefugePhysics
