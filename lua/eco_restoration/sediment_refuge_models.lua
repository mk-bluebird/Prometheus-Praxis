local SedimentRefugeModels = {}

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

local function bounded(value, name, lower, upper)
    finite(value, name)
    assert(value >= lower and value <= upper, name .. " must be within permitted bounds")
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

function SedimentRefugeModels.sod_temperature_adjusted(input)
    assert(type(input) == "table", "input must be a table")

    local sod_reference = nonnegative(input.sod_reference_mg_m2_h, "sod_reference_mg_m2_h")
    local q10 = positive(input.q10, "q10")
    local temperature_c = finite(input.temperature_c, "temperature_c")
    local reference_temperature_c = finite(input.reference_temperature_c, "reference_temperature_c")
    local organic_fraction = bounded(input.organic_fraction, "organic_fraction", 0.0, 1.0)
    local organic_multiplier = nonnegative(input.organic_multiplier, "organic_multiplier")
    local contaminant_inhibition_fraction = bounded(
        input.contaminant_inhibition_fraction or 0.0,
        "contaminant_inhibition_fraction",
        0.0,
        1.0
    )

    local organic_factor = 1.0 + organic_multiplier * organic_fraction
    local temperature_factor = q10 ^ ((temperature_c - reference_temperature_c) / 10.0)
    local contaminant_factor = 1.0 - contaminant_inhibition_fraction

    return sod_reference * temperature_factor * organic_factor * contaminant_factor
end

function SedimentRefugeModels.estimate_q10(sod_at_t1, temperature_t1_c, sod_at_t2, temperature_t2_c)
    positive(sod_at_t1, "sod_at_t1")
    positive(sod_at_t2, "sod_at_t2")
    finite(temperature_t1_c, "temperature_t1_c")
    finite(temperature_t2_c, "temperature_t2_c")

    local delta_temperature = temperature_t2_c - temperature_t1_c
    assert(math.abs(delta_temperature) > 1e-9,
        "temperatures must differ to estimate Q10")

    return (sod_at_t2 / sod_at_t1) ^ (10.0 / delta_temperature)
end

function SedimentRefugeModels.advection_dispersion_kernel(distance_m, elapsed_seconds, velocity_m_s,
                                                          dispersion_m2_s, decay_per_s)
    finite(distance_m, "distance_m")
    positive(elapsed_seconds, "elapsed_seconds")
    finite(velocity_m_s, "velocity_m_s")
    positive(dispersion_m2_s, "dispersion_m2_s")
    nonnegative(decay_per_s, "decay_per_s")

    local denominator = math.sqrt(4.0 * math.pi * dispersion_m2_s * elapsed_seconds)
    local centered_distance = distance_m - velocity_m_s * elapsed_seconds
    local exponent = -(
        centered_distance * centered_distance
    ) / (4.0 * dispersion_m2_s * elapsed_seconds) - decay_per_s * elapsed_seconds

    return math.exp(exponent) / denominator
end

function SedimentRefugeModels.periodic_reversal_concentration(input)
    assert(type(input) == "table", "input must be a table")

    local distance_m = finite(input.distance_m, "distance_m")
    local elapsed_seconds = positive(input.elapsed_seconds, "elapsed_seconds")
    local velocity_m_s = positive(input.velocity_m_s, "velocity_m_s")
    local reversal_period_seconds = positive(
        input.reversal_period_seconds,
        "reversal_period_seconds"
    )
    local dispersion_m2_s = positive(input.dispersion_m2_s, "dispersion_m2_s")
    local decay_per_s = nonnegative(input.decay_per_s, "decay_per_s")
    local impulse_mass_per_m2 = nonnegative(
        input.impulse_mass_per_m2,
        "impulse_mass_per_m2"
    )
    local image_count = input.image_count or 12

    assert(type(image_count) == "number" and image_count == math.floor(image_count)
        and image_count >= 0 and image_count <= 200,
        "image_count must be an integer within 0..200")

    local concentration = 0.0

    for n = -image_count, image_count do
        local shifted_time = elapsed_seconds + math.abs(n) * reversal_period_seconds
        local direction = (n % 2 == 0) and 1.0 or -1.0
        local kernel = SedimentRefugeModels.advection_dispersion_kernel(
            distance_m,
            shifted_time,
            direction * velocity_m_s,
            dispersion_m2_s,
            decay_per_s
        )
        concentration = concentration + impulse_mass_per_m2 * kernel
    end

    return concentration
end

function SedimentRefugeModels.resuspension_source(sediment_mass_flux_kg_m2_s,
                                                   desorbable_fraction,
                                                   porewater_concentration_ng_kg,
                                                   water_depth_m)
    nonnegative(sediment_mass_flux_kg_m2_s, "sediment_mass_flux_kg_m2_s")
    bounded(desorbable_fraction, "desorbable_fraction", 0.0, 1.0)
    nonnegative(porewater_concentration_ng_kg, "porewater_concentration_ng_kg")
    positive(water_depth_m, "water_depth_m")

    return (
        sediment_mass_flux_kg_m2_s
        * desorbable_fraction
        * porewater_concentration_ng_kg
    ) / water_depth_m
end

function SedimentRefugeModels.refuge_volume_from_bathymetry(cells, water_level_m,
                                                            minimum_refuge_depth_m)
    assert(type(cells) == "table" and #cells > 0, "cells must be non-empty")
    finite(water_level_m, "water_level_m")
    positive(minimum_refuge_depth_m, "minimum_refuge_depth_m")

    local volume_m3 = 0.0
    local connected_area_m2 = 0.0

    for _, cell in ipairs(cells) do
        assert(type(cell) == "table", "bathymetry cell must be a table")
        finite(cell.bed_elevation_m, "bed_elevation_m")
        positive(cell.area_m2, "area_m2")

        local depth_m = water_level_m - cell.bed_elevation_m
        if depth_m >= minimum_refuge_depth_m then
            volume_m3 = volume_m3 + depth_m * cell.area_m2
            connected_area_m2 = connected_area_m2 + cell.area_m2
        end
    end

    return {
        refuge_volume_m3 = volume_m3,
        refuge_area_m2 = connected_area_m2
    }
end

function SedimentRefugeModels.refuge_sensitivity(cells, water_level_m,
                                                 minimum_refuge_depth_m, delta_h_m)
    positive(delta_h_m, "delta_h_m")

    local current = SedimentRefugeModels.refuge_volume_from_bathymetry(
        cells,
        water_level_m,
        minimum_refuge_depth_m
    )
    local lower = SedimentRefugeModels.refuge_volume_from_bathymetry(
        cells,
        water_level_m - delta_h_m,
        minimum_refuge_depth_m
    )

    local derivative = (
        current.refuge_volume_m3 - lower.refuge_volume_m3
    ) / delta_h_m

    local area_loss_rate = (
        current.refuge_area_m2 - lower.refuge_area_m2
    ) / delta_h_m

    return {
        refuge_volume_m3 = current.refuge_volume_m3,
        refuge_area_m2 = current.refuge_area_m2,
        d_refuge_volume_d_h_m2 = derivative,
        d_refuge_area_d_h_m = area_loss_rate,
        connectivity_warning = lower.refuge_area_m2 == 0.0
            or lower.refuge_volume_m3 == 0.0
    }
end

function SedimentRefugeModels.assess(input)
    assert(type(input) == "table", "input must be a table")

    local sod = SedimentRefugeModels.sod_temperature_adjusted(input.sod)
    local refuge = SedimentRefugeModels.refuge_sensitivity(
        input.bathymetry_cells,
        input.water_level_m,
        input.minimum_refuge_depth_m,
        input.delta_h_m
    )

    local maximum_sod = positive(input.maximum_sod_mg_m2_h, "maximum_sod_mg_m2_h")
    local minimum_refuge_volume = positive(
        input.minimum_refuge_volume_m3,
        "minimum_refuge_volume_m3"
    )

    local sod_safe = sod <= maximum_sod
    local refuge_safe = refuge.refuge_volume_m3 >= minimum_refuge_volume

    return {
        sediment_oxygen_demand_mg_m2_h = sod,
        refuge_volume_m3 = refuge.refuge_volume_m3,
        refuge_area_m2 = refuge.refuge_area_m2,
        d_refuge_volume_d_h_m2 = refuge.d_refuge_volume_d_h_m2,
        connectivity_warning = refuge.connectivity_warning,
        corridor_status = sod_safe and refuge_safe and not refuge.connectivity_warning
            and "safe" or "breached",
        knowledge_factor = clamp01(input.evidence_quality or 0.0),
        eco_impact_value = sod_safe and refuge_safe and 1.0 or 0.0,
        harm_risk = clamp01(math.max(
            sod / maximum_sod - 1.0,
            1.0 - refuge.refuge_volume_m3 / minimum_refuge_volume
        ))
    }
end

return SedimentRefugeModels
