-- Lua 5.4
-- Phoenix heat-equity and shade-planning decision-support utilities:
-- 1) bias-aware heat-vulnerability-index calibration;
-- 2) tree-shadow geometry and schoolyard placement ranking;
-- 3) separate daytime/nighttime LST-to-air-temperature regression fitting.
--
-- This module is for transparent scenario analysis. It must not be used as the
-- sole basis for medical triage, denial of services, enforcement, land-use
-- decisions, or allocation of public protection without community review and
-- independent validation.

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

local function solve_linear_system(matrix, vector)
    local n = #matrix
    local augmented = {}

    for row = 1, n do
        augmented[row] = {}
        for column = 1, n do
            augmented[row][column] = matrix[row][column]
        end
        augmented[row][n + 1] = vector[row]
    end

    for pivot_column = 1, n do
        local pivot_row = pivot_column
        local pivot_value = math.abs(augmented[pivot_row][pivot_column])

        for candidate = pivot_column + 1, n do
            local value = math.abs(augmented[candidate][pivot_column])
            if value > pivot_value then
                pivot_row = candidate
                pivot_value = value
            end
        end

        if pivot_value < 1e-12 then
            return nil, "singular regression matrix"
        end

        augmented[pivot_column], augmented[pivot_row] =
            augmented[pivot_row], augmented[pivot_column]

        local pivot = augmented[pivot_column][pivot_column]
        for column = pivot_column, n + 1 do
            augmented[pivot_column][column] =
                augmented[pivot_column][column] / pivot
        end

        for row = 1, n do
            if row ~= pivot_column then
                local multiplier = augmented[row][pivot_column]
                for column = pivot_column, n + 1 do
                    augmented[row][column] = augmented[row][column]
                        - multiplier * augmented[pivot_column][column]
                end
            end
        end
    end

    local solution = {}
    for row = 1, n do
        solution[row] = augmented[row][n + 1]
    end

    return solution
end

local function weighted_ridge_regression(rows, targets, weights, ridge_lambda)
    assert(type(rows) == "table" and #rows > 0, "rows must be non-empty")
    assert(#rows == #targets and #rows == #weights,
        "rows, targets, and weights must have equal length")
    assert_nonnegative(ridge_lambda, "ridge_lambda")

    local p = #rows[1]
    local xtwx = {}
    local xtwy = {}

    for i = 1, p do
        xtwx[i] = {}
        xtwy[i] = 0
        for j = 1, p do
            xtwx[i][j] = 0
        end
    end

    for row_index, row in ipairs(rows) do
        assert(#row == p, "inconsistent feature width")
        assert(finite(targets[row_index]), "target must be finite")
        assert_positive(weights[row_index], "weight must be positive")

        for i = 1, p do
            assert(finite(row[i]), "feature must be finite")
            xtwy[i] = xtwy[i] + weights[row_index] * row[i] * targets[row_index]

            for j = 1, p do
                xtwx[i][j] = xtwx[i][j]
                    + weights[row_index] * row[i] * row[j]
            end
        end
    end

    for i = 2, p do
        xtwx[i][i] = xtwx[i][i] + ridge_lambda
    end

    return solve_linear_system(xtwx, xtwy)
end

function M.normalize_hvi_weights(weights)
    assert(type(weights) == "table", "weights must be a table")
    assert_nonnegative(weights.exposure, "weights.exposure")
    assert_nonnegative(weights.sensitivity, "weights.sensitivity")
    assert_nonnegative(weights.adaptive_capacity, "weights.adaptive_capacity")

    local total = weights.exposure + weights.sensitivity + weights.adaptive_capacity
    assert(total > 0, "at least one HVI weight must be positive")

    return {
        exposure = weights.exposure / total,
        sensitivity = weights.sensitivity / total,
        adaptive_capacity = weights.adaptive_capacity / total,
    }
end

function M.compute_hvi(tract, weights)
    assert(type(tract) == "table", "tract must be a table")
    assert_fraction(tract.exposure, "tract.exposure")
    assert_fraction(tract.sensitivity, "tract.sensitivity")
    assert_fraction(tract.adaptive_capacity, "tract.adaptive_capacity")

    local w = M.normalize_hvi_weights(weights)
    local raw = w.exposure * tract.exposure
        + w.sensitivity * tract.sensitivity
        - w.adaptive_capacity * tract.adaptive_capacity

    return clamp(raw, 0, 1)
end

function M.calibrate_hvi_bias_aware(records, options)
    assert(type(records) == "table" and #records >= 12,
        "at least twelve records are required")
    assert(type(options) == "table", "options must be a table")
    assert_nonnegative(options.ridge_lambda or 0.1, "ridge_lambda")
    assert_fraction(options.minimum_reporting_completeness or 0.75,
        "minimum_reporting_completeness")

    local rows = {}
    local targets = {}
    local weights = {}
    local retained = 0
    local excluded = 0

    for _, record in ipairs(records) do
        assert(type(record) == "table", "record must be a table")
        assert_fraction(record.exposure, "record.exposure")
        assert_fraction(record.sensitivity, "record.sensitivity")
        assert_fraction(record.adaptive_capacity, "record.adaptive_capacity")
        assert_nonnegative(record.heat_illness_rate, "record.heat_illness_rate")
        assert_fraction(record.reporting_completeness, "record.reporting_completeness")
        assert_fraction(record.population_coverage, "record.population_coverage")
        assert_fraction(record.healthcare_access_proxy, "record.healthcare_access_proxy")

        if record.reporting_completeness >= options.minimum_reporting_completeness
            and record.population_coverage > 0 then

            retained = retained + 1
            rows[retained] = {
                1,
                record.exposure,
                record.sensitivity,
                -record.adaptive_capacity,
            }

            targets[retained] = record.heat_illness_rate

            -- Downweight records likely distorted by incomplete reporting,
            -- undercoverage, or unequal clinical access. Do not use protected
            -- characteristics as a penalty or a proxy for deservingness.
            weights[retained] = math.max(
                1e-6,
                record.reporting_completeness
                    * record.population_coverage
                    * (0.50 + 0.50 * record.healthcare_access_proxy)
            )
        else
            excluded = excluded + 1
        end
    end

    assert(retained >= 12, "insufficient retained records after quality filtering")

    local coefficients, err = weighted_ridge_regression(
        rows,
        targets,
        weights,
        options.ridge_lambda or 0.1
    )
    if not coefficients then
        return nil, err
    end

    local positive_weights = {
        exposure = math.max(0, coefficients[2]),
        sensitivity = math.max(0, coefficients[3]),
        adaptive_capacity = math.max(0, -coefficients[4]),
    }

    local normalized = M.normalize_hvi_weights(positive_weights)

    return {
        intercept = coefficients[1],
        raw_coefficients = {
            exposure = coefficients[2],
            sensitivity = coefficients[3],
            adaptive_capacity = coefficients[4],
        },
        calibrated_weights = normalized,
        retained_records = retained,
        excluded_low_quality_records = excluded,
        fairness_controls = {
            "Use geographically separated and time-separated validation folds.",
            "Audit residuals by reporting completeness, healthcare access, age structure, disability prevalence, language access, housing insecurity, and neighborhood.",
            "Treat hospital data as an incomplete outcome signal; integrate EMS, mortality, syndromic, survey, cooling-center, and community-partner evidence where lawful and privacy-protective.",
            "Do not use protected traits as negative weight multipliers or as a proxy for lower priority.",
            "Publish uncertainty, missingness, and disagreement maps alongside HVI maps.",
            "Use the index to target additional protection and outreach, not to exclude any tract from assistance.",
        },
        knowledge_factor = clamp(
            0.40 * math.min(1, retained / 100)
                + 0.30 * (options.independent_outcome_coverage or 0)
                + 0.30 * (options.community_validation_coverage or 0),
            0,
            1
        ),
        eco_impact_value = 0,
        harm_risk = clamp(
            0.35 * (excluded / #records)
                + 0.35 * (1 - (options.independent_outcome_coverage or 0))
                + 0.30 * (1 - (options.community_validation_coverage or 0)),
            0,
            1
        ),
        limitation = "Hospitalizations reflect illness, access, coding, transport, insurance, and care-seeking—not total community heat burden. The result is a calibration hypothesis requiring independent fairness and outcome validation.",
    }
end

function M.shadow_length(tree_height_m, solar_elevation_degrees)
    assert_positive(tree_height_m, "tree_height_m")
    assert(solar_elevation_degrees > 0 and solar_elevation_degrees < 90,
        "solar_elevation_degrees must be in (0, 90)")

    local radians = solar_elevation_degrees * math.pi / 180
    return tree_height_m / math.tan(radians)
end

function M.rank_schoolyard_shade_options(options)
    assert(type(options) == "table" and #options > 0,
        "options must be a non-empty array")

    local ranked = {}

    for index, option in ipairs(options) do
        assert(type(option) == "table", "option must be a table")
        assert(type(option.id) == "string" and #option.id > 0, "option id required")
        assert_positive(option.tree_height_m, "tree_height_m")
        assert_positive(option.canopy_width_m, "canopy_width_m")
        assert_positive(option.distance_to_target_m, "distance_to_target_m")
        assert_fraction(option.canopy_density, "canopy_density")
        assert_fraction(option.peak_uv_occupancy_fraction,
            "peak_uv_occupancy_fraction")
        assert_fraction(option.irrigation_reliability, "irrigation_reliability")
        assert_fraction(option.establishment_survival, "establishment_survival")
        assert_fraction(option.root_zone_compatibility, "root_zone_compatibility")
        assert_fraction(option.habitat_value, "habitat_value")
        assert(type(option.peak_solar_elevations_degrees) == "table"
            and #option.peak_solar_elevations_degrees > 0,
            "peak_solar_elevations_degrees must be non-empty")

        local shadow_lengths = {}
        local target_covered_count = 0

        for hour, elevation in ipairs(option.peak_solar_elevations_degrees) do
            local shadow = M.shadow_length(option.tree_height_m, elevation)
            shadow_lengths[hour] = shadow

            if shadow + option.canopy_width_m / 2 >= option.distance_to_target_m then
                target_covered_count = target_covered_count + 1
            end
        end

        local temporal_coverage = target_covered_count / #shadow_lengths
        local effective_shade = temporal_coverage
            * option.canopy_density
            * option.peak_uv_occupancy_fraction

        local priority = effective_shade
            * option.irrigation_reliability
            * option.establishment_survival
            * option.root_zone_compatibility
            * (0.80 + 0.20 * option.habitat_value)

        ranked[index] = {
            id = option.id,
            shadow_lengths_m = shadow_lengths,
            peak_period_target_coverage_fraction = temporal_coverage,
            effective_peak_uv_shade_score = effective_shade,
            schoolyard_shade_priority = priority,
            knowledge_factor = clamp(
                0.35 * (option.sun_path_validation_coverage or 0)
                    + 0.30 * (option.canopy_measurement_coverage or 0)
                    + 0.20 * option.irrigation_reliability
                    + 0.15 * option.establishment_survival,
                0,
                1
            ),
            eco_impact_value = clamp(
                0.60 * effective_shade
                    + 0.40 * option.habitat_value,
                0,
                1
            ),
            harm_risk = clamp(
                0.40 * (1 - option.establishment_survival)
                    + 0.30 * (1 - option.irrigation_reliability)
                    + 0.20 * (1 - option.root_zone_compatibility)
                    + 0.10 * (1 - temporal_coverage),
                0,
                1
            ),
            limitation = "The calculation gives geometric shadow reach. Verify solar azimuth, canopy asymmetry, growth trajectory, root conflicts, sightlines, irrigation, maintenance, and built-shade needs with a site plan.",
        }
    end

    table.sort(ranked, function(left, right)
        return left.schoolyard_shade_priority > right.schoolyard_shade_priority
    end)

    return ranked
end

function M.fit_lst_air_model(observations, ridge_lambda)
    assert(type(observations) == "table" and #observations >= 8,
        "at least eight observations are required")
    assert_nonnegative(ridge_lambda or 0.1, "ridge_lambda")

    local rows = {}
    local targets = {}
    local weights = {}

    for index, observation in ipairs(observations) do
        assert(type(observation) == "table", "observation must be a table")
        assert(finite(observation.lst_c), "lst_c must be finite")
        assert(finite(observation.air_temperature_c), "air_temperature_c must be finite")
        assert(finite(observation.ndvi), "ndvi must be finite")
        assert_fraction(observation.impervious_fraction, "impervious_fraction")
        assert_fraction(observation.quality_weight or 1, "quality_weight")

        rows[index] = {
            1,
            observation.air_temperature_c,
            observation.ndvi,
            observation.impervious_fraction,
        }

        targets[index] = observation.lst_c
        weights[index] = math.max(1e-6, observation.quality_weight or 1)
    end

    local coefficients, err = weighted_ridge_regression(
        rows,
        targets,
        weights,
        ridge_lambda or 0.1
    )
    if not coefficients then
        return nil, err
    end

    local squared_error = 0
    for index, row in ipairs(rows) do
        local predicted = 0
        for coefficient_index, coefficient in ipairs(coefficients) do
            predicted = predicted + coefficient * row[coefficient_index]
        end
        squared_error = squared_error + (targets[index] - predicted) ^ 2
    end

    local rmse = math.sqrt(squared_error / #rows)

    return {
        equation = "LST = a + b*T_air + c*NDVI + d*impervious_fraction",
        a = coefficients[1],
        b = coefficients[2],
        c = coefficients[3],
        d = coefficients[4],
        rmse_c = rmse,
        observation_count = #observations,
    }
end

function M.fit_day_night_lst_air_models(day_observations, night_observations, options)
    assert(type(options) == "table", "options must be a table")

    local day_model, day_error = M.fit_lst_air_model(
        day_observations,
        options.ridge_lambda or 0.1
    )
    if not day_model then
        return nil, day_error
    end

    local night_model, night_error = M.fit_lst_air_model(
        night_observations,
        options.ridge_lambda or 0.1
    )
    if not night_model then
        return nil, night_error
    end

    return {
        daytime = day_model,
        nighttime = night_model,
        calibration_protocol = {
            "Fit daytime and nighttime observations independently; do not force equal coefficients.",
            "Match satellite acquisition times to station or mobile-traverse observations within a documented tolerance.",
            "Use cloud, emissivity, terrain-shadow, and sensor-quality masks.",
            "Spatially block cross-validation by neighborhood and temporally hold out heat events.",
            "Test residuals against land cover, canopy, imperviousness, elevation, sky-view factor, irrigation, and station siting.",
            "Report LST and air-temperature maps separately; do not treat one as a direct substitute for the other.",
        },
        knowledge_factor = clamp(
            0.30 * math.min(1, (#day_observations + #night_observations) / 200)
                + 0.35 * (options.station_coverage or 0)
                + 0.20 * (options.mobile_transect_coverage or 0)
                + 0.15 * (options.cross_validation_coverage or 0),
            0,
            1
        ),
        eco_impact_value = 0,
        harm_risk = clamp(
            0.35 * math.min(1, (day_model.rmse_c + night_model.rmse_c) / 8)
                + 0.35 * (1 - (options.station_coverage or 0))
                + 0.30 * (1 - (options.cross_validation_coverage or 0)),
            0,
            1
        ),
        limitation = "A four-variable linear model is a transparent baseline. Humidity, wind, emissivity, irrigation, landform, building geometry, and temporal mismatch may require additional predictors or nonlinear models.",
    }
end

return M
