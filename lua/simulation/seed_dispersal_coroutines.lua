local seed_dispersal_coroutines = {}

local SeedDispersal = {}
SeedDispersal.__index = SeedDispersal

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

local function deterministic_unit(seed)
    local value = math.sin(seed * 12.9898 + 78.233) * 43758.5453
    return value - math.floor(value)
end

local function validate_parcel(parcel_id, parcel)
    assert(type(parcel_id) == "string" and parcel_id ~= "",
        "parcel ID must be a non-empty string")
    assert(type(parcel) == "table", "parcel must be a table")
    assert(finite_number(parcel.habitat_suitability)
        and parcel.habitat_suitability >= 0.0
        and parcel.habitat_suitability <= 1.0,
        "parcel habitat_suitability must be within [0, 1]")
    assert(finite_number(parcel.canopy_cover)
        and parcel.canopy_cover >= 0.0
        and parcel.canopy_cover <= 1.0,
        "parcel canopy_cover must be within [0, 1]")
    assert(finite_number(parcel.impervious_fraction)
        and parcel.impervious_fraction >= 0.0
        and parcel.impervious_fraction <= 1.0,
        "parcel impervious_fraction must be within [0, 1]")
end

local function validate_agent(agent_id, agent, parcels)
    assert(type(agent_id) == "string" and agent_id ~= "",
        "agent ID must be a non-empty string")
    assert(type(agent) == "table", "agent must be a table")
    assert(parcels[agent.parcel_id],
        "agent parcel_id must reference an existing parcel")
    assert(finite_number(agent.seed_quantity) and agent.seed_quantity >= 0.0,
        "agent seed_quantity must be non-negative")
    assert(type(agent.memory_length) == "number"
        and agent.memory_length % 1 == 0
        and agent.memory_length >= 0,
        "agent memory_length must be a non-negative integer")
end

local function weighted_destination(agent, parcel_id, neighbors, parcels, visits, step)
    local candidates = {}
    local total_weight = 0.0

    for index = 1, #neighbors do
        local neighbor = neighbors[index]
        local destination = parcels[neighbor.id]

        assert(destination, "movement graph references an unknown parcel")
        assert(finite_number(neighbor.distance_m) and neighbor.distance_m > 0.0,
            "neighbor distance_m must be positive")
        assert(finite_number(neighbor.corridor_conductance)
            and neighbor.corridor_conductance >= 0.0,
            "neighbor corridor_conductance must be non-negative")

        local revisited = visits[neighbor.id] or 0
        local memory_penalty = 1.0 / (1.0 + revisited)

        local preference = (
            0.45 * destination.habitat_suitability
            + 0.35 * destination.canopy_cover
            + 0.20 * (1.0 - destination.impervious_fraction)
        )

        local distance_penalty = 1.0 / neighbor.distance_m
        local weight = neighbor.corridor_conductance
            * preference
            * distance_penalty
            * memory_penalty

        if weight > 0.0 then
            total_weight = total_weight + weight
            candidates[#candidates + 1] = {
                id = neighbor.id,
                weight = weight,
            }
        end
    end

    if total_weight == 0.0 then
        return parcel_id
    end

    local draw = deterministic_unit(
        #agent.id * 1000003 + step * 9176 + #parcel_id * 31
    ) * total_weight

    local cumulative = 0.0

    for index = 1, #candidates do
        cumulative = cumulative + candidates[index].weight
        if draw <= cumulative then
            return candidates[index].id
        end
    end

    return candidates[#candidates].id
end

function SeedDispersal.new(parcels, movement_graph, agents)
    assert(type(parcels) == "table", "parcels must be a table")
    assert(type(movement_graph) == "table", "movement_graph must be a table")
    assert(type(agents) == "table", "agents must be a table")

    local model = setmetatable({
        parcels = {},
        movement_graph = copy_table(movement_graph),
        seed_rain = {},
        agents = {},
        time_step = 0,
    }, SeedDispersal)

    for parcel_id, parcel in pairs(parcels) do
        validate_parcel(parcel_id, parcel)
        model.parcels[parcel_id] = copy_table(parcel)
        model.seed_rain[parcel_id] = 0.0
    end

    for agent_id, agent in pairs(agents) do
        validate_agent(agent_id, agent, model.parcels)

        local state = copy_table(agent)
        state.id = agent_id
        state.memory = {}
        state.coroutine = coroutine.create(function()
            while true do
                local next_parcel = weighted_destination(
                    state,
                    state.parcel_id,
                    model.movement_graph[state.parcel_id] or {},
                    model.parcels,
                    state.memory,
                    model.time_step
                )

                state.parcel_id = next_parcel
                state.memory[next_parcel] = (state.memory[next_parcel] or 0) + 1

                local memory_total = 0
                for _ in pairs(state.memory) do
                    memory_total = memory_total + 1
                end

                if memory_total > state.memory_length then
                    local oldest_id = nil

                    for visited_id in pairs(state.memory) do
                        if oldest_id == nil or visited_id < oldest_id then
                            oldest_id = visited_id
                        end
                    end

                    state.memory[oldest_id] = nil
                end

                local parcel = model.parcels[state.parcel_id]
                local deposited = state.seed_quantity
                    * parcel.habitat_suitability
                    * (1.0 - parcel.impervious_fraction)

                coroutine.yield({
                    agent_id = state.id,
                    parcel_id = state.parcel_id,
                    deposited_seeds = deposited,
                    time_step = model.time_step,
                })
            end
        end)

        model.agents[agent_id] = state
    end

    return model
end

function SeedDispersal:step()
    local events = {}

    for agent_id, agent in pairs(self.agents) do
        assert(coroutine.status(agent.coroutine) ~= "dead",
            "agent coroutine unexpectedly terminated: " .. agent_id)

        local ok, event = coroutine.resume(agent.coroutine)
        assert(ok, "agent coroutine failed: " .. tostring(event))

        self.seed_rain[event.parcel_id] = self.seed_rain[event.parcel_id]
            + event.deposited_seeds

        events[#events + 1] = event
    end

    self.time_step = self.time_step + 1
    return events
end

function SeedDispersal:run(steps)
    assert(type(steps) == "number" and steps % 1 == 0 and steps > 0,
        "steps must be a positive integer")

    local events = {}

    for step = 1, steps do
        events[step] = self:step()
    end

    return events
end

function SeedDispersal:seed_rain_at(parcel_id)
    assert(self.parcels[parcel_id], "unknown parcel: " .. tostring(parcel_id))
    return self.seed_rain[parcel_id]
end

function SeedDispersal:metrics()
    local parcel_count = 0
    local total_seed_rain = 0.0
    local occupied_parcels = 0
    local low_suitability_deposition = 0.0

    for parcel_id, parcel in pairs(self.parcels) do
        parcel_count = parcel_count + 1

        local deposited = self.seed_rain[parcel_id]
        total_seed_rain = total_seed_rain + deposited

        if deposited > 0.0 then
            occupied_parcels = occupied_parcels + 1
        end

        low_suitability_deposition = low_suitability_deposition
            + deposited * (1.0 - parcel.habitat_suitability)
    end

    local coverage = occupied_parcels / math.max(parcel_count, 1)
    local unsuitable_fraction = low_suitability_deposition
        / math.max(total_seed_rain, 1e-12)

    local knowledge_factor = clamp(
        math.min(1.0, self.time_step / 30.0) * coverage,
        0.0,
        1.0
    )

    local eco_impact_value = clamp(
        coverage * (1.0 - unsuitable_fraction) * knowledge_factor,
        0.0,
        1.0
    )

    local harm_risk = clamp(unsuitable_fraction, 0.0, 1.0)

    return {
        total_seed_rain = total_seed_rain,
        occupied_parcel_fraction = coverage,
        unsuitable_deposition_fraction = unsuitable_fraction,
        knowledge_factor = knowledge_factor,
        eco_impact_value = eco_impact_value,
        harm_risk = harm_risk,
    }
end

function seed_dispersal_coroutines.new(parcels, movement_graph, agents)
    return SeedDispersal.new(parcels, movement_graph, agents)
end

return seed_dispersal_coroutines
