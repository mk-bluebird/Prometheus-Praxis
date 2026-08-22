-- Lua 5.4
-- Phoenix ecological-restoration decision-support utilities:
-- 1) hex-graph effective resistance and stepping-stone parcel ranking;
-- 2) heavy-tailed pollinator dispersal-kernel fitting;
-- 3) ephemeral-wash restoration screening using Shields stress and
--    conservative flood, erosion, and habitat-corridor criteria.
--
-- Outputs are planning rankings only. They do not authorize land acquisition,
-- construction, flood-control changes, or environmental clearance decisions.

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

local function copy_array(values)
    local result = {}
    for index, value in ipairs(values) do
        result[index] = value
    end
    return result
end

local function identity_matrix(size)
    local matrix = {}
    for row = 1, size do
        matrix[row] = {}
        for column = 1, size do
            matrix[row][column] = row == column and 1 or 0
        end
    end
    return matrix
end

local function matrix_copy(matrix)
    local result = {}
    for row = 1, #matrix do
        result[row] = {}
        for column = 1, #matrix[row] do
            result[row][column] = matrix[row][column]
        end
    end
    return result
end

local function invert_matrix(matrix)
    local size = #matrix
    local left = matrix_copy(matrix)
    local right = identity_matrix(size)

    for pivot_column = 1, size do
        local pivot_row = pivot_column
        local pivot_abs = math.abs(left[pivot_row][pivot_column])

        for candidate = pivot_column + 1, size do
            local candidate_abs = math.abs(left[candidate][pivot_column])
            if candidate_abs > pivot_abs then
                pivot_row = candidate
                pivot_abs = candidate_abs
            end
        end

        if pivot_abs < 1e-12 then
            return nil, "matrix is singular or numerically ill-conditioned"
        end

        left[pivot_column], left[pivot_row] = left[pivot_row], left[pivot_column]
        right[pivot_column], right[pivot_row] = right[pivot_row], right[pivot_column]

        local pivot = left[pivot_column][pivot_column]
        for column = 1, size do
            left[pivot_column][column] = left[pivot_column][column] / pivot
            right[pivot_column][column] = right[pivot_column][column] / pivot
        end

        for row = 1, size do
            if row ~= pivot_column then
                local factor = left[row][pivot_column]
                if factor ~= 0 then
                    for column = 1, size do
                        left[row][column] = left[row][column]
                            - factor * left[pivot_column][column]
                        right[row][column] = right[row][column]
                            - factor * right[pivot_column][column]
                    end
                end
            end
        end
    end

    return right
end

local function build_laplacian(node_count, edges)
    local laplacian = {}

    for row = 1, node_count do
        laplacian[row] = {}
        for column = 1, node_count do
            laplacian[row][column] = 0
        end
    end

    for index, edge in ipairs(edges) do
        assert(type(edge) == "table", "edge " .. index .. " must be a table")
        assert(type(edge.from) == "number" and edge.from >= 1 and edge.from <= node_count,
            "edge.from out of range")
        assert(type(edge.to) == "number" and edge.to >= 1 and edge.to <= node_count,
            "edge.to out of range")
        assert(edge.from ~= edge.to, "self edges are not allowed")
        assert_positive(edge.conductance, "edge.conductance")

        local i = edge.from
        local j = edge.to
        local conductance = edge.conductance

        laplacian[i][i] = laplacian[i][i] + conductance
        laplacian[j][j] = laplacian[j][j] + conductance
        laplacian[i][j] = laplacian[i][j] - conductance
        laplacian[j][i] = laplacian[j][i] - conductance
    end

    return laplacian
end

function M.laplacian_pseudoinverse(node_count, edges)
    assert(type(node_count) == "number" and node_count >= 2 and node_count % 1 == 0,
        "node_count must be an integer of at least two")
    assert(type(edges) == "table" and #edges > 0, "edges must be non-empty")

    local laplacian = build_laplacian(node_count, edges)
    local adjusted = {}

    for row = 1, node_count do
        adjusted[row] = {}
        for column = 1, node_count do
            adjusted[row][column] = laplacian[row][column] + 1 / node_count
        end
    end

    local inverse, err = invert_matrix(adjusted)
    if not inverse then
        return nil, err
    end

    local pseudoinverse = {}
    for row = 1, node_count do
        pseudoinverse[row] = {}
        for column = 1, node_count do
            pseudoinverse[row][column] = inverse[row][column] - 1 / node_count
        end
    end

    return pseudoinverse
end

function M.effective_resistance(node_count, edges, source, destination)
    assert(type(source) == "number" and source >= 1 and source <= node_count,
        "source out of range")
    assert(type(destination) == "number" and destination >= 1 and destination <= node_count,
        "destination out of range")
    assert(source ~= destination, "source and destination must differ")

    local lplus, err = M.laplacian_pseudoinverse(node_count, edges)
    if not lplus then
        return nil, err
    end

    local resistance = lplus[source][source]
        + lplus[destination][destination]
        - 2 * lplus[source][destination]

    return math.max(0, resistance)
end

function M.rank_stepping_stone_parcels(graph, source, destination, candidates)
    assert(type(graph) == "table", "graph must be a table")
    assert(type(graph.node_count) == "number", "graph.node_count required")
    assert(type(graph.edges) == "table", "graph.edges required")
    assert(type(candidates) == "table" and #candidates > 0,
        "candidates must be a non-empty array")

    local baseline, baseline_error = M.effective_resistance(
        graph.node_count,
        graph.edges,
        source,
        destination
    )
    if not baseline then
        return nil, baseline_error
    end

    local ranked = {}

    for index, candidate in ipairs(candidates) do
        assert(type(candidate) == "table", "candidate must be a table")
        assert(type(candidate.id) == "string" and #candidate.id > 0,
            "candidate id required")
        assert(type(candidate.edges) == "table" and #candidate.edges > 0,
            "candidate edges must be a non-empty array")
        assert_fraction(candidate.habitat_quality, "habitat_quality")
        assert_fraction(candidate.restoration_feasibility, "restoration_feasibility")
        assert_fraction(candidate.permitting_readiness, "permitting_readiness")
        assert_fraction(candidate.erosion_compatibility, "erosion_compatibility")

        local expanded_edges = copy_array(graph.edges)
        for _, edge in ipairs(candidate.edges) do
            expanded_edges[#expanded_edges + 1] = edge
        end

        local after, err = M.effective_resistance(
            graph.node_count,
            expanded_edges,
            source,
            destination
        )

        if after then
            local resistance_reduction = math.max(0, baseline - after)
            local normalized_reduction = resistance_reduction / math.max(baseline, 1e-12)

            ranked[#ranked + 1] = {
                id = candidate.id,
                baseline_effective_resistance = baseline,
                restored_effective_resistance = after,
                effective_resistance_reduction = resistance_reduction,
                relative_resistance_reduction = normalized_reduction,
                restoration_priority = normalized_reduction
                    * candidate.habitat_quality
                    * candidate.restoration_feasibility
                    * candidate.permitting_readiness
                    * candidate.erosion_compatibility,
                knowledge_factor = clamp(
                    0.35 * (candidate.connectivity_data_coverage or 0)
                        + 0.35 * (candidate.field_habitat_validation or 0)
                        + 0.30 * (candidate.hydrologic_validation or 0),
                    0,
                    1
                ),
                eco_impact_value = clamp(
                    0.60 * normalized_reduction
                        + 0.40 * candidate.habitat_quality,
                    0,
                    1
                ),
                harm_risk = clamp(
                    0.45 * (1 - candidate.erosion_compatibility)
                        + 0.30 * (1 - candidate.permitting_readiness)
                        + 0.25 * (1 - candidate.restoration_feasibility),
                    0,
                    1
                ),
            }
        else
            ranked[#ranked + 1] = {
                id = candidate.id,
                rejected = true,
                reason = err,
                restoration_priority = 0,
                knowledge_factor = 0,
                eco_impact_value = 0,
                harm_risk = 1,
            }
        end
    end

    table.sort(ranked, function(left, right)
        return left.restoration_priority > right.restoration_priority
    end)

    return ranked
end

function M.fit_power_law_beta(distances_m, minimum_distance_m)
    assert(type(distances_m) == "table" and #distances_m >= 2,
        "at least two dispersal distances are required")
    assert_positive(minimum_distance_m, "minimum_distance_m")

    local sum_log_ratio = 0
    local retained = 0

    for index, distance_m in ipairs(distances_m) do
        assert_positive(distance_m, "distance[" .. index .. "]")
        if distance_m >= minimum_distance_m then
            sum_log_ratio = sum_log_ratio + math.log(distance_m / minimum_distance_m)
            retained = retained + 1
        end
    end

    assert(retained >= 2, "at least two observations must meet minimum_distance_m")
    assert(sum_log_ratio > 0, "distances must not all equal minimum_distance_m")

    local beta = 1 + retained / sum_log_ratio
    local standard_error = (beta - 1) / math.sqrt(retained)

    return {
        beta = beta,
        standard_error = standard_error,
        sample_size = retained,
        minimum_distance_m = minimum_distance_m,
        model = "continuous Pareto tail p(d) proportional to d^(-beta)",
        knowledge_factor = clamp(math.min(1, retained / 100), 0, 1),
        eco_impact_value = 0,
        harm_risk = clamp(1 - math.min(1, retained / 100), 0, 1),
        limitation = "Estimate separately by species, body size, sex, season, and habitat context. Detection and sampling bias must be modeled before interpreting a dispersal kernel as population movement.",
    }
end

function M.compare_power_law_candidates(observations, candidate_minimums_m)
    assert(type(candidate_minimums_m) == "table" and #candidate_minimums_m > 0,
        "candidate_minimums_m must be non-empty")

    local candidates = {}

    for _, minimum_distance_m in ipairs(candidate_minimums_m) do
        local fitted = M.fit_power_law_beta(observations, minimum_distance_m)
        local log_likelihood = 0

        for _, distance_m in ipairs(observations) do
            if distance_m >= minimum_distance_m then
                log_likelihood = log_likelihood
                    + math.log(fitted.beta - 1)
                    + (fitted.beta - 1) * math.log(minimum_distance_m)
                    - fitted.beta * math.log(distance_m)
            end
        end

        fitted.log_likelihood = log_likelihood
        candidates[#candidates + 1] = fitted
    end

    table.sort(candidates, function(left, right)
        return left.log_likelihood > right.log_likelihood
    end)

    return candidates
end

function M.shields_stress(shear_stress_pa, sediment_density_kg_m3,
                          water_density_kg_m3, median_grain_size_m)
    assert_nonnegative(shear_stress_pa, "shear_stress_pa")
    assert_positive(sediment_density_kg_m3, "sediment_density_kg_m3")
    assert_positive(water_density_kg_m3, "water_density_kg_m3")
    assert_positive(median_grain_size_m, "median_grain_size_m")
    assert(sediment_density_kg_m3 > water_density_kg_m3,
        "sediment density must exceed water density")

    local gravity_m_per_s2 = 9.80665
    return shear_stress_pa
        / ((sediment_density_kg_m3 - water_density_kg_m3)
            * gravity_m_per_s2
            * median_grain_size_m)
end

function M.screen_ephemeral_wash(wash)
    assert(type(wash) == "table", "wash must be a table")
    assert(type(wash.id) == "string" and #wash.id > 0, "wash id required")
    assert_nonnegative(wash.design_shear_stress_pa, "design_shear_stress_pa")
    assert_positive(wash.sediment_density_kg_m3, "sediment_density_kg_m3")
    assert_positive(wash.water_density_kg_m3, "water_density_kg_m3")
    assert_positive(wash.d50_m, "d50_m")
    assert_positive(wash.critical_shields_stress, "critical_shields_stress")
    assert_fraction(wash.flood_conveyance_compatibility,
        "flood_conveyance_compatibility")
    assert_fraction(wash.wildlife_corridor_value, "wildlife_corridor_value")
    assert_fraction(wash.downstream_erosion_compatibility,
        "downstream_erosion_compatibility")
    assert_fraction(wash.cultural_and_permitting_readiness,
        "cultural_and_permitting_readiness")
    assert_fraction(wash.vegetation_establishment_suitability,
        "vegetation_establishment_suitability")

    local shields = M.shields_stress(
        wash.design_shear_stress_pa,
        wash.sediment_density_kg_m3,
        wash.water_density_kg_m3,
        wash.d50_m
    )

    local mobility_ratio = shields / wash.critical_shields_stress
    local sediment_stability = clamp(1 - math.max(0, mobility_ratio - 1), 0, 1)

    local eligible = wash.flood_conveyance_compatibility >= 0.70
        and wash.downstream_erosion_compatibility >= 0.75
        and wash.cultural_and_permitting_readiness >= 0.70
        and wash.vegetation_establishment_suitability >= 0.50

    local priority = sediment_stability
        * wash.flood_conveyance_compatibility
        * wash.wildlife_corridor_value
        * wash.downstream_erosion_compatibility
        * wash.cultural_and_permitting_readiness
        * wash.vegetation_establishment_suitability

    return {
        id = wash.id,
        dimensionless_shields_stress = shields,
        critical_shields_stress = wash.critical_shields_stress,
        mobility_ratio = mobility_ratio,
        sediment_stability_score = sediment_stability,
        eligible_for_further_design = eligible,
        restoration_priority = eligible and priority or 0,
        required_next_steps = {
            "Survey channel geometry, grade controls, culverts, utilities, and sediment supply.",
            "Model design-storm hydraulics and sediment transport at reach and downstream scales.",
            "Complete cultural-resource, land-tenure, community, and regulatory review.",
            "Use monitored pilot treatments before any larger intervention.",
        },
        knowledge_factor = clamp(
            0.30 * (wash.hydraulic_observation_coverage or 0)
                + 0.30 * (wash.geomorphic_survey_coverage or 0)
                + 0.20 * (wash.ecological_survey_coverage or 0)
                + 0.20 * (wash.downstream_monitoring_coverage or 0),
            0,
            1
        ),
        eco_impact_value = clamp(
            0.55 * wash.wildlife_corridor_value
                + 0.25 * wash.vegetation_establishment_suitability
                + 0.20 * wash.flood_conveyance_compatibility,
            0,
            1
        ),
        harm_risk = clamp(
            0.45 * (1 - wash.downstream_erosion_compatibility)
                + 0.30 * (1 - wash.flood_conveyance_compatibility)
                + 0.25 * math.min(1, math.max(0, mobility_ratio - 1)),
            0,
            1
        ),
        limitation = "The Shields criterion screens bed-material mobility only. It cannot by itself establish reach stability, flood safety, sediment continuity, or habitat outcomes.",
    }
end

return M
