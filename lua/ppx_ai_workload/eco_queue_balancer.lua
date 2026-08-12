-- File: lua/ppx_ai_workload/eco_queue_balancer.lua
local queue_balancer = {}

local function valid_queue(queue)
    return type(queue) == "table" and
        type(queue.queue_id) == "string" and queue.queue_id ~= "" and
        type(queue.ecological_impact_score) == "number" and
        queue.ecological_impact_score >= 0.0 and
        type(queue.estimated_energy_j) == "number" and
        queue.estimated_energy_j >= 0.0
end

function queue_balancer.select_lowest_impact(active_queues)
    if type(active_queues) ~= "table" or #active_queues == 0 then
        return nil, "no_active_queues"
    end

    local candidates = {}
    for _, queue in ipairs(active_queues) do
        if valid_queue(queue) then
            candidates[#candidates + 1] = queue
        end
    end
    if #candidates == 0 then
        return nil, "no_valid_queues"
    end

    table.sort(candidates, function(left, right)
        if left.ecological_impact_score ~= right.ecological_impact_score then
            return left.ecological_impact_score < right.ecological_impact_score
        end
        if left.estimated_energy_j ~= right.estimated_energy_j then
            return left.estimated_energy_j < right.estimated_energy_j
        end
        return left.queue_id < right.queue_id
    end)

    return candidates[1], "lowest_ecological_impact"
end

return queue_balancer
