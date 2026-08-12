-- File: lua/eco_restoration/hex_action_dependency_lookup.lua

local sqlite3 = require("lsqlite3")

local query = [[
WITH RECURSIVE
dependency_tree(action_id, prerequisite_action_id, depth) AS (
    SELECT action_id, prerequisite_action_id, 1 FROM hex_action_dependency
    UNION ALL
    SELECT tree.action_id, dependency.prerequisite_action_id, tree.depth + 1
    FROM dependency_tree AS tree
    JOIN hex_action_dependency AS dependency ON dependency.action_id = tree.prerequisite_action_id
),
dependency_depth(action_id, maximum_depth) AS (
    SELECT action_id, MAX(depth) FROM dependency_tree GROUP BY action_id
)
SELECT node.action_id,node.action,node.priority,COALESCE(depth.maximum_depth,0) AS dependency_depth
FROM hex_action_node AS node
LEFT JOIN dependency_depth AS depth ON depth.action_id=node.action_id
WHERE node.hex_anchor=? AND node.state='PENDING'
AND NOT EXISTS (
    SELECT 1 FROM dependency_tree AS tree
    JOIN hex_action_node AS prerequisite ON prerequisite.action_id=tree.prerequisite_action_id
    WHERE tree.action_id=node.action_id AND prerequisite.state<>'COMPLETED'
)
ORDER BY dependency_depth ASC,node.priority DESC,node.action_id ASC;
]]

local function feasible_actions(database_path, hex_anchor)
    local database = assert(sqlite3.open(database_path))
    local statement = assert(database:prepare(query))
    statement:bind_values(hex_anchor)

    local actions = {}
    for row in statement:nrows() do
        actions[#actions + 1] = {
            action_id = row.action_id,
            action = row.action,
            priority = row.priority,
            dependency_depth = row.dependency_depth
        }
    end

    statement:finalize()
    database:close()
    return actions
end

return { feasible_actions = feasible_actions }
