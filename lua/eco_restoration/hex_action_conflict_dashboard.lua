-- File: lua/eco_restoration/hex_action_conflict_dashboard.lua
local sqlite3 = require("lsqlite3")

local database_path = assert(arg[1], "usage: lua hex_action_conflict_dashboard.lua <database.sqlite>")
local database = assert(sqlite3.open(database_path))

local vectors = {}
for row in database:nrows([[
    SELECT hex_anchor, action_id, replica_id, counter
    FROM hex_action_version_vector
    ORDER BY hex_anchor, action_id, replica_id
]]) do
    local key = tostring(row.hex_anchor) .. "|" .. row.action_id
    vectors[key] = vectors[key] or {
        hex_anchor = row.hex_anchor,
        action_id = row.action_id,
        components = {}
    }
    vectors[key].components[row.replica_id] = tonumber(row.counter)
end

local grouped = {}
for _, vector in pairs(vectors) do
    grouped[vector.hex_anchor] = grouped[vector.hex_anchor] or {}
    table.insert(grouped[vector.hex_anchor], vector)
end

local function dominates(left, right)
    local strictly_greater = false
    local replicas = {}
    for replica in pairs(left.components) do replicas[replica] = true end
    for replica in pairs(right.components) do replicas[replica] = true end
    for replica in pairs(replicas) do
        local a = left.components[replica] or 0
        local b = right.components[replica] or 0
        if a < b then return false end
        if a > b then strictly_greater = true end
    end
    return strictly_greater
end

local conflicts = 0
for anchor, actions in pairs(grouped) do
    local concurrent = {}
    for i = 1, #actions do
        for j = i + 1, #actions do
            if not dominates(actions[i], actions[j]) and not dominates(actions[j], actions[i]) then
                concurrent[actions[i].action_id] = true
                concurrent[actions[j].action_id] = true
            end
        end
    end
    local choices = {}
    for action in pairs(concurrent) do table.insert(choices, action) end
    if #choices > 0 then
        table.sort(choices)
        conflicts = conflicts + 1
        io.write(string.format(
            "HEX %s requires operator selection: [%s]\n",
            tostring(anchor), table.concat(choices, ", ")
        ))
    end
end

database:close()
io.write(string.format("Conflict review complete: %d hexes require manual selection.\n", conflicts))
