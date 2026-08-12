-- File: lua/ppx_ai_workload/median_ring_filter.lua
local median_ring_filter = {}
median_ring_filter.__index = median_ring_filter

function median_ring_filter.new(window_size, field_name)
    assert(type(window_size) == "number" and window_size >= 3 and window_size % 2 == 1)
    assert(type(field_name) == "string" and field_name ~= "")
    return setmetatable({
        window_size = window_size,
        field_name = field_name,
        values = {},
        next_slot = 1,
        count = 0
    }, median_ring_filter)
end

local function median(values)
    local sorted = {}
    for i = 1, #values do sorted[i] = values[i] end
    table.sort(sorted, function(left, right) return left < right end)
    return sorted[(#sorted + 1) // 2]
end

function median_ring_filter:push(record)
    assert(type(record) == "table")
    local value = record[self.field_name]
    assert(type(value) == "number" and value == value)

    self.values[self.next_slot] = value
    self.next_slot = (self.next_slot % self.window_size) + 1
    self.count = math.min(self.count + 1, self.window_size)

    local active = {}
    for i = 1, self.count do active[i] = self.values[i] end
    local filtered = median(active)

    local output = {}
    for key, item in pairs(record) do output[key] = item end
    output[self.field_name .. "_median"] = filtered
    output[self.field_name .. "_sample_count"] = self.count
    return output
end

return median_ring_filter
