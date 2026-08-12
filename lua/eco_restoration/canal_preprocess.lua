-- File: lua/eco_restoration/canal_preprocess.lua

local input_path, output_path = arg[1], arg[2]
if not input_path or not output_path then
    error("usage: lua canal_preprocess.lua input.csv normalized_canal.csv")
end

local input = assert(io.open(input_path, "r"))
local output = assert(io.open(output_path, "w"))
local rows = {}
local sums = {0.0, 0.0, 0.0}
local sums_sq = {0.0, 0.0, 0.0}

input:read("*l")
for line in input:lines() do
    local timestamp, flow, head, sediment =
        line:match("^([^,]+),([^,]+),([^,]+),([^,]+)$")
    flow, head, sediment = tonumber(flow), tonumber(head), tonumber(sediment)
    if not timestamp or not flow or not head or not sediment or flow < 0 or head < 0 or sediment < 0 then
        error("invalid canal observation")
    end
    local values = {flow, head, sediment}
    for i = 1, 3 do
        sums[i] = sums[i] + values[i]
        sums_sq[i] = sums_sq[i] + values[i] * values[i]
    end
    rows[#rows + 1] = {timestamp, flow, head, sediment}
end
input:close()

if #rows < 8 then
    error("at least eight observations are required")
end

local means, deviations = {}, {}
for i = 1, 3 do
    means[i] = sums[i] / #rows
    deviations[i] = math.sqrt(math.max(1e-12, sums_sq[i] / #rows - means[i] * means[i]))
end

output:write("timestamp,flow,head,sediment\n")
for _, row in ipairs(rows) do
    output:write(string.format(
        "%s,%.17g,%.17g,%.17g\n",
        row[1],
        (row[2] - means[1]) / deviations[1],
        (row[3] - means[2]) / deviations[2],
        (row[4] - means[3]) / deviations[3]
    ))
end
output:close()
