-- File: lua/hex_corridor_planning.lua
-- Lua-scripted hexagonal grid traversal for restorative corridor planning.
-- Given a starting hex-cell, iteratively explore neighbours using H3 adjacency
-- functions (ported to Lua) and compute cumulative restoration cost, stopping
-- when marginal benefit (LST reduction per kJ) falls below a threshold.
-- Results are written to SQL "corridor_plan" table.

local CorridorPlanner = {}

-- Data structures:
-- hex_state[h3_index] = {
--     green_fraction,
--     deltaT,         -- LST anomaly (K)
--     cost_per_kJ,    -- cost per unit energy (currency/kJ)
--     visited = false
-- }

local hex_state = {}

-- Placeholder for H3 edge/neighbour functions ported to Lua.
-- In practice, these would compute adjacent H3 indices from a given index.
local function h3_neighbors(h3_index)
    -- Return list of neighbour H3 indices.
    -- Here we mock with a pre-defined adjacency table; real code uses H3 bindings.
    local neighbors = {
        ["hex-A"] = {"hex-B", "hex-C"},
        ["hex-B"] = {"hex-A", "hex-D"},
        ["hex-C"] = {"hex-A", "hex-D"},
        ["hex-D"] = {"hex-B", "hex-C"}
    }
    return neighbors[h3_index] or {}
end

-- Compute marginal benefit: LST reduction per kJ for a given hex.
local function marginal_benefit(h3_index)
    local hs = hex_state[h3_index]
    if not hs then return 0.0 end
    -- Example model: benefit = deltaT / (cost_per_kJ * energy_required)
    -- Assume unit energy_required per step for simplicity.
    if hs.cost_per_kJ <= 0.0 then return 0.0 end
    return hs.deltaT / hs.cost_per_kJ
end

-- Recursive/iterative traversal with benefit threshold.
local function traverse_corridor(start_h3, benefit_threshold)
    local corridor = {}
    local cumulative_cost = 0.0

    local stack = {start_h3}
    while #stack > 0 do
        local h3 = table.remove(stack)
        if not hex_state[h3] or hex_state[h3].visited then
            goto continue
        end

        local mb = marginal_benefit(h3)
        if mb < benefit_threshold then
            goto continue
        end

        hex_state[h3].visited = true
        cumulative_cost = cumulative_cost + hex_state[h3].cost_per_kJ
        table.insert(corridor, {h3_index = h3, benefit = mb, cumulative_cost = cumulative_cost})

        -- Explore neighbours.
        for _, nb in ipairs(h3_neighbors(h3)) do
            if hex_state[nb] and not hex_state[nb].visited then
                table.insert(stack, nb)
            end
        end

        ::continue::
    end

    return corridor
end

-- Write corridor plan to SQLite via shelling out to sqlite3.
local function write_corridor_plan(db_path, corridor_id, corridor)
    -- Create table if not exists.
    local create_cmd = string.format(
        "sqlite3 %s \"CREATE TABLE IF NOT EXISTS corridor_plan (" ..
        "corridor_id INTEGER, h3_index TEXT, benefit REAL, cumulative_cost REAL);\"",
        db_path
    )
    os.execute(create_cmd)

    for _, entry in ipairs(corridor) do
        local insert_cmd = string.format(
            "sqlite3 %s \"INSERT INTO corridor_plan " ..
            "(corridor_id, h3_index, benefit, cumulative_cost) " ..
            "VALUES (%d, '%s', %f, %f);\"",
            db_path, corridor_id, entry.h3_index, entry.benefit, entry.cumulative_cost
        )
        os.execute(insert_cmd)
    end
end

-- Public API: plan a restorative corridor from a starting hex.
function CorridorPlanner.plan(db_path, corridor_id, start_h3, benefit_threshold)
    -- hex_state should be populated beforehand from SQL (e.g., hex_thermal_recovery).
    -- Here we mock some values.
    hex_state["hex-A"] = {green_fraction = 0.2, deltaT = 4.5, cost_per_kJ = 0.01, visited = false}
    hex_state["hex-B"] = {green_fraction = 0.3, deltaT = 3.8, cost_per_kJ = 0.015, visited = false}
    hex_state["hex-C"] = {green_fraction = 0.4, deltaT = 5.2, cost_per_kJ = 0.02, visited = false}
    hex_state["hex-D"] = {green_fraction = 0.5, deltaT = 2.5, cost_per_kJ = 0.012, visited = false}

    local corridor = traverse_corridor(start_h3, benefit_threshold)
    write_corridor_plan(db_path, corridor_id, corridor)
end

-- Example usage:
-- CorridorPlanner.plan("./data/cyboquatic_workload.db", 1, "hex-A", 200.0)

return CorridorPlanner
