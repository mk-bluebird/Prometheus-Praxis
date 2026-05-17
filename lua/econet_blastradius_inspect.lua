-- filename: lua/econet_blastradius_inspect.lua
-- destination: mk-bluebird/eco_restoration_shard/lua/econet_blastradius_inspect.lua
-- role: Lua‑side, non‑actuating inspector for blastradius + energy ledger

local sqlite3 = require("lsqlite3")

local M = {}

function M.open_spine(path)
    local db = sqlite3.open(path)
    return db
end

function M.close_spine(db)
    if db then
        db:close()
    end
end

function M.list_candidate_ecorestorative(db, limit)
    limit = limit or 20
    local stmt = db:prepare([[
        SELECT source_type, source_id, impact_carbon, impact_biodiv,
               vt_sensitivity_avg, dv_avg
        FROM v_candidate_ecorestorative
        ORDER BY impact_carbon DESC, impact_biodiv DESC
        LIMIT ?1;
    ]])
    stmt:bind_values(limit)

    local results = {}
    for row in stmt:nrows() do
        table.insert(results, {
            source_type = row.source_type,
            source_id = row.source_id,
            impact_carbon = row.impact_carbon,
            impact_biodiv = row.impact_biodiv,
            vt_sensitivity_avg = row.vt_sensitivity_avg,
            dv_avg = row.dv_avg
        })
    end
    stmt:finalize()
    return results
end

function M.best_nodes_for_energy_tailwind(db, limit)
    limit = limit or 20
    local stmt = db:prepare([[
        SELECT node_id, n_events, e_req_accept_j, e_surplus_accept_j,
               r_carbon_avg, r_biodiv_avg, dv_avg
        FROM v_node_energy_carbon
        WHERE n_events >= 5
          AND dv_avg <= 0.0
          AND e_surplus_accept_j >= e_req_accept_j
        ORDER BY r_carbon_avg ASC, e_req_accept_j ASC
        LIMIT ?1;
    ]])
    stmt:bind_values(limit)

    local results = {}
    for row in stmt:nrows() do
        table.insert(results, {
            node_id = row.node_id,
            n_events = row.n_events,
            e_req_accept_j = row.e_req_accept_j,
            e_surplus_accept_j = row.e_surplus_accept_j,
            r_carbon_avg = row.r_carbon_avg,
            r_biodiv_avg = row.r_biodiv_avg,
            dv_avg = row.dv_avg
        })
    end
    stmt:finalize()
    return results
end

return M
