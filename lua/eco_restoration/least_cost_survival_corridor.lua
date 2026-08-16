local least_cost_survival_corridor = {}

local CorridorGraph = {}
CorridorGraph.__index = CorridorGraph

local function finite_number(value)
    return type(value) == "number"
        and value == value
        and value ~= math.huge
        and value ~= -math.huge
end

local function clamp(value, minimum, maximum)
    if value < minimum then
        return minimum
    end

    if value > maximum then
        return maximum
    end

    return value
end

local function copy_table(source)
    local result = {}

    for key, value in pairs(source) do
        if type(value) == "table" then
            result[key] = copy_table(value)
        else
            result[key] = value
        end
    end

    return result
end

local function validate_probability(value, name)
    assert(finite_number(value) and value >= 0.0 and value <= 1.0,
        name .. " must be within [0, 1]")
end

function CorridorGraph.new(nodes)
    assert(type(nodes) == "table", "nodes must be a table")

    local normalized = {}

    for node_id, node in pairs(nodes) do
        assert(type(node_id) == "string" and node_id ~= "",
            "node IDs must be non-empty strings")
        assert(type(node) == "table", "node metadata must be a table")

        normalized[node_id] = copy_table(node)
    end

    return setmetatable({
        nodes = normalized,
        edges = {},
    }, CorridorGraph)
end

function CorridorGraph:add_edge(from_id, to_id, cost, survival_probability)
    assert(self.nodes[from_id], "unknown source node: " .. tostring(from_id))
    assert(self.nodes[to_id], "unknown destination node: " .. tostring(to_id))
    assert(from_id ~= to_id, "self-edges are not supported")
    assert(finite_number(cost) and cost > 0.0,
        "cost must be a positive finite number")
    validate_probability(survival_probability, "survival_probability")

    self.edges[from_id] = self.edges[from_id] or {}
    self.edges[from_id][to_id] = {
        cost = cost,
        survival_probability = survival_probability,
    }
end

function CorridorGraph:add_bidirectional_edge(
    left_id,
    right_id,
    cost,
    left_to_right_survival_probability,
    right_to_left_survival_probability
)
    self:add_edge(left_id, right_id, cost, left_to_right_survival_probability)
    self:add_edge(
        right_id,
        left_id,
        cost,
        right_to_left_survival_probability or left_to_right_survival_probability
    )
end

function CorridorGraph:neighbors(node_id)
    assert(self.nodes[node_id], "unknown node: " .. tostring(node_id))
    return self.edges[node_id] or {}
end

function CorridorGraph:least_cost_path(source_id, destination_id)
    assert(self.nodes[source_id], "unknown source node: " .. tostring(source_id))
    assert(self.nodes[destination_id],
        "unknown destination node: " .. tostring(destination_id))

    local distance = {}
    local predecessor = {}
    local unsettled = {}

    for node_id in pairs(self.nodes) do
        distance[node_id] = math.huge
        unsettled[node_id] = true
    end

    distance[source_id] = 0.0

    while true do
        local current_id = nil
        local current_distance = math.huge

        for node_id in pairs(unsettled) do
            if distance[node_id] < current_distance then
                current_id = node_id
                current_distance = distance[node_id]
            end
        end

        if current_id == nil then
            break
        end

        if current_id == destination_id then
            break
        end

        unsettled[current_id] = nil

        for neighbor_id, edge in pairs(self:neighbors(current_id)) do
            if unsettled[neighbor_id] then
                local proposed = current_distance + edge.cost

                if proposed < distance[neighbor_id] then
                    distance[neighbor_id] = proposed
                    predecessor[neighbor_id] = current_id
                end
            end
        end
    end

    if distance[destination_id] == math.huge then
        return nil, "destination is unreachable"
    end

    local reverse_path = {}
    local cursor = destination_id

    while cursor do
        reverse_path[#reverse_path + 1] = cursor
        cursor = predecessor[cursor]
    end

    local path = {}

    for index = #reverse_path, 1, -1 do
        path[#path + 1] = reverse_path[index]
    end

    assert(path[1] == source_id,
        "path reconstruction failed to reach source")

    return {
        nodes = path,
        cost = distance[destination_id],
    }
end

function CorridorGraph:path_probability(path)
    assert(type(path) == "table" and #path >= 2,
        "path must contain at least two nodes")

    local no_success_probability = 1.0
    local survival_probability = 1.0

    for index = 1, #path - 1 do
        local from_id = path[index]
        local to_id = path[index + 1]
        local edge = self.edges[from_id] and self.edges[from_id][to_id]

        assert(edge, "path contains an unknown edge")

        survival_probability = survival_probability * edge.survival_probability
        no_success_probability = no_success_probability
            * (1.0 - edge.survival_probability)
    end

    -- The product of edge survival probabilities is the probability of
    -- surviving every required edge on a serial path. The complementary form
    -- answers a different question: probability of at least one edge success.
    return {
        serial_path_survival_probability = survival_probability,
        at_least_one_edge_success_probability = 1.0 - no_success_probability,
    }
end

function CorridorGraph:least_cost_connectivity(source_id, destination_id)
    local path, err = self:least_cost_path(source_id, destination_id)

    if not path then
        return nil, err
    end

    local probability = self:path_probability(path.nodes)

    return {
        path = path.nodes,
        total_cost = path.cost,
        serial_path_survival_probability =
            probability.serial_path_survival_probability,
        at_least_one_edge_success_probability =
            probability.at_least_one_edge_success_probability,
    }
end

function CorridorGraph:calibrate_edge_probability(features, coefficients)
    assert(type(features) == "table", "features must be a table")
    coefficients = coefficients or {}

    local road_crossing_count = features.road_crossing_count or 0.0
    local road_class_risk = features.road_class_risk or 0.0
    local traffic_intensity = features.traffic_intensity or 0.0
    local impervious_fraction = features.impervious_fraction or 0.0
    local habitat_suitability = features.habitat_suitability or 0.0
    local crossing_structure_quality = features.crossing_structure_quality or 0.0

    assert(finite_number(road_crossing_count) and road_crossing_count >= 0.0,
        "road_crossing_count must be non-negative")
    assert(finite_number(road_class_risk)
        and road_class_risk >= 0.0 and road_class_risk <= 1.0,
        "road_class_risk must be within [0, 1]")
    assert(finite_number(traffic_intensity)
        and traffic_intensity >= 0.0 and traffic_intensity <= 1.0,
        "traffic_intensity must be within [0, 1]")
    assert(finite_number(impervious_fraction)
        and impervious_fraction >= 0.0 and impervious_fraction <= 1.0,
        "impervious_fraction must be within [0, 1]")
    assert(finite_number(habitat_suitability)
        and habitat_suitability >= 0.0 and habitat_suitability <= 1.0,
        "habitat_suitability must be within [0, 1]")
    assert(finite_number(crossing_structure_quality)
        and crossing_structure_quality >= 0.0
        and crossing_structure_quality <= 1.0,
        "crossing_structure_quality must be within [0, 1]")

    local intercept = coefficients.intercept or 0.0
    local road_count_weight = coefficients.road_count_weight or -0.80
    local road_class_weight = coefficients.road_class_weight or -1.20
    local traffic_weight = coefficients.traffic_weight or -1.10
    local impervious_weight = coefficients.impervious_weight or -0.90
    local habitat_weight = coefficients.habitat_weight or 1.10
    local crossing_weight = coefficients.crossing_weight or 0.80

    local logit = intercept
        + road_count_weight * road_crossing_count
        + road_class_weight * road_class_risk
        + traffic_weight * traffic_intensity
        + impervious_weight * impervious_fraction
        + habitat_weight * habitat_suitability
        + crossing_weight * crossing_structure_quality

    local probability = 1.0 / (1.0 + math.exp(-logit))

    return {
        survival_probability = clamp(probability, 0.0, 1.0),
        logit = logit,
        features = copy_table(features),
        coefficients = copy_table(coefficients),
    }
end

function CorridorGraph:metrics(
    validation_coverage,
    validation_agreement,
    connectivity_benefit,
    mortality_or_disturbance_risk
)
    validate_probability(validation_coverage, "validation_coverage")
    validate_probability(validation_agreement, "validation_agreement")
    validate_probability(connectivity_benefit, "connectivity_benefit")
    validate_probability(mortality_or_disturbance_risk,
        "mortality_or_disturbance_risk")

    local knowledge_factor = math.sqrt(validation_coverage * validation_agreement)

    return {
        knowledge_factor = knowledge_factor,
        eco_impact_value = knowledge_factor
            * connectivity_benefit
            * (1.0 - mortality_or_disturbance_risk),
        harm_risk = mortality_or_disturbance_risk,
    }
end

function least_cost_survival_corridor.new(nodes)
    return CorridorGraph.new(nodes)
end

return least_cost_survival_corridor
