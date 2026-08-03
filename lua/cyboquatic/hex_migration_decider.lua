-- File: lua/cyboquatic/hex_migration_decider.lua
-- Lua module for hex-to-hex workload migration approval using Lyapunov-KER corridor

local sqlite3 = require("lsqlite3")

local NET_DVT_MARGIN = 0.01
local HEX_VT_MARGIN  = 0.05

local function load_hex_residual(db, hex_id)
    local sql = [[
        SELECT total_delta_v_t, v_corridor_max
        FROM v_hex_stability_ker_dvt_carbon
        WHERE hex_id = ?
    ]]
    
    local stmt = db:prepare(sql)
    if not stmt then
        return nil, "Failed to prepare statement for hex " .. hex_id
    end
    
    stmt:bind_names(hex_id)
    local row = stmt:get_named_row()
    stmt:finalize()
    
    if not row then
        return nil, "Missing residual for hex " .. hex_id
    end
    
    return {
        vt = row.total_delta_v_t or 0.0,
        vt_corridor = row.v_corridor_max or 0.1
    }
end

local function estimate_dvt_for_workload(workload, hex_id)
    -- Check if workload has per-hex dvt estimates
    if workload.dvt_est and workload.dvt_est[hex_id] then
        return workload.dvt_est[hex_id]
    end
    -- Fall back to default dvt
    return workload.dvt_default or 0.0
end

local function approve_migration(db, workload, hexA, hexB)
    -- Load residual state for both hexes
    local resA, errA = load_hex_residual(db, hexA)
    if not resA then
        return false, errA
    end

    local resB, errB = load_hex_residual(db, hexB)
    if not resB then
        return false, errB
    end

    -- Estimate ΔVt impact for each hex
    local dvt_A = estimate_dvt_for_workload(workload, hexA)
    local dvt_B = estimate_dvt_for_workload(workload, hexB)

    -- Check network-wide residual change (ΔVnet margin)
    local delta_V_net = dvt_B - dvt_A
    if delta_V_net > NET_DVT_MARGIN then
        return false, "Network residual increase above margin: " .. delta_V_net
    end

    -- Check Hex A corridor after migration (workload leaving)
    -- When workload leaves A, its vt decreases by dvt_A contribution
    local vtA_after = resA.vt - dvt_A
    if vtA_after < 0 then
        vtA_after = 0
    end

    -- Check Hex B corridor after migration (workload arriving)
    local vtB_after = resB.vt + dvt_B

    if vtB_after > resB.vt_corridor + HEX_VT_MARGIN then
        return false, string.format(
            "Hex B residual exceeds corridor after migration: %.6f > %.6f",
            vtB_after, resB.vt_corridor + HEX_VT_MARGIN
        )
    end

    -- Log the approved migration to audit table
    local log_sql = [[
        INSERT INTO governance_query_audit (tool_name, caller_id, ker_k, ker_e, ker_r, ker_s, neuro_flag, lane_default, query_payload)
        VALUES ('hex_migration', ?, 0.0, 0.0, 0.0, 0.0, 0, 'RESEARCH', ?)
    ]]
    
    local stmt = db:prepare(log_sql)
    if stmt then
        local payload = string.format('{"from":"%s","to":"%s","dvt_A":%.6f,"dvt_B":%.6f}', hexA, hexB, dvt_A, dvt_B)
        stmt:bind_names(hexA, payload)
        stmt:step()
        stmt:finalize()
    end

    return true, "Migration approved from " .. hexA .. " to " .. hexB
end

-- CLI interface
local function main(db_path, workload_json, hexA, hexB)
    local db = sqlite3.open(db_path)
    if not db then
        print("Error: Could not open database " .. db_path)
        return 1
    end

    -- Simple JSON parsing for workload (expects format: {"dvt_default": X, "dvt_est": {...}})
    local workload = {}
    if workload_json then
        -- Very basic JSON parsing for demo purposes
        for k, v in string.gmatch(workload_json, '"([^"]+)"%s*:%s*([%d%.]+)') do
            workload[k] = tonumber(v)
        end
    end

    local approved, reason = approve_migration(db, workload, hexA, hexB)
    
    if approved then
        print(string.format('{"approved":true,"reason":"%s"}', reason))
    else
        print(string.format('{"approved":false,"reason":"%s"}', reason))
    end
    
    db:close()
    return approved and 0 or 1
end

-- Export functions for use as module
return {
    approve_migration = approve_migration,
    load_hex_residual = load_hex_residual,
    estimate_dvt_for_workload = estimate_dvt_for_workload,
    NET_DVT_MARGIN = NET_DVT_MARGIN,
    HEX_VT_MARGIN = HEX_VT_MARGIN
}
