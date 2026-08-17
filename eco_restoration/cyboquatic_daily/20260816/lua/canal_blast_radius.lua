local function clamp01(value)
    if value < 0.0 then
        return 0.0
    end
    if value > 1.0 then
        return 1.0
    end
    return value
end

local function number_argument(arguments, index, name)
    local value = tonumber(arguments[index])
    assert(value, name .. " must be numeric")
    return value
end

local function assess(breach_flow_lps, surcharge_duration_s, bank_sensitivity, distance_m, energyreqJ, delta_vt)
    assert(breach_flow_lps > 0.0, "breach_flow_lps must be positive")
    assert(surcharge_duration_s > 0.0, "surcharge_duration_s must be positive")
    assert(bank_sensitivity >= 0.0 and bank_sensitivity <= 1.0, "bank_sensitivity must be 0..1")
    assert(distance_m >= 0.0 and energyreqJ >= 0.0 and delta_vt >= 0.0, "remaining inputs must be non-negative")

    local base_radius_m = math.sqrt(breach_flow_lps * surcharge_duration_s) / 10.0
    local conservative_radius_m = base_radius_m * (1.0 + bank_sensitivity * 1.5)
    local exposure = 0.0
    if conservative_radius_m > 0.0 then
        exposure = clamp01(1.0 - distance_m / conservative_radius_m)
    end

    local energy_load = clamp01(energyreqJ / 1000000.0)
    local velocity_load = clamp01(delta_vt / 10.0)
    local harm_risk = clamp01(
        0.60 * exposure + 0.20 * bank_sensitivity + 0.10 * energy_load + 0.10 * velocity_load
    )
    local knowledge_factor = clamp01(1.0 - 0.35 * bank_sensitivity - 0.25 * energy_load)
    local eco_impact_value = clamp01((1.0 - harm_risk) * (0.40 + 0.60 * knowledge_factor))

    local zone, machine_action = "SAFE", "OPERATE_LOW_IMPACT"
    if harm_risk >= 0.60 then
        zone, machine_action = "EXCLUDE", "NO_ENTRY"
    elseif harm_risk > 0.25 then
        zone, machine_action = "CAUTION", "HOLD_FOR_INSPECTION"
    end

    return {
        base_radius_m = base_radius_m,
        conservative_radius_m = conservative_radius_m,
        knowledge_factor = knowledge_factor,
        eco_impact_value = eco_impact_value,
        harm_risk = harm_risk,
        zone = zone,
        machine_action = machine_action
    }
end

if #arg ~= 6 then
    io.stderr:write(
        "usage: lua canal_blast_radius.lua <breach_flow_lps> <surcharge_duration_s> " ..
        "<bank_sensitivity_0_to_1> <distance_m> <energyreqJ> <delta_vt>\n"
    )
    os.exit(64)
end

local ok, result = pcall(
    assess,
    number_argument(arg, 1, "breach_flow_lps"),
    number_argument(arg, 2, "surcharge_duration_s"),
    number_argument(arg, 3, "bank_sensitivity"),
    number_argument(arg, 4, "distance_m"),
    number_argument(arg, 5, "energyreqJ"),
    number_argument(arg, 6, "delta_vt")
)

if not ok then
    io.stderr:write("error: " .. result .. "\n")
    os.exit(65)
end

print(string.format("base_radius_m=%.3f", result.base_radius_m))
print(string.format("conservative_radius_m=%.3f", result.conservative_radius_m))
print(string.format("knowledge_factor=%.3f", result.knowledge_factor))
print(string.format("eco_impact_value=%.3f", result.eco_impact_value))
print(string.format("harm_risk=%.3f", result.harm_risk))
print("zone=" .. result.zone)
print("machine_action=" .. result.machine_action)
