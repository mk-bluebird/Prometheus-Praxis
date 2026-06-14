-- filename crates/econet-hardware/lua/blast_radius_guard.lua
-- destination eco_restoration_shard/crates/econet-hardware/lua/blast_radius_guard.lua
-- repo-target github.com/mk-bluebird/eco_restoration_shard

-- Lua Module: Blast Radius and MT6883 Safety Guard
-- Role:
--   - Read-only guard over EcoNet SQLite spine
--   - Evaluates blast-radius and MT6883 invariants for a node
--   - Returns a monotone "safe" / "violation" verdict without actuating hardware

local blast_radius_guard = {}

blast_radius_guard.DB_PATH = os.getenv("ECONET_GOVERNANCE_DB") or "econet_constellation_index.sqlite3"
blast_radius_guard.RISK_MAX_R = 0.13
blast_radius_guard.MIN_LYAP_STABILITY = 0.85

local sqlite3 = require("lsqlite3")

local function with_db(path, fn)
    local db = sqlite3.open(path)
    if not db then
        return nil, "failed to open SQLite DB at " .. path
    end
    db:exec("PRAGMA foreign_keys = ON;")
    local ok, res, err = pcall(fn, db)
    db:close()
    if not ok then
        return nil, res
    end
    if not res then
        return nil, err
    end
    return res, nil
end

local function fetch_route_guard(db, nodeid)
    local stmt = db:prepare([[
        SELECT
            nodeid,
            region,
            max_physical_radius_meters,
            max_thermal_propagation_kelvin,
            max_acoustic_decibels,
            network_hop_containment,
            medium,
            max_permitted_attenuation,
            environmental_safety_floor,
            min_radius_m,
            max_radius_m,
            min_radius_h,
            max_radius_h,
            mean_risk_R,
            mean_knowledge_K,
            mean_energy_E
        FROM v_blast_radius_route_guard
        WHERE nodeid = ?
        LIMIT 1;
    ]])
    if not stmt then
        return nil, "failed to prepare v_blast_radius_route_guard query"
    end
    stmt:bind_values(nodeid)
    local row = stmt:nrows()()
    stmt:finalize()
    if not row then
        return nil, "no blast-radius zone registered for node " .. nodeid
    end
    return row, nil
end

local function fetch_latest_risk_chain(db, nodeid)
    local stmt = db:prepare([[
        SELECT
            risk_id,
            timestamp,
            unauthorized_mutation_attempt,
            risk_evidence_bundle
        FROM RISK_chain
        WHERE node_did = ?
        ORDER BY timestamp DESC, risk_id DESC
        LIMIT 1;
    ]])
    if not stmt then
        return nil, "failed to prepare RISK_chain query"
    end
    stmt:bind_values(nodeid)
    local row = stmt:nrows()()
    stmt:finalize()
    return row or {}, nil
end

local function boolean_flag(value)
    if value == nil then
        return false
    end
    if type(value) == "number" then
        return value ~= 0
    end
    if type(value) == "string" then
        return value ~= "0" and value ~= "" and value:lower() ~= "false"
    end
    return false
end

function blast_radius_guard.evaluate_node(nodeid)
    return with_db(blast_radius_guard.DB_PATH, function(db)
        local guard, err = fetch_route_guard(db, nodeid)
        if not guard then
            return {
                nodeid = nodeid,
                safe = false,
                reason = "NO_ZONE",
                details = err
            }
        end

        local physical_ok = true
        if guard.max_radius_m and guard.max_physical_radius_meters then
            physical_ok = guard.max_radius_m <= guard.max_physical_radius_meters
        end

        local risk_ok = true
        if guard.mean_risk_R then
            risk_ok = guard.mean_risk_R <= blast_radius_guard.RISK_MAX_R
        end

        local lyap_ok = true
        if guard.mean_knowledge_K then
            lyap_ok = guard.mean_knowledge_K >= blast_radius_guard.MIN_LYAP_STABILITY
        end

        local risk_row, _ = fetch_latest_risk_chain(db, nodeid)
        local mt6883_ok = true
        if risk_row and risk_row.unauthorized_mutation_attempt ~= nil then
            mt6883_ok = tonumber(risk_row.unauthorized_mutation_attempt) == 0
        end

        local safe = physical_ok and risk_ok and lyap_ok and mt6883_ok

        local reason
        if safe then
            reason = "OK"
        else
            if not physical_ok then
                reason = "PHYSICAL_ENVELOPE_EXCEEDED"
            elseif not risk_ok then
                reason = "RISK_R_COORDINATE_EXCEEDED"
            elseif not lyap_ok then
                reason = "LYAPUNOV_STABILITY_LOW"
            elseif not mt6883_ok then
                reason = "MT6883_UNAUTHORIZED_MUTATION"
            else
                reason = "UNKNOWN_VIOLATION"
            end
        end

        return {
            nodeid = nodeid,
            safe = safe,
            reason = reason,
            physical_ok = physical_ok,
            risk_ok = risk_ok,
            lyap_ok = lyap_ok,
            mt6883_ok = mt6883_ok,
            max_physical_radius_meters = guard.max_physical_radius_meters,
            max_radius_m = guard.max_radius_m,
            mean_risk_R = guard.mean_risk_R,
            mean_knowledge_K = guard.mean_knowledge_K,
            mean_energy_E = guard.mean_energy_E
        }
    end)
end

function blast_radius_guard.evaluate_route(source_nodeid, dest_nodeid)
    return with_db(blast_radius_guard.DB_PATH, function(db)
        local stmt = db:prepare([[
            SELECT
                route_id,
                routing_status,
                blast_safe
            FROM v_cyber_physical_routing_effective
            WHERE source_nodeid = ?
              AND destination_nodeid = ?
            LIMIT 1;
        ]])
        if not stmt then
            return {
                source_nodeid = source_nodeid,
                destination_nodeid = dest_nodeid,
                permitted = false,
                reason = "NO_ROUTE"
            }
        end
        stmt:bind_values(source_nodeid, dest_nodeid)
        local row = stmt:nrows()()
        stmt:finalize()

        if not row then
            return {
                source_nodeid = source_nodeid,
                destination_nodeid = dest_nodeid,
                permitted = false,
                reason = "NO_ROUTE"
            }
        end

        local blast_safe = boolean_flag(row.blast_safe)
        local status_ok = (row.routing_status == "ACTIVE_ROUTED")
        local permitted = blast_safe and status_ok

        local reason
        if permitted then
            reason = "OK"
        else
            if not status_ok then
                reason = "ROUTE_STATUS_BLOCKED"
            elseif not blast_safe then
                reason = "BLAST_RADIUS_VIOLATION"
            else
                reason = "UNKNOWN_BLOCK"
            end
        end

        return {
            source_nodeid = source_nodeid,
            destination_nodeid = dest_nodeid,
            route_id = row.route_id,
            permitted = permitted,
            reason = reason,
            blast_safe = blast_safe,
            routing_status = row.routing_status
        }
    end)
end

return blast_radius_guard
