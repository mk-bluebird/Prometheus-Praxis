-- Lua 5.4
-- Phoenix heat-recovery decision-support utilities:
-- 1) stored-heat and nighttime-release comparison for surface replacement;
-- 2) ventilation scenario ranking with a dust-resuspension constraint;
-- 3) humidity-dependent evaporative-cooling screening.
--
-- All outputs are scenario estimates. They do not replace site microclimate
-- modeling, dust monitoring, building code review, public-health review,
-- hydrologic design, or community approval.

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
    assert(finite(value) and value >= 0 and value <= 1,
        name .. " must be within 0..1")
end

function M.stored_heat_joules(density_kg_m3, specific_heat_j_kg_k,
                              volume_m3, temperature_excess_k)
    assert_positive(density_kg_m3, "density_kg_m3")
    assert_positive(specific_heat_j_kg_k, "specific_heat_j_kg_k")
    assert_nonnegative(volume_m3, "volume_m3")
    assert_nonnegative(temperature_excess_k, "temperature_excess_k")

    return density_kg_m3
        * specific_heat_j_kg_k
        * volume_m3
        * temperature_excess_k
end

function M.compare_surface_heat_release(input)
    assert(type(input) == "table", "input must be a table")
    assert_positive(input.area_m2, "area_m2")
    assert_positive(input.thickness_m, "thickness_m")
    assert_positive(input.release_duration_hours, "release_duration_hours")
    assert_fraction(input.legacy.release_fraction_overnight,
        "legacy.release_fraction_overnight")
    assert_fraction(input.replacement.release_fraction_overnight,
        "replacement.release_fraction_overnight")

    local volume_m3 = input.area_m2 * input.thickness_m

    local legacy_q = M.stored_heat_joules(
        input.legacy.density_kg_m3,
        input.legacy.specific_heat_j_kg_k,
        volume_m3,
        input.legacy.daytime_temperature_excess_k
    )

    local replacement_q = M.stored_heat_joules(
        input.replacement.density_kg_m3,
        input.replacement.specific_heat_j_kg_k,
        volume_m3,
        input.replacement.daytime_temperature_excess_k
    )

    local legacy_released_j = legacy_q * input.legacy.release_fraction_overnight
    local replacement_released_j = replacement_q
        * input.replacement.release_fraction_overnight

    local avoided_release_j = math.max(0, legacy_released_j - replacement_released_j)
    local duration_seconds = input.release_duration_hours * 3600

    return {
        area_m2 = input.area_m2,
        modeled_legacy_stored_heat_mj = legacy_q / 1e6,
        modeled_replacement_stored_heat_mj = replacement_q / 1e6,
        modeled_legacy_overnight_release_mj = legacy_released_j / 1e6,
        modeled_replacement_overnight_release_mj = replacement_released_j / 1e6,
        modeled_avoided_overnight_release_mj = avoided_release_j / 1e6,
        mean_avoided_release_power_w = avoided_release_j / duration_seconds,
        knowledge_factor = clamp(
            0.40 * (input.surface_temperature_validation or 0)
                + 0.30 * (input.material_property_validation or 0)
                + 0.30 * (input.nighttime_air_temperature_validation or 0),
            0,
            1
        ),
        eco_impact_value = clamp(
            avoided_release_j / math.max(legacy_released_j, 1e-12),
            0,
            1
        ),
        harm_risk = clamp(
            0.45 * (1 - (input.drainage_compatibility or 0))
                + 0.30 * (1 - (input.maintenance_feasibility or 0))
                + 0.25 * (1 - (input.surface_temperature_validation or 0)),
            0,
            1
        ),
        limitation = "This estimates stored and released heat, not a direct air-temperature reduction. Couple it to local radiative, aerodynamic, subsurface, drainage, and traffic-safety measurements before design use.",
    }
end

function M.log_wind_speed(friction_velocity_m_s, height_m,
                          displacement_height_m, roughness_length_m)
    assert_positive(friction_velocity_m_s, "friction_velocity_m_s")
    assert_positive(height_m, "height_m")
    assert_nonnegative(displacement_height_m, "displacement_height_m")
    assert_positive(roughness_length_m, "roughness_length_m")
    assert(height_m > displacement_height_m + roughness_length_m,
        "height must exceed displacement height plus roughness length")

    local von_karman = 0.41
    return friction_velocity_m_s / von_karman
        * math.log((height_m - displacement_height_m) / roughness_length_m)
end

function M.rank_ventilation_arrangements(arrangements, forcing)
    assert(type(arrangements) == "table" and #arrangements > 0,
        "arrangements must be a non-empty array")
    assert(type(forcing) == "table", "forcing must be a table")
    assert_positive(forcing.reference_height_m, "reference_height_m")
    assert_positive(forcing.dust_sensitive_speed_m_s, "dust_sensitive_speed_m_s")
    assert_fraction(forcing.dust_surface_stability, "dust_surface_stability")

    local results = {}

    for index, item in ipairs(arrangements) do
        assert(type(item) == "table", "arrangement must be a table")
        assert(type(item.id) == "string" and #item.id > 0, "arrangement id required")
        assert_positive(item.friction_velocity_m_s, "friction_velocity_m_s")
        assert_nonnegative(item.displacement_height_m, "displacement_height_m")
        assert_positive(item.roughness_length_m, "roughness_length_m")
        assert_fraction(item.crosswind_gap_fraction, "crosswind_gap_fraction")
        assert_fraction(item.ground_level_permeability, "ground_level_permeability")
        assert_fraction(item.shade_compatibility, "shade_compatibility")
        assert_fraction(item.pedestrian_wind_safety, "pedestrian_wind_safety")

        local wind_speed = M.log_wind_speed(
            item.friction_velocity_m_s,
            forcing.reference_height_m,
            item.displacement_height_m,
            item.roughness_length_m
        )

        local dust_exposure = clamp(
            wind_speed / forcing.dust_sensitive_speed_m_s
                * (1 - forcing.dust_surface_stability),
            0,
            1
        )

        local ventilation_score = clamp(
            wind_speed / forcing.dust_sensitive_speed_m_s,
            0,
            1
        ) * item.crosswind_gap_fraction
            * item.ground_level_permeability
            * item.pedestrian_wind_safety

        local priority = ventilation_score
            * item.shade_compatibility
            * (1 - dust_exposure)

        results[index] = {
            id = item.id,
            estimated_wind_speed_m_s = wind_speed,
            dust_resuspension_screening_risk = dust_exposure,
            ventilation_score = ventilation_score,
            arrangement_priority = priority,
            suitable_for_further_analysis = dust_exposure <= 0.35
                and item.pedestrian_wind_safety >= 0.70,
            knowledge_factor = clamp(
                0.40 * (item.wind_measurement_coverage or 0)
                    + 0.30 * (item.dust_measurement_coverage or 0)
                    + 0.30 * (item.geometry_validation_coverage or 0),
                0,
                1
            ),
            eco_impact_value = clamp(
                0.50 * ventilation_score
                    + 0.30 * item.shade_compatibility
                    + 0.20 * item.ground_level_permeability,
                0,
                1
            ),
            harm_risk = clamp(
                0.60 * dust_exposure
                    + 0.25 * (1 - item.pedestrian_wind_safety)
                    + 0.15 * (1 - item.shade_compatibility),
                0,
                1
            ),
        }
    end

    table.sort(results, function(left, right)
        return left.arrangement_priority > right.arrangement_priority
    end)

    return results
end

function M.evaporation_potential(relative_humidity_fraction)
    assert_fraction(relative_humidity_fraction, "relative_humidity_fraction")
    return 1 - relative_humidity_fraction
end

function M.screen_evaporative_cooling(input)
    assert(type(input) == "table", "input must be a table")
    assert_fraction(input.relative_humidity_fraction, "relative_humidity_fraction")
    assert_positive(input.maximum_dry_air_cooling_c, "maximum_dry_air_cooling_c")
    assert_nonnegative(input.water_l_per_hour, "water_l_per_hour")
    assert_nonnegative(input.people_served, "people_served")
    assert_fraction(input.shade_fraction, "shade_fraction")
    assert_fraction(input.water_quality_suitability, "water_quality_suitability")
    assert_fraction(input.drainage_compatibility, "drainage_compatibility")

    local potential = M.evaporation_potential(input.relative_humidity_fraction)
    local modeled_cooling = input.maximum_dry_air_cooling_c * potential

    local category
    if potential >= 0.70 then
        category = "high_evaporative_potential"
    elseif potential >= 0.45 then
        category = "moderate_evaporative_potential"
    elseif potential >= 0.25 then
        category = "limited_evaporative_potential"
    else
        category = "ineffective_without_additional_shade_or_alternative_heat_controls"
    end

    local recommended = potential >= 0.45
        and input.shade_fraction >= 0.50
        and input.water_quality_suitability >= 0.80
        and input.drainage_compatibility >= 0.80

    return {
        relative_humidity_percent = input.relative_humidity_fraction * 100,
        evaporation_potential = potential,
        modeled_temperature_reduction_c = modeled_cooling,
        effectiveness_category = category,
        suitable_for_further_design = recommended,
        operational_screening_threshold_rh_percent = 55,
        ineffective_screening_threshold_rh_percent = 75,
        water_l_per_person_hour = input.water_l_per_hour
            / math.max(input.people_served, 1),
        knowledge_factor = clamp(
            0.35 * (input.local_rh_measurement_coverage or 0)
                + 0.25 * (input.local_wind_measurement_coverage or 0)
                + 0.20 * input.water_quality_suitability
                + 0.20 * input.drainage_compatibility,
            0,
            1
        ),
        eco_impact_value = clamp(
            0.55 * input.shade_fraction
                + 0.45 * potential,
            0,
            1
        ),
        harm_risk = clamp(
            0.35 * (1 - input.water_quality_suitability)
                + 0.30 * (1 - input.drainage_compatibility)
                + 0.20 * (1 - input.shade_fraction)
                + 0.15 * (1 - potential),
            0,
            1
        ),
        limitation = "The humidity thresholds are conservative screening values, not universal physical cutoffs. Actual performance also depends on dry-bulb temperature, wet-bulb temperature, droplet size, solar exposure, wind, water use, and whether added humidity impairs human sweat evaporation.",
    }
end

return M
