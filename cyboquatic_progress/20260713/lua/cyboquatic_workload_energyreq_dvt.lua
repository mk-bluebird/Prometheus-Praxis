-- filename: eco_restoration_shard/cyboquatic_progress/20260713/lua/cyboquatic_workload_energyreq_dvt.lua
-- domain: (d) Cyboquatic workload in Lua (FOG-free helper)
-- purpose: Lightweight Lua module to compute workload risk and K,E,R for edge devices.

local M = {}

local W_ENERGY = 0.8
local W_HYDRAULIC = 1.0
local W_UNCERTAINTY = 0.6

local ENERGY_TAILWIND_SAFE_RATIO = 1.2
local ENERGY_MIN_RATIO = 0.0
local ENERGY_MAX_RATIO = 2.5

local function clamp01(x)
    if x < 0.0 then return 0.0 end
    if x > 1.0 then return 1.0 end
    return x
end

local function normalize_risk(energy_req_j, energy_surplus_j, hydraulic_risk, uncertainty_risk)
    local ratio
    if energy_req_j <= 0.0 then
        ratio = ENERGY_MAX_RATIO
    else
        ratio = energy_surplus_j / energy_req_j
    end

    local renergy_raw
    if ratio >= ENERGY_TAILWIND_SAFE_RATIO then
        renergy_raw = 0.0
    elseif ratio <= ENERGY_MIN_RATIO then
        renergy_raw = 1.0
    else
        local bounded = ratio
        if bounded > ENERGY_MAX_RATIO then
            bounded = ENERGY_MAX_RATIO
        end
        local span = ENERGY_TAILWIND_SAFE_RATIO - ENERGY_MIN_RATIO
        local rel = (bounded - ENERGY_MIN_RATIO) / span
        renergy_raw = 1.0 - rel
        if renergy_raw < 0.0 then renergy_raw = 0.0 end
        if renergy_raw > 1.0 then renergy_raw = 1.0 end
    end

    local renergy = clamp01(renergy_raw)
    local rhydraulic = clamp01(hydraulic_risk)
    local runcertainty = clamp01(uncertainty_risk)

    return {
        renergy = renergy,
        rhydraulic = rhydraulic,
        runcertainty = runcertainty
    }
end

local function residual(risk)
    return W_ENERGY * risk.renergy * risk.renergy
        + W_HYDRAULIC * risk.rhydraulic * risk.rhydraulic
        + W_UNCERTAINTY * risk.runcertainty * risk.runcertainty
end

local function compute_ker(risk, vt_before)
    if vt_before < 0.0 then vt_before = 0.0 end
    local vt_after = residual(risk)
    local delta_vt = vt_after - vt_before

    local max_r = risk.renergy
    if risk.rhydraulic > max_r then max_r = risk.rhydraulic end
    if risk.runcertainty > max_r then max_r = risk.runcertainty end

    local k = 0.95 - 0.4 * max_r
    if delta_vt > 0.0 then
        k = k - 0.25
    end
    if k < 0.0 then k = 0.0 end
    if k > 1.0 then k = 1.0 end

    local e = 0.95 - vt_after
    if delta_vt > 0.0 then
        e = e - 0.3
    end
    if e < 0.0 then e = 0.0 end
    if e > 1.0 then e = 1.0 end

    local r = vt_after
    if delta_vt > 0.0 then
        r = r + delta_vt
    end
    if r < 0.0 then r = 0.0 end
    if r > 1.0 then r = 1.0 end

    return {
        vt = vt_after,
        delta_vt = delta_vt,
        k = k,
        e = e,
        r = r
    }
end

function M.evaluate_workload(energy_req_j, energy_surplus_j, hydraulic_risk, uncertainty_risk, vt_before)
    local risk = normalize_risk(energy_req_j, energy_surplus_j, hydraulic_risk, uncertainty_risk)
    local ker = compute_ker(risk, vt_before)
    return {
        renergy = risk.renergy,
        rhydraulic = risk.rhydraulic,
        runcertainty = risk.runcertainty,
        vt = ker.vt,
        delta_vt = ker.delta_vt,
        k = ker.k,
        e = ker.e,
        r = ker.r
    }
end

return M
