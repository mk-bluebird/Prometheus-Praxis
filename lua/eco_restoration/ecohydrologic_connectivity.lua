local ecohydrologic_connectivity = {}

local SparseAdjacency = {}
SparseAdjacency.__index = SparseAdjacency

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

local function validate_node(node_id, node)
    assert(type(node_id) == "string" and node_id ~= "",
        "node identifier must be a non-empty string")
    assert(type(node) == "table", "node must be a table")
    assert(finite_number(node.alpha) and node.alpha >= 0.0,
        "node.alpha must be finite and non-negative")
end

function SparseAdjacency.new(nodes)
    assert(type(nodes) == "table", "nodes must be a table")

    local normalized_nodes = {}

    for node_id, node in pairs(nodes) do
        validate_node(node_id, node)
        normalized_nodes[node_id] = {
            alpha = node.alpha,
        }
    end

    return setmetatable({
        nodes = normalized_nodes,
        edges = {},
    }, SparseAdjacency)
end

function SparseAdjacency:add_edge(from_id, to_id, distance_m, transition_cost)
    assert(self.nodes[from_id], "unknown source node: " .. tostring(from_id))
    assert(self.nodes[to_id], "unknown destination node: " .. tostring(to_id))
    assert(from_id ~= to_id, "self-edges are not permitted")
    assert(finite_number(distance_m) and distance_m > 0.0,
        "distance_m must be positive")

    transition_cost = transition_cost or 1.0
    assert(finite_number(transition_cost) and transition_cost > 0.0,
        "transition_cost must be positive")

    self.edges[from_id] = self.edges[from_id] or {}

    self.edges[from_id][to_id] = {
        distance_m = distance_m,
        transition_cost = transition_cost,
    }
end

function SparseAdjacency:add_bidirectional_edge(left_id, right_id, distance_m, transition_cost)
    self:add_edge(left_id, right_id, distance_m, transition_cost)
    self:add_edge(right_id, left_id, distance_m, transition_cost)
end

function SparseAdjacency:neighbors(node_id)
    assert(self.nodes[node_id], "unknown node: " .. tostring(node_id))
    return self.edges[node_id] or {}
end

function SparseAdjacency:shortest_cost_distances(source_id)
    assert(self.nodes[source_id], "unknown source node: " .. tostring(source_id))

    local distances = {}
    local unvisited = {}

    for node_id in pairs(self.nodes) do
        distances[node_id] = math.huge
        unvisited[node_id] = true
    end

    distances[source_id] = 0.0

    while true do
        local current_id = nil
        local current_distance = math.huge

        for node_id in pairs(unvisited) do
            if distances[node_id] < current_distance then
                current_id = node_id
                current_distance = distances[node_id]
            end
        end

        if current_id == nil then
            break
        end

        unvisited[current_id] = nil

        for neighbor_id, edge in pairs(self:neighbors(current_id)) do
            if unvisited[neighbor_id] then
                local candidate = current_distance
                    + edge.distance_m * edge.transition_cost

                if candidate < distances[neighbor_id] then
                    distances[neighbor_id] = candidate
                end
            end
        end
    end

    return distances
end

function SparseAdjacency:connectivity_index(dispersal_length_m)
    assert(finite_number(dispersal_length_m) and dispersal_length_m > 0.0,
        "dispersal_length_m must be positive")

    local numerator = 0.0
    local denominator = 0.0
    local pair_count = 0

    for source_id, source in pairs(self.nodes) do
        denominator = denominator + source.alpha

        local distances = self:shortest_cost_distances(source_id)

        for destination_id, destination in pairs(self.nodes) do
            if source_id ~= destination_id then
                local distance = distances[destination_id]

                if distance < math.huge then
                    numerator = numerator
                        + source.alpha
                        * destination.alpha
                        * math.exp(-distance / dispersal_length_m)

                    pair_count = pair_count + 1
                end
            end
        end
    end

    assert(denominator > 0.0,
        "sum of alpha weights must be positive")

    return {
        eci = numerator / denominator,
        numerator = numerator,
        denominator = denominator,
        reachable_directed_pairs = pair_count,
        dispersal_length_m = dispersal_length_m,
    }
end

function SparseAdjacency:transition_observations_to_length(observations)
    assert(type(observations) == "table" and #observations > 0,
        "observations must be a non-empty array")

    local weighted_distance_sum = 0.0
    local weight_sum = 0.0
    local accepted = 0

    for index = 1, #observations do
        local observation = observations[index]
        assert(type(observation) == "table",
            "observation " .. index .. " must be a table")
        assert(finite_number(observation.distance_m) and observation.distance_m > 0.0,
            "observation.distance_m must be positive")
        assert(finite_number(observation.transition_probability)
            and observation.transition_probability > 0.0
            and observation.transition_probability <= 1.0,
            "observation.transition_probability must be within (0, 1]")
        assert(finite_number(observation.land_cover_suitability)
            and observation.land_cover_suitability >= 0.0
            and observation.land_cover_suitability <= 1.0,
            "observation.land_cover_suitability must be within [0, 1]")

        local adjusted_weight = observation.transition_probability
            * math.max(observation.land_cover_suitability, 1e-6)

        weighted_distance_sum = weighted_distance_sum
            + observation.distance_m * adjusted_weight
        weight_sum = weight_sum + adjusted_weight
        accepted = accepted + 1
    end

    assert(weight_sum > 0.0,
        "transition observations have zero usable weight")

    local mean_transition_distance_m = weighted_distance_sum / weight_sum

    return {
        dispersal_length_m = mean_transition_distance_m,
        mean_transition_distance_m = mean_transition_distance_m,
        observation_count = accepted,
        interpretation = "This is a weighted exponential-kernel scale estimate. Fit and validate it separately by taxon, season, and movement mechanism.",
    }
end

function SparseAdjacency:metrics(
    dispersal_length_m,
    observation_coverage,
    observed_transition_agreement,
    restoration_benefit,
    disturbance_risk
)
    assert(finite_number(observation_coverage)
        and observation_coverage >= 0.0
        and observation_coverage <= 1.0,
        "observation_coverage must be within [0, 1]")
    assert(finite_number(observed_transition_agreement)
        and observed_transition_agreement >= 0.0
        and observed_transition_agreement <= 1.0,
        "observed_transition_agreement must be within [0, 1]")
    assert(finite_number(restoration_benefit)
        and restoration_benefit >= 0.0
        and restoration_benefit <= 1.0,
        "restoration_benefit must be within [0, 1]")
    assert(finite_number(disturbance_risk)
        and disturbance_risk >= 0.0
        and disturbance_risk <= 1.0,
        "disturbance_risk must be within [0, 1]")

    local index = self:connectivity_index(dispersal_length_m)

    local knowledge_factor = math.sqrt(
        observation_coverage * observed_transition_agreement
    )

    local eco_impact_value = clamp(
        index.eci * restoration_benefit * knowledge_factor,
        0.0,
        1.0
    )

    local harm_risk = clamp(
        disturbance_risk * (1.0 - 0.5 * knowledge_factor),
        0.0,
        1.0
    )

    return {
        eci = index.eci,
        knowledge_factor = knowledge_factor,
        eco_impact_value = eco_impact_value,
        harm_risk = harm_risk,
    }
end

function ecohydrologic_connectivity.new(nodes)
    return SparseAdjacency.new(nodes)
end

return ecohydrologic_connectivity
