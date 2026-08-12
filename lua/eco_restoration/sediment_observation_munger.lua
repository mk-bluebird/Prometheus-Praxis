-- File: lua/eco_restoration/sediment_observation_munger.lua

local input_path, output_path = arg[1], arg[2]
if not input_path or not output_path then
    error("usage: lua sediment_observation_munger.lua raw.csv cleaned.csv")
end

local input = assert(io.open(input_path, "r"))
local output = assert(io.open(output_path, "w"))
local rows = {}

input:read("*l")
for line in input:lines() do
    local velocity, concentration, d50, deposition =
        line:match("^%s*([^,]+),%s*([^,]+),%s*([^,]+),%s*([^,]+)%s*$")
    velocity, concentration, d50, deposition =
        tonumber(velocity), tonumber(concentration), tonumber(d50), tonumber(deposition)

    if not velocity or not concentration or not d50 or not deposition or
       velocity < 0 or concentration < 0 or d50 <= 0 or deposition < 0 then
        error("invalid sediment observation")
    end

    rows[#rows + 1] = {velocity, concentration, d50, deposition}
end
input:close()

if #rows < 4 then
    error("at least four sediment observations are required")
end

output:write("velocity_m_s,concentration_kg_m3,d50_m,observed_deposition_kg_m2\n")
for _, row in ipairs(rows) do
    output:write(string.format("%.17g,%.17g,%.17g,%.17g\n", row[1], row[2], row[3], row[4]))
end
output:close()
