-- File: lua/cyboquatic/fog_router_predicates.lua

local FogRouter = {}

local function is_cold_survival_corridor(sample)
    return sample.temperatureC <= 12.0 and sample.pfasConcentrationUgL >= 0.1
end

local function is_eco_restoration_ready(sample)
    local o2Safe = sample.dissolvedO2MgL >= 5.0
    local turbiditySafe = sample.turbidityNTU <= 50.0
    local pfasLow = sample.pfasConcentrationUgL < 0.05
    return o2Safe and turbiditySafe and pfasLow
end

function FogRouter.route(sample)
    if is_cold_survival_corridor(sample) then
        return "FOG:COLD_SURVIVAL_MONITOR"
    elseif is_eco_restoration_ready(sample) then
        return "FOG:RESTORATION_PREFERRED"
    else
        return "FOG:NEEDS_DIAGNOSTIC"
    end
end

function FogRouter.classify_media(viscosity_cP, turbidity_NTU, organic_fraction)
    if viscosity_cP < 5 and turbidity_NTU < 50 and organic_fraction > 0.7 then
        return "SAFE"
    elseif viscosity_cP < 50 and turbidity_NTU < 200 and organic_fraction > 0.3 then
        return "CAUTION"
    else
        return "BLOCK"
    end
end

function FogRouter.predicate_score(viscosity_cP, turbidity_NTU, organic_fraction)
    local v_score = math.max(0, 1 - (viscosity_cP / 100))
    local t_score = math.max(0, 1 - (turbidity_NTU / 500))
    local o_score = math.max(0, math.min(organic_fraction, 1))
    return (v_score + t_score + o_score) / 3
end

function FogRouter.suggest_route(predicate_score, canal_capacity_m3_s)
    if predicate_score >= 0.8 and canal_capacity_m3_s >= 0.1 then
        return "PRIMARY_CANAL"
    elseif predicate_score >= 0.5 and canal_capacity_m3_s >= 0.05 then
        return "SECONDARY_CANAL"
    else
        return "HOLD_TANK"
    end
end

function FogRouter.route_from_row(row)
    local deltaVt = row.deltaVt or 0
    local topoStress = row.topo_stress_norm or 0
    local tempC = row.canal_temperature_C or 15
    local pfasUgL = row.pfas_concentration_ugL or 0

    local approxDO2 = math.max(0, 8.0 - deltaVt * 5.0)
    local approxTurbidity = 20.0 + topoStress * 60.0

    local sample = {
        mediumType = "water",
        temperatureC = tempC,
        pfasConcentrationUgL = pfasUgL,
        dissolvedO2MgL = approxDO2,
        turbidityNTU = approxTurbidity
    }

    return FogRouter.route(sample)
end

local function demo()
    local sample = {
        mediumType = "sediment",
        temperatureC = 8.5,
        pfasConcentrationUgL = 0.25,
        dissolvedO2MgL = 4.5,
        turbidityNTU = 80.0
    }
    local route = FogRouter.route(sample)
    print("Lua FOG route: " .. route)
end

local function run_cli(nodeCode, dbPath)
    nodeCode = nodeCode or "PHX_CANAL_NODE_A"
    dbPath = dbPath or "eco_restoration_workload.sqlite"

    local query = string.format(
        "sqlite3 '%s' \"SELECT deltaVt, topo_stress_norm, canal_temperature_C, pfas_concentration_ugL " ..
        "FROM cyboquatic_workload_telemetry ct " ..
        "JOIN canal_node cn ON cn.node_id = ct.node_id " ..
        "WHERE cn.node_code = '%s' ORDER BY ct.timestamp_utc DESC LIMIT 1;\"",
        dbPath, nodeCode
    )

    local handle = io.popen(query)
    if not handle then
        print("Error: Could not open sqlite3 pipe")
        return
    end

    local result = handle:read("*a")
    handle:close()

    if not result or result == "" then
        print("No telemetry data found for node: " .. nodeCode)
        return
    end

    local parts = {}
    for part in result:gmatch("[^|]+") do
        table.insert(parts, tonumber(part) or part)
    end

    if #parts >= 4 then
        local row = {
            deltaVt = parts[1],
            topo_stress_norm = parts[2],
            canal_temperature_C = parts[3],
            pfas_concentration_ugL = parts[4]
        }

        local route = FogRouter.route_from_row(row)
        print("Lua FOG Router")
        print("Node: " .. nodeCode)
        print("DB: " .. dbPath)
        print(string.format(
            "Latest telemetry: deltaVt=%.4f, topoStress=%.4f, temp=%.2fC, pfas=%.4f ug/L",
            row.deltaVt, row.topo_stress_norm, row.canal_temperature_C, row.pfas_concentration_ugL
        ))
        print("Decided FOG route: " .. route)
    else
        print("Error parsing SQLite output: " .. result)
    end
end

if arg and #arg > 0 then
    local nodeCode
    local dbPath

    for _, a in ipairs(arg) do
        if a:match("^--node=") then
            nodeCode = a:sub(8)
        elseif a:match("^--db-path=") then
            dbPath = a:sub(11)
        end
    end

    run_cli(nodeCode, dbPath)
else
    demo()
end

return FogRouter
