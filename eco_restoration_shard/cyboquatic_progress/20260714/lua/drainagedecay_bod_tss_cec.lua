-- File: eco_restoration_shard/cyboquatic_progress/20260714/lua/drainagedecay_bod_tss_cec.lua
-- Domain (e): drainagedecay frames (BOD, TSS, CEC) for cyboquatic machinery.
-- Pure Lua 5.3+; suitable for embedding in low-power controllers.

local Drainagedecay = {}

local function first_order_decay(initial, k_per_hour, dt_hours)
    if initial <= 0.0 then
        return 0.0
    end
    if k_per_hour <= 0.0 or dt_hours == 0.0 then
        return initial
    end
    local exponent = -k_per_hour * dt_hours
    return initial * math.exp(exponent)
end

local function temperature_factor(theta, ref_temp_c, current_temp_c)
    local delta = current_temp_c - ref_temp_c
    return math.exp(math.log(theta) * (delta / 10.0))
end

function Drainagedecay.step(state, params, dt_hours)
    if dt_hours < 0.0 then
        error("dt_hours must be non-negative")
    end

    local temp_factor = temperature_factor(params.theta, params.ref_temp_c, state.temperature_c)

    local k_bod_per_hour = params.k_bod_per_day / 24.0 * temp_factor
    local k_tss_per_hour = params.k_tss_per_day / 24.0 * temp_factor

    local bod_next = first_order_decay(state.bod_mg_l, k_bod_per_hour, dt_hours)
    local tss_next = first_order_decay(state.tss_mg_l, k_tss_per_hour, dt_hours)

    local bod_clamped = bod_next
    if bod_clamped < 0.0 then bod_clamped = 0.0 end

    local tss_clamped = tss_next
    if tss_clamped < 0.0 then tss_clamped = 0.0 end

    return {
        bod_mg_l = bod_clamped,
        tss_mg_l = tss_clamped,
        cec_cmol_kg = state.cec_cmol_kg,
        temperature_c = state.temperature_c,
        flow_lps = state.flow_lps
    }
end

function Drainagedecay.oxygen_demand_mg_per_sec(state)
    local bod = state.bod_mg_l
    if bod < 0.0 then bod = 0.0 end
    local flow = state.flow_lps
    if flow < 0.0 then flow = 0.0 end
    return bod * flow / 1000.0
end

local function demo()
    local state = {
        bod_mg_l = 50.0,
        tss_mg_l = 100.0,
        cec_cmol_kg = 35.0,
        temperature_c = 24.0,
        flow_lps = 7.0
    }

    local params = {
        k_bod_per_day = 0.25,
        k_tss_per_day = 0.09,
        theta = 1.05,
        ref_temp_c = 20.0
    }

    local dt_hours = 2.5

    local next_state = Drainagedecay.step(state, params, dt_hours)
    local oxygen_demand = Drainagedecay.oxygen_demand_mg_per_sec(next_state)

    print("Initial BOD (mg/L):", state.bod_mg_l)
    print("Next BOD (mg/L):   ", next_state.bod_mg_l)
    print("Initial TSS (mg/L):", state.tss_mg_l)
    print("Next TSS (mg/L):   ", next_state.tss_mg_l)
    print("Oxygen demand (mg O2/s):", oxygen_demand)
end

if ... == nil then
    demo()
end

return Drainagedecay
