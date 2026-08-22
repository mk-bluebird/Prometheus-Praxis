-- Lua 5.4
-- Phoenix restoration and urban-heat decision-support utilities:
-- 1) Fisher-KPP buffelgrass corridor-spread calibration and screening;
-- 2) urban-canyon effective-albedo / solar-access scenario comparison;
-- 3) 10 m thermal-comfort risk classification from supplied gridded inputs;
-- 4) cool-roof versus green-roof water-price and heat-risk comparison.
--
-- No function authorizes treatment, construction, irrigation, land access,
-- or ecological intervention. All input data must be independently reviewed.

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

local function average(values)
    assert(type(values) == "table" and #values > 0, "values must be non-empty")
    local total = 0
    for index, value in ipairs(values) do
        assert(finite(value), "values[" .. index .. "] must be finite")
        total = total + value
    end
    return total / #values
end

function M.fisher_kpp_speed(growth_rate_per_year, diffusion_m2_per_year)
    assert_nonnegative(growth_rate_per_year, "growth_rate_per_year")
    assert_nonnegative(diffusion_m2_per_year, "diffusion_m2_per_year")
    return 2 * math.sqrt(growth_rate_per_year * diffusion_m2_per_year)
end

function M.estimate_buffelgrass_diffusion(observations, growth_rate_per_year)
    assert(type(observations) == "table" and #observations >= 2,
        "at least two annual front observations are required")
    assert_positive(growth_rate_per_year, "growth_rate_per_year")

    local implied_diffusions = {}
    local speeds = {}

    for index, observation in ipairs(observations) do
        assert(type(observation) == "table", "observation must be a table")
        assert_positive(observation.interval_years, "interval_years")
        assert_nonnegative(observation.front_advance_m, "front_advance_m")
        assert_fraction(observation.detection_coverage, "detection_coverage")
        assert_fraction(observation.corridor_connectivity, "corridor_connectivity")

        local speed = observation.front_advance_m / observation.interval_years
        local adjusted_speed = speed / math.max(
            observation.detection_coverage * observation.corridor_connectivity,
            1e-9
        )

        speeds[index] = adjusted_speed
        implied_diffusions[index] = (adjusted_speed ^ 2)
            / (4 * growth_rate_per_year)
    end

    local diffusion = average(implied_diffusions)
    local mean_speed = average(speeds)

    local variance = 0
    for _, value in ipairs(implied_diffusions) do
        variance = variance + (value - diffusion) ^ 2
    end
    variance = variance / #implied_diffusions

    return {
        diffusion_m2_per_year = diffusion,
        diffusion_standard_deviation_m2_per_year = math.sqrt(variance),
        observed_adjusted_speed_m_per_year = mean_speed,
        implied_fisher_kpp_speed_m_per_year = M.fisher_kpp_speed(
            growth_rate_per_year,
            diffusion
        ),
        knowledge_factor = clamp(
            0.40 * math.min(1, #observations / 12)
                + 0.30 * average((function()
                    local coverage = {}
                    for i, value in ipairs(observations) do
                        coverage[i] = value.detection_coverage
                    end
                    return coverage
                end)())
                + 0.30 * average((function()
                    local connectivity = {}
                    for i, value in ipairs(observations) do
                        connectivity[i] = value.corridor_connectivity
                    end
                    return connectivity
                end)()),
            0,
            1
        ),
        eco_impact_value = 0,
        harm_risk = clamp(
            0.50 * math.min(1, math.sqrt(variance) / math.max(diffusion, 1e-9))
                + 0.30 * (1 - average((function()
                    local coverage = {}
                    for i, value in ipairs(observations) do
                        coverage[i] = value.detection_coverage
                    end
                    return coverage
                end)()))
                + 0.20,
            0,
            1
        ),
        limitation = "Fisher-KPP assumes homogeneous growth and random dispersal. Use separate models or covariates for seed transport by floods, roads, vehicles, wildlife, wind, treatment, and wildfire.",
    }
end

function M.screen_restoration_corridor_for_invasion(corridor, diffusion_m2_per_year)
    assert(type(corridor) == "table", "corridor must be a table")
    assert(type(corridor.id) == "string" and #corridor.id > 0, "corridor id required")
    assert_positive(diffusion_m2_per_year, "diffusion_m2_per_year")
    assert_nonnegative(corridor.buffelgrass_growth_rate_per_year,
        "buffelgrass_growth_rate_per_year")
    assert_nonnegative(corridor.distance_to_unmanaged_source_m,
        "distance_to_unmanaged_source_m")
    assert_positive(corridor.planning_horizon_years, "planning_horizon_years")
    assert_fraction(corridor.seed_source_control_coverage,
        "seed_source_control_coverage")
    assert_fraction(corridor.monitoring_coverage, "monitoring_coverage")
    assert_fraction(corridor.revegetation_competitiveness,
        "revegetation_competitiveness")
    assert_fraction(corridor.fire_break_readiness, "fire_break_readiness")

    local unmanaged_speed = M.fisher_kpp_speed(
        corridor.buffelgrass_growth_rate_per_year,
        diffusion_m2_per_year
    )

    local residual_spread_factor = (1 - corridor.seed_source_control_coverage)
        * (1 - corridor.revegetation_competitiveness)

    local expected_advance = unmanaged_speed
        * residual_spread_factor
        * corridor.planning_horizon_years

    local source_reach_ratio = expected_advance
        / math.max(corridor.distance_to_unmanaged_source_m, 1e-9)

    local eligible = source_reach_ratio < 0.5
        and corridor.monitoring_coverage >= 0.80
        and corridor.seed_source_control_coverage >= 0.75
        and corridor.fire_break_readiness >= 0.75

    return {
        id = corridor.id,
        unmanaged_spread_speed_m_per_year = unmanaged_speed,
        modeled_residual_advance_m = expected_advance,
        source_reach_ratio = source_reach_ratio,
        eligible_for_restoration_phase = eligible,
        required_controls = {
            "Map and treat upstream and adjacent seed sources before connectivity planting.",
            "Use repeated detection and removal over multiple growing seasons.",
            "Monitor wash, road, trail, and disturbed-edge pathways separately.",
            "Maintain native revegetation cover and fire-risk safeguards.",
        },
        knowledge_factor = clamp(
            0.45 * corridor.monitoring_coverage
                + 0.35 * corridor.seed_source_control_coverage
                + 0.20 * corridor.revegetation_competitiveness,
            0,
            1
        ),
        eco_impact_value = eligible and clamp(
            0.55 * corridor.revegetation_competitiveness
                + 0.45 * corridor.seed_source_control_coverage,
            0,
            1
        ) or 0,
        harm_risk = clamp(
            0.45 * math.min(1, source_reach_ratio)
                + 0.30 * (1 - corridor.seed_source_control_coverage)
                + 0.15 * (1 - corridor.monitoring_coverage)
                + 0.10 * (1 - corridor.fire_break_readiness),
            0,
            1
        ),
    }
end

function M.estimate_svf_from_aspect_ratio(height_to_width_ratio)
    assert_nonnegative(height_to_width_ratio, "height_to_width_ratio")

    -- Idealized infinitely long street canyon approximation.
    return math.sqrt(1 + height_to_width_ratio ^ 2) - height_to_width_ratio
end

function M.effective_canyon_albedo(surface_albedo, sky_view_factor)
    assert_fraction(surface_albedo, "surface_albedo")
    assert_fraction(sky_view_factor, "sky_view_factor")

    local denominator = 1 - (1 - surface_albedo) * (1 - sky_view_factor)
    assert(denominator > 0, "invalid canyon optical geometry")

    return clamp(surface_albedo / denominator, 0, 1)
end

function M.rank_canyon_geometry_scenarios(scenarios)
    assert(type(scenarios) == "table" and #scenarios > 0,
        "scenarios must be a non-empty array")

    local results = {}

    for index, scenario in ipairs(scenarios) do
        assert(type(scenario) == "table", "scenario must be a table")
        assert(type(scenario.id) == "string" and #scenario.id > 0,
            "scenario id required")
        assert_nonnegative(scenario.height_m, "height_m")
        assert_positive(scenario.width_m, "width_m")
        assert_fraction(scenario.surface_albedo, "surface_albedo")
        assert_fraction(scenario.winter_solar_access_fraction,
            "winter_solar_access_fraction")
        assert_fraction(scenario.minimum_winter_solar_access_fraction,
            "minimum_winter_solar_access_fraction")
        assert_fraction(scenario.ventilation_compatibility,
            "ventilation_compatibility")
        assert_fraction(scenario.night_sky_cooling_compatibility,
            "night_sky_cooling_compatibility")

        local aspect_ratio = scenario.height_m / scenario.width_m
        local svf = scenario.sky_view_factor
            or M.estimate_svf_from_aspect_ratio(aspect_ratio)

        assert_fraction(svf, "sky_view_factor")

        local effective_albedo = M.effective_canyon_albedo(
            scenario.surface_albedo,
            svf
        )

        local winter_access_ok = scenario.winter_solar_access_fraction
            >= scenario.minimum_winter_solar_access_fraction

        local priority = effective_albedo
            * scenario.ventilation_compatibility
            * scenario.night_sky_cooling_compatibility
            * (winter_access_ok and 1 or 0)

        results[index] = {
            id = scenario.id,
            height_to_width_ratio = aspect_ratio,
            sky_view_factor = svf,
            effective_albedo = effective_albedo,
            winter_solar_access_ok = winter_access_ok,
            geometry_priority = priority,
            knowledge_factor = clamp(
                0.40 * (scenario.geometry_validation_coverage or 0)
                    + 0.35 * (scenario.winter_solar_validation_coverage or 0)
                    + 0.25 * (scenario.microclimate_validation_coverage or 0),
                0,
                1
            ),
            eco_impact_value = clamp(
                0.60 * effective_albedo
                    + 0.20 * scenario.ventilation_compatibility
                    + 0.20 * scenario.night_sky_cooling_compatibility,
                0,
                1
            ),
            harm_risk = clamp(
                0.45 * (1 - scenario.winter_solar_access_fraction)
                    + 0.30 * (1 - scenario.ventilation_compatibility)
                    + 0.25 * (1 - scenario.night_sky_cooling_compatibility),
                0,
                1
            ),
        }
    end

    table.sort(results, function(left, right)
        return left.geometry_priority > right.geometry_priority
    end)

    return results
end

function M.classify_utci(utci_c)
    assert(finite(utci_c), "utci_c must be finite")

    if utci_c < 26 then
        return "no_heat_stress"
    elseif utci_c < 32 then
        return "moderate_heat_stress"
    elseif utci_c < 38 then
        return "strong_heat_stress"
    elseif utci_c < 46 then
        return "very_strong_heat_stress"
    end

    return "extreme_heat_stress"
end

function M.map_block_thermal_comfort(cells, options)
    assert(type(cells) == "table" and #cells > 0, "cells must be non-empty")
    assert(type(options) == "table", "options must be a table")
    assert_positive(options.reference_air_temperature_c, "reference_air_temperature_c")
    assert_positive(options.reference_wind_m_s, "reference_wind_m_s")
    assert_fraction(options.reference_relative_humidity,
        "reference_relative_humidity")

    local mapped = {}
    local dangerous_count = 0

    for index, cell in ipairs(cells) do
        assert(type(cell) == "table", "cell must be a table")
        assert(type(cell.id) == "string" and #cell.id > 0, "cell id required")
        assert(finite(cell.lst_c), "lst_c must be finite")
        assert_fraction(cell.shade_fraction, "shade_fraction")
        assert_nonnegative(cell.wind_m_s, "wind_m_s")
        assert_fraction(cell.relative_humidity, "relative_humidity")
        assert_fraction(cell.sky_view_factor, "sky_view_factor")

        -- A transparent screening approximation, not the official UTCI solver.
        local radiant_excess = (cell.lst_c - options.reference_air_temperature_c)
            * (1 - cell.shade_fraction)
            * (0.45 + 0.55 * cell.sky_view_factor)

        local wind_relief = math.min(
            6,
            1.8 * math.log(1 + cell.wind_m_s / options.reference_wind_m_s)
        )

        local humidity_load = 8
            * math.max(0, cell.relative_humidity - options.reference_relative_humidity)

        local screening_utci = options.reference_air_temperature_c
            + 0.35 * radiant_excess
            - wind_relief
            + humidity_load

        local category = M.classify_utci(screening_utci)
        local dangerous = category == "very_strong_heat_stress"
            or category == "extreme_heat_stress"

        if dangerous then
            dangerous_count = dangerous_count + 1
        end

        mapped[index] = {
            id = cell.id,
            grid_resolution_m = 10,
            screening_utci_c = screening_utci,
            heat_stress_category = category,
            remains_dangerous_after_sunset = dangerous,
            shade_fraction = cell.shade_fraction,
            wind_m_s = cell.wind_m_s,
            sky_view_factor = cell.sky_view_factor,
        }
    end

    return {
        cells = mapped,
        dangerous_cell_fraction = dangerous_count / #mapped,
        knowledge_factor = clamp(
            0.35 * (options.lst_validation_coverage or 0)
                + 0.30 * (options.shade_validation_coverage or 0)
                + 0.20 * (options.wind_validation_coverage or 0)
                + 0.15 * (options.humidity_validation_coverage or 0),
            0,
            1
        ),
        eco_impact_value = 0,
        harm_risk = clamp(
            0.50 * (dangerous_count / #mapped)
                + 0.50 * (1 - (options.field_validation_coverage or 0)),
            0,
            1
        ),
        limitation = "This is a screening index. Operational UTCI or PET mapping requires a validated radiation, air-temperature, humidity, and wind model at matching spatial and temporal resolution.",
    }
end

function M.compare_roof_options(input)
    assert(type(input) == "table", "input must be a table")
    assert_positive(input.roof_area_m2, "roof_area_m2")
    assert_positive(input.analysis_days, "analysis_days")
    assert_nonnegative(input.water_price_per_m3, "water_price_per_m3")
    assert_nonnegative(input.heat_risk_value_per_degree_day,
        "heat_risk_value_per_degree_day")
    assert_fraction(input.cool_roof.albedo, "cool_roof.albedo")
    assert_nonnegative(input.cool_roof.annualized_cost_per_m2,
        "cool_roof.annualized_cost_per_m2")
    assert_nonnegative(input.cool_roof.daytime_cooling_c,
        "cool_roof.daytime_cooling_c")
    assert_nonnegative(input.cool_roof.nighttime_cooling_c,
        "cool_roof.nighttime_cooling_c")
    assert_fraction(input.green_roof.albedo, "green_roof.albedo")
    assert_nonnegative(input.green_roof.annualized_cost_per_m2,
        "green_roof.annualized_cost_per_m2")
    assert_nonnegative(input.green_roof.irrigation_m3_per_m2_day,
        "green_roof.irrigation_m3_per_m2_day")
    assert_nonnegative(input.green_roof.daytime_cooling_c,
        "green_roof.daytime_cooling_c")
    assert_nonnegative(input.green_roof.nighttime_cooling_c,
        "green_roof.nighttime_cooling_c")
    assert_fraction(input.green_roof.establishment_reliability,
        "green_roof.establishment_reliability")
    assert_fraction(input.green_roof.habitat_value, "green_roof.habitat_value")

    local function evaluate_cool_roof()
        local direct_cost = input.cool_roof.annualized_cost_per_m2 * input.roof_area_m2
        local cooling = input.cool_roof.daytime_cooling_c
            + input.cool_roof.nighttime_cooling_c

        local risk_benefit = cooling
            * input.analysis_days
            * input.heat_risk_value_per_degree_day

        return {
            option = "cool_roof",
            annualized_direct_cost = direct_cost,
            irrigation_water_m3 = 0,
            modeled_cooling_c = cooling,
            heat_risk_benefit = risk_benefit,
            net_value = risk_benefit - direct_cost,
            knowledge_factor = 0.70,
            eco_impact_value = clamp(cooling / 5, 0, 1),
            harm_risk = 0.15,
        }
    end

    local function evaluate_green_roof()
        local irrigation_m3 = input.green_roof.irrigation_m3_per_m2_day
            * input.roof_area_m2
            * input.analysis_days

        local direct_cost = input.green_roof.annualized_cost_per_m2 * input.roof_area_m2
        local water_cost = irrigation_m3 * input.water_price_per_m3

        local cooling = (
            input.green_roof.daytime_cooling_c
            + input.green_roof.nighttime_cooling_c
        ) * input.green_roof.establishment_reliability

        local risk_benefit = cooling
            * input.analysis_days
            * input.heat_risk_value_per_degree_day

        local habitat_benefit = input.green_roof.habitat_value
            * input.heat_risk_value_per_degree_day
            * input.analysis_days

        return {
            option = "green_roof",
            annualized_direct_cost = direct_cost + water_cost,
            irrigation_water_m3 = irrigation_m3,
            modeled_cooling_c = cooling,
            heat_risk_benefit = risk_benefit,
            habitat_benefit = habitat_benefit,
            net_value = risk_benefit + habitat_benefit - direct_cost - water_cost,
            knowledge_factor = 0.55,
            eco_impact_value = clamp(
                0.55 * input.green_roof.habitat_value
                    + 0.45 * input.green_roof.establishment_reliability,
                0,
                1
            ),
            harm_risk = clamp(
                0.55 * (1 - input.green_roof.establishment_reliability)
                    + 0.45 * math.min(1, irrigation_m3 / 1000),
                0,
                1
            ),
        }
    end

    local cool = evaluate_cool_roof()
    local green = evaluate_green_roof()

    return {
        cool_roof = cool,
        green_roof = green,
        preferred_option = green.net_value > cool.net_value
            and "green_roof"
            or "cool_roof",
        green_roof_outperforms_when = {
            "water price and irrigation demand remain low enough that green-roof water cost does not erase cooling and habitat benefits",
            "establishment reliability remains high under local heat and maintenance conditions",
            "heat-risk and biodiversity benefits are explicitly valued above the cool roof's lower water demand and generally lower direct cost",
        },
        limitation = "This simplified comparison does not replace building-energy, structural-load, stormwater, maintenance, water-quality, or life-cycle assessment.",
    }
end

return M
