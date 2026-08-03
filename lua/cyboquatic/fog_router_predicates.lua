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

demo()

return FogRouter
