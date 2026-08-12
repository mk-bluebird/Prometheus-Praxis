-- File: lua/eco_restoration/noise_light_risk_plugin.lua

local function clamp01(value)
    return math.max(0.0, math.min(1.0, value))
end

function risk(raw)
    local noise_risk = clamp01((raw.noise_dba - 45.0) / 30.0)
    local light_risk = clamp01((raw.light_lux - 2.0) / 18.0)
    return clamp01(0.60 * noise_risk + 0.40 * light_risk)
end
