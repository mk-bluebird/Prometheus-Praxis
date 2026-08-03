-- File: lua/cyboquatic/fog_cli.lua
-- Simple FOG-router CLI that reads telemetry from a CSV file and
-- routes each sample into a FOG band using existing predicates.
--
-- Expected CSV columns (header optional):
--   node_code,temperatureC,pfasConcentrationUgL,dissolvedO2MgL,turbidityNTU
--
-- Usage:
--   lua fog_cli.lua telemetry.csv

local FogRouter = require("cyboquatic.fog_router_predicates")

local function split_csv_line(line)
    local fields = {}
    for field in string.gmatch(line, "([^,]+)") do
        fields[#fields + 1] = field
    end
    return fields
end

local function parse_number(s)
    return tonumber(s) or 0.0
end

local function route_file(path)
    local fh = io.open(path, "r")
    if not fh then
        io.stderr:write("Failed to open CSV: " .. path .. "\n")
        os.exit(1)
    end

    -- Try to detect header.
    local first = fh:read("*l")
    if not first then
        io.stderr:write("Empty CSV: " .. path .. "\n")
        fh:close()
        os.exit(1)
    end

    local fields = split_csv_line(first)
    local has_header = false
    if #fields >= 5 and fields[1]:lower():find("node") then
        has_header = true
    else
        -- Treat first line as data.
        local node_code = fields[1]
        local sample = {
            mediumType = "water",
            temperatureC = parse_number(fields[2]),
            pfasConcentrationUgL = parse_number(fields[3]),
            dissolvedO2MgL = parse_number(fields[4]),
            turbidityNTU = parse_number(fields[5])
        }
        local route = FogRouter.route(sample)
        print(node_code .. "," .. route)
    end

    if has_header then
        -- Process remaining lines as data.
        for line in fh:lines() do
            if #line > 0 then
                local cols = split_csv_line(line)
                if #cols >= 5 then
                    local node_code = cols[1]
                    local sample = {
                        mediumType = "water",
                        temperatureC = parse_number(cols[2]),
                        pfasConcentrationUgL = parse_number(cols[3]),
                        dissolvedO2MgL = parse_number(cols[4]),
                        turbidityNTU = parse_number(cols[5])
                    }
                    local route = FogRouter.route(sample)
                    print(node_code .. "," .. route)
                end
            end
        end
    else
        -- Already processed first data line; process remaining.
        for line in fh:lines() do
            if #line > 0 then
                local cols = split_csv_line(line)
                if #cols >= 5 then
                    local node_code = cols[1]
                    local sample = {
                        mediumType = "water",
                        temperatureC = parse_number(cols[2]),
                        pfasConcentrationUgL = parse_number(cols[3]),
                        dissolvedO2MgL = parse_number(cols[4]),
                        turbidityNTU = parse_number(cols[5])
                    }
                    local route = FogRouter.route(sample)
                    print(node_code .. "," .. route)
                end
            end
        end
    end

    fh:close()
end

local function main()
    if #arg < 1 then
        io.stderr:write("Usage: lua fog_cli.lua <telemetry.csv>\n")
        os.exit(1)
    end
    route_file(arg[1])
end

main()
