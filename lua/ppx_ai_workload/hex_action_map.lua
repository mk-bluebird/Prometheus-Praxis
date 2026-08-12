-- File: lua/ppx_ai_workload/hex_action_map.lua
local actions_by_hex = require("ppx_ai_workload.generated_hex_actions")

local hex_action_map = {}

function hex_action_map.permitted(hex_anchor)
    local actions = actions_by_hex[hex_anchor]
    if not actions then return {} end

    local copy = {}
    for index, action in ipairs(actions) do copy[index] = action end
    return copy
end

function hex_action_map.allows(hex_anchor, requested_action)
    for _, action in ipairs(actions_by_hex[hex_anchor] or {}) do
        if action == requested_action then return true end
    end
    return false
end

return hex_action_map
