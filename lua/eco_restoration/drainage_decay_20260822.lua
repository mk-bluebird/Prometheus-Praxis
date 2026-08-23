-- Repository: mk-bluebird/Prometheus-Praxis
-- Filename: lua/eco_restoration/drainage_decay_20260822.lua
-- Destination: lua/eco_restoration/

local drainage_decay = {}

local CEC_CAPACITY_CMOL_KG = 60.0

local function finite(value)
    return type(value) == "number" and value == value
        and value ~= math.huge and value ~= -math.huge
end

local function require_range(name, value, minimum, maximum)
    assert(finite(value) and value >= minimum and value <= maximum,
        name .. " is outside its permitted range")
end

local function clamp(value, minimum, maximum)
    if value < minimum then
        return minimum
    end
    if value > maximum then
        return maximum
    end
    return value
end

function drainage_decay.project_frame(input)
    assert(type(input) == "table", "input must be a table")

    require_range("hours", input.hours, 0.0, 24.0 * 365.0)
    require_range("initial_bod_mg_l", input.initial_bod_mg_l, 0.0, 100000.0)
    require_range("initial_tss_mg_l", input.initial_tss_mg_l, 0.0, 100000.0)
    require_range("initial_cec_cmol_kg", input.initial_cec_cmol_kg, 0.0, 200.0)
    require_range("bod_decay_per_hour", input.bod_decay_per_hour, 0.0, 1.0)
    require_range("tss_decay_per_hour", input.tss_decay_per_hour, 0.0, 1.0)
    require_range("cec_recovery_per_hour", input.cec_recovery_per_hour, 0.0, 1.0)
    require_range("energyreq_j", input.energyreq_j, 0.0, 1.0e12)
    require_range("delta_vt", input.delta_vt, -1000.0, 1000.0)
    require_range("sample_completeness", input.sample_completeness or 1.0, 0.0, 1.0)

    local bod_mg_l = input.initial_bod_mg_l * math.exp(-input.bod_decay_per_hour * input.hours)
    local tss_mg_l = input.initial_tss_mg_l * math.exp(-input.tss_decay_per_hour * input.hours)
    local cec_cmol_kg = CEC_CAPACITY_CMOL_KG
        - (CEC_CAPACITY_CMOL_KG - input.initial_cec_cmol_kg)
        * math.exp(-input.cec_recovery_per_hour * input.hours)

    local bod_quality = clamp(1.0 - bod_mg_l / 30.0, 0.0, 1.0)
    local tss_quality = clamp(1.0 - tss_mg_l / 30.0, 0.0, 1.0)
    local cec_quality = clamp(cec_cmol_kg / 30.0, 0.0, 1.0)
    local energy_quality = clamp(1.0 - input.energyreq_j / 5.0e6, 0.0, 1.0)
    local voltage_stability = clamp(1.0 - math.abs(input.delta_vt) / 24.0, 0.0, 1.0)

    return {
        hours = input.hours,
        bod_mg_l = bod_mg_l,
        tss_mg_l = tss_mg_l,
        cec_cmol_kg = cec_cmol_kg,
        energyreq_j = input.energyreq_j,
        delta_vt = input.delta_vt,
        knowledge_factor = clamp(
            0.65 * (input.sample_completeness or 1.0) + 0.35 * voltage_stability, 0.0, 1.0
        ),
        eco_impact_value = clamp(
            0.35 * bod_quality + 0.30 * tss_quality + 0.20 * cec_quality + 0.15 * energy_quality,
            0.0, 1.0
        ),
        harm_risk = clamp(
            1.0 - (0.40 * bod_quality + 0.35 * tss_quality + 0.15 * voltage_stability
                + 0.10 * energy_quality),
            0.0, 1.0
        )
    }
end

return drainage_decay
