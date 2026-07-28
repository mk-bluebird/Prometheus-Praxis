-- filename: src/lua/cyboquatic_fogrouter_mainlanes.lua
-- license: MIT OR Apache-2.0
-- role: Non-actuating FOG-router predicates for cyboquatic workload, drainage, and energy/restoration frames.

local M = {}

local LANE_RESEARCH   = "RESEARCH"
local LANE_PILOT      = "PILOT"
local LANE_PRODUCTION = "PRODUCTION"

local function clamp01(x)
    if x < 0.0 then
        return 0.0
    elseif x > 1.0 then
        return 1.0
    else
        return x
    end
end

local function normalize_topology(topology_hint)
    if topology_hint == nil then
        return 0.5
    end
    return clamp01(topology_hint)
end

local function aggregate_score(k, e, r, topo)
    local k1 = clamp01(k or 0.0)
    local e1 = clamp01(e or 0.0)
    local r1 = clamp01(r or 0.0)
    local t1 = normalize_topology(topo)
    local base = k1 * 0.4 + e1 * 0.4 - r1 * 0.3
    local topo_adjust = (t1 - 0.5) * 0.2
    return base + topo_adjust
end

----------------------------------------------------------------------
-- Workload frame predicates
-- frame: {
--   k = number in [0,1],
--   e = number in [0,1],
--   r = number in [0,1],
--   topology = number in [0,1] or nil,
--   vt = number,
--   roh = number in [0,1],
-- }
----------------------------------------------------------------------

function M.is_safe_research_frame(frame)
    if not frame then
        return false
    end
    local k = clamp01(frame.k or 0.0)
    local e = clamp01(frame.e or 0.0)
    local r = clamp01(frame.r or 0.0)
    local roh = clamp01(frame.roh or 0.0)
    local vt = frame.vt or 0.0
    local topo = frame.topology
    local score = aggregate_score(k, e, r, topo)
    if roh > 0.9 then
        return false
    end
    if vt > 1.5 then
        return false
    end
    return score >= -0.2
end

function M.is_safe_pilot_frame(frame)
    if not frame then
        return false
    end
    local k = clamp01(frame.k or 0.0)
    local e = clamp01(frame.e or 0.0)
    local r = clamp01(frame.r or 0.0)
    local roh = clamp01(frame.roh or 0.0)
    local vt = frame.vt or 0.0
    local topo = frame.topology
    if k < 0.70 then
        return false
    end
    if e < 0.70 then
        return false
    end
    if r > 0.25 then
        return false
    end
    if roh > 0.7 then
        return false
    end
    if vt > 1.2 then
        return false
    end
    local score = aggregate_score(k, e, r, topo)
    return score >= 0.1
end

function M.is_safe_production_frame(frame)
    if not frame then
        return false
    end
    local k = clamp01(frame.k or 0.0)
    local e = clamp01(frame.e or 0.0)
    local r = clamp01(frame.r or 0.0)
    local roh = clamp01(frame.roh or 0.0)
    local vt = frame.vt or 0.0
    local topo = frame.topology
    if k < 0.90 then
        return false
    end
    if e < 0.90 then
        return false
    end
    if r > 0.13 then
        return false
    end
    if roh > 0.5 then
        return false
    end
    if vt > 1.0 then
        return false
    end
    local score = aggregate_score(k, e, r, topo)
    return score >= 0.3
end

function M.classify_workload_lane(frame)
    if M.is_safe_production_frame(frame) then
        return LANE_PRODUCTION
    end
    if M.is_safe_pilot_frame(frame) then
        return LANE_PILOT
    end
    if M.is_safe_research_frame(frame) then
        return LANE_RESEARCH
    end
    return nil
end

----------------------------------------------------------------------
-- Drainage-decay frame predicates
-- frame: {
--   k = number in [0,1],
--   e = number in [0,1],
--   r = number in [0,1],
--   topology = number in [0,1] or nil,
--   bod_mg_l = number,
--   tss_mg_l = number,
--   cec_cmol_per_kg = number,
-- }
----------------------------------------------------------------------

local function drainage_in_bands(frame)
    local bod = frame.bod_mg_l or 0.0
    local tss = frame.tss_mg_l or 0.0
    local cec = frame.cec_cmol_per_kg or 0.0
    if bod < 0.0 or bod > 80.0 then
        return false
    end
    if tss < 0.0 or tss > 500.0 then
        return false
    end
    if cec < 0.0 or cec > 100.0 then
        return false
    end
    return true
end

function M.is_safe_research_drainage_frame(frame)
    if not frame then
        return false
    end
    if not drainage_in_bands(frame) then
        return false
    end
    local k = clamp01(frame.k or 0.0)
    local e = clamp01(frame.e or 0.0)
    local r = clamp01(frame.r or 0.0)
    local topo = frame.topology
    local score = aggregate_score(k, e, r, topo)
    return score >= -0.2
end

function M.is_safe_pilot_drainage_frame(frame)
    if not frame then
        return false
    end
    if not drainage_in_bands(frame) then
        return false
    end
    local k = clamp01(frame.k or 0.0)
    local e = clamp01(frame.e or 0.0)
    local r = clamp01(frame.r or 0.0)
    local topo = frame.topology
    if k < 0.70 then
        return false
    end
    if e < 0.70 then
        return false
    end
    if r > 0.25 then
        return false
    end
    local score = aggregate_score(k, e, r, topo)
    return score >= 0.1
end

function M.is_safe_production_drainage_frame(frame)
    if not frame then
        return false
    end
    if not drainage_in_bands(frame) then
        return false
    end
    local k = clamp01(frame.k or 0.0)
    local e = clamp01(frame.e or 0.0)
    local r = clamp01(frame.r or 0.0)
    local topo = frame.topology
    if k < 0.90 then
        return false
    end
    if e < 0.90 then
        return false
    end
    if r > 0.13 then
        return false
    end
    local score = aggregate_score(k, e, r, topo)
    return score >= 0.3
end

function M.classify_drainage_lane(frame)
    if M.is_safe_production_drainage_frame(frame) then
        return LANE_PRODUCTION
    end
    if M.is_safe_pilot_drainage_frame(frame) then
        return LANE_PILOT
    end
    if M.is_safe_research_drainage_frame(frame) then
        return LANE_RESEARCH
    end
    return nil
end

----------------------------------------------------------------------
-- Energy/ecoperJoule restoration frame predicates
-- frame: {
--   k = number in [0,1],
--   e = number in [0,1],
--   r = number in [0,1],
--   topology = number in [0,1] or nil,
--   ecoperjoule = number,
--   carbon_negative_ok = boolean (0/1),
-- }
----------------------------------------------------------------------

local function energy_restoration_in_bands(frame)
    local ecoper = frame.ecoperjoule or 0.0
    if ecoper < 0.0 or ecoper > 1000.0 then
        return false
    end
    if frame.carbon_negative_ok ~= 1 then
        return false
    end
    return true
end

function M.is_safe_production_energy_restoration_frame(frame)
    if not frame then
        return false
    end
    if not energy_restoration_in_bands(frame) then
        return false
    end
    local k = clamp01(frame.k or 0.0)
    local e = clamp01(frame.e or 0.0)
    local r = clamp01(frame.r or 0.0)
    local topo = frame.topology
    if k < 0.90 then
        return false
    end
    if e < 0.90 then
        return false
    end
    if r > 0.13 then
        return false
    end
    local score = aggregate_score(k, e, r, topo)
    return score >= 0.3
end

----------------------------------------------------------------------
-- Public API
----------------------------------------------------------------------

return M
