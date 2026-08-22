local HexAnchorConsistency = {}

local function finite(value, name)
    assert(type(value) == "number" and value == value and value ~= math.huge and value ~= -math.huge,
        name .. " must be finite")
    return value
end

local function nonempty(value, name)
    assert(type(value) == "string" and value:match("%S"),
        name .. " must be a non-empty string")
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

local function validate_coordinate(latitude, longitude)
    finite(latitude, "latitude")
    finite(longitude, "longitude")
    assert(latitude >= -90.0 and latitude <= 90.0, "latitude must be within -90..90")
    assert(longitude >= -180.0 and longitude <= 180.0, "longitude must be within -180..180")
end

local function validate_hex_anchor(anchor)
    assert(type(anchor) == "table", "anchor must be a table")
    nonempty(anchor.hex_id, "hex_id")
    assert(type(anchor.resolution) == "number" and anchor.resolution == math.floor(anchor.resolution),
        "resolution must be an integer")
    assert(anchor.resolution >= 0 and anchor.resolution <= 15,
        "resolution must be within 0..15")
    validate_coordinate(anchor.latitude, anchor.longitude)
end

function HexAnchorConsistency.coordinate_truncation_error_m(latitude, decimal_places)
    finite(latitude, "latitude")
    assert(type(decimal_places) == "number" and decimal_places == math.floor(decimal_places)
        and decimal_places >= 0 and decimal_places <= 12,
        "decimal_places must be an integer within 0..12")

    local degree_step = 10.0 ^ (-decimal_places)
    local latitude_error_m = 111320.0 * degree_step / 2.0
    local longitude_error_m = 111320.0 * math.cos(math.rad(latitude)) * degree_step / 2.0

    return math.sqrt(latitude_error_m * latitude_error_m + longitude_error_m * longitude_error_m)
end

function HexAnchorConsistency.precision_required(use_case)
    local policies = {
        aquifer_summary = {
            exact_coordinate_required = false,
            recommended_decimal_places = 3,
            reason = "Publish only hydrologic-unit summaries; avoid exposing private well locations."
        },
        aquifer_monitoring_station = {
            exact_coordinate_required = true,
            recommended_decimal_places = 5,
            reason = "Authorized monitoring requires enough precision to reproduce a public or approved station."
        },
        tree_inventory = {
            exact_coordinate_required = true,
            recommended_decimal_places = 6,
            reason = "Individual trees and sidewalk shade require sub-block spatial placement."
        },
        heat_hex_summary = {
            exact_coordinate_required = false,
            recommended_decimal_places = 4,
            reason = "A hex identifier and resolution are sufficient for public heat-risk aggregation."
        },
        public_corridor_report = {
            exact_coordinate_required = false,
            recommended_decimal_places = 3,
            reason = "Coarsened coordinates support accountability while limiting infrastructure exposure."
        }
    }

    assert(policies[use_case], "unknown use_case")
    return policies[use_case]
end

function HexAnchorConsistency.second_order_penalty(latent_values, adjacency, lambda)
    assert(type(latent_values) == "table" and #latent_values > 0,
        "latent_values must be a non-empty array")
    assert(type(adjacency) == "table", "adjacency must be an array")
    finite(lambda, "lambda")
    assert(lambda >= 0.0, "lambda must be non-negative")

    local first_order = 0.0
    local second_order = 0.0
    local neighbor_sum = {}
    local neighbor_count = {}

    for i = 1, #latent_values do
        finite(latent_values[i], "latent value")
        neighbor_sum[i] = 0.0
        neighbor_count[i] = 0
    end

    for _, edge in ipairs(adjacency) do
        assert(type(edge) == "table", "edge must be a table")
        local i = edge[1]
        local j = edge[2]
        assert(type(i) == "number" and type(j) == "number"
            and i == math.floor(i) and j == math.floor(j),
            "edge indices must be integers")
        assert(i >= 1 and i <= #latent_values and j >= 1 and j <= #latent_values and i ~= j,
            "edge index outside latent array")

        local difference = latent_values[i] - latent_values[j]
        first_order = first_order + difference * difference
        neighbor_sum[i] = neighbor_sum[i] + latent_values[j]
        neighbor_sum[j] = neighbor_sum[j] + latent_values[i]
        neighbor_count[i] = neighbor_count[i] + 1
        neighbor_count[j] = neighbor_count[j] + 1
    end

    local boundary_scores = {}
    for i = 1, #latent_values do
        if neighbor_count[i] > 0 then
            local neighbor_mean = neighbor_sum[i] / neighbor_count[i]
            local difference = latent_values[i] - neighbor_mean
            second_order = second_order + difference * difference
            boundary_scores[i] = math.abs(difference)
        else
            boundary_scores[i] = 0.0
        end
    end

    return {
        first_order = first_order,
        second_order = second_order,
        total_penalty = first_order + lambda * second_order,
        boundary_scores = boundary_scores
    }
end

function HexAnchorConsistency.detect_heat_boundary(latent_values, adjacency, threshold)
    finite(threshold, "threshold")
    assert(threshold >= 0.0, "threshold must be non-negative")

    local result = HexAnchorConsistency.second_order_penalty(latent_values, adjacency, 0.0)
    local boundaries = {}

    for i, score in ipairs(result.boundary_scores) do
        if score >= threshold then
            boundaries[#boundaries + 1] = {
                hex_position = i,
                discontinuity_score = score
            }
        end
    end

    return boundaries
end

function HexAnchorConsistency.public_anchor(anchor, use_case)
    validate_hex_anchor(anchor)
    local policy = HexAnchorConsistency.precision_required(use_case)
    local places = policy.recommended_decimal_places
    local multiplier = 10 ^ places

    return {
        hex_id = anchor.hex_id,
        resolution = anchor.resolution,
        latitude = math.floor(anchor.latitude * multiplier + 0.5) / multiplier,
        longitude = math.floor(anchor.longitude * multiplier + 0.5) / multiplier,
        coordinate_precision_required = policy.exact_coordinate_required,
        coordinate_precision_reason = policy.reason,
        maximum_rounding_error_m = HexAnchorConsistency.coordinate_truncation_error_m(
            anchor.latitude,
            places
        )
    }
end

function HexAnchorConsistency.assess(anchor, use_case, latent_values, adjacency, lambda, boundary_threshold)
    local public_anchor = HexAnchorConsistency.public_anchor(anchor, use_case)
    local regularizer = HexAnchorConsistency.second_order_penalty(latent_values, adjacency, lambda)
    local boundaries = HexAnchorConsistency.detect_heat_boundary(
        latent_values,
        adjacency,
        boundary_threshold
    )

    local knowledge_factor = clamp01(
        0.55
        + 0.20 * (public_anchor.coordinate_precision_required and 1.0 or 0.8)
        + 0.25 * math.min(#adjacency / math.max(#latent_values, 1), 1.0)
    )
    local eco_impact_value = clamp01(0.70 + 0.30 * (#boundaries > 0 and 1.0 or 0.5))
    local harm_risk = clamp01(#boundaries > 0 and 0.15 or 0.35)

    return {
        anchor = public_anchor,
        regularizer = regularizer,
        detected_boundaries = boundaries,
        knowledge_factor = knowledge_factor,
        eco_impact_value = eco_impact_value,
        harm_risk = harm_risk
    }
end

return HexAnchorConsistency
