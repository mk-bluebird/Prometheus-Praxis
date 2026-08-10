-- File: lua/cyboquatic/fog_router.lua
local sqlite3 = require("lsqlite3")
local corridor = require("cyboquatic.generated.workload_corridor_validator")

local M = {}

local function finite_number(value)
    return type(value) == "number"
        and value == value
        and value ~= math.huge
        and value ~= -math.huge
end

local function numeric(value)
    local converted = tonumber(value)
    return finite_number(converted) and converted or nil
end

local function route_decision(frame)
    local fog_concentration = numeric(frame.fog_concentration_mgL)
    local unmodeled_media = numeric(frame.unmodeled_media_flag)
    local residual_r = numeric(frame.R or frame.residual_R)
    local energy_requirement = numeric(frame.energyreqJ or frame.energyreq_j)

    if not fog_concentration or not unmodeled_media or not residual_r or not energy_requirement then
        return {
            accepted = false,
            route = "SAFE_TREATMENT_BASIN",
            reason = "Incomplete FOG, media, residual, or energy telemetry."
        }
    end

    if fog_concentration > 100.0 or unmodeled_media > 0.5 or residual_r > 0.7 then
        return {
            accepted = false,
            route = "SAFE_TREATMENT_BASIN",
            reason = "FOG, unmodeled media, or residual risk exceeds its corridor."
        }
    end

    if energy_requirement > 50000.0 then
        return {
            accepted = false,
            route = "EFFICIENCY_RETUNING",
            reason = "Energy requirement exceeds the machinery efficiency corridor."
        }
    end

    local required = {
        owner_did = frame.owner_did,
        canal_node = frame.canal_node or frame.canal_node_id,
        energyreqJ = energy_requirement,
        deltaVt = numeric(frame.deltaVt or frame.delta_vt),
        K = numeric(frame.K or frame.ker_k),
        E = numeric(frame.E or frame.ker_e),
        R = residual_r,
        fog_confidence = numeric(frame.fog_confidence),
        eco_impact_value = numeric(frame.eco_impact_value)
    }

    local valid, reason = corridor.validate_workload_frame(required)
    if not valid then
        return {
            accepted = false,
            route = "SAFE_TREATMENT_BASIN",
            reason = "ALN workload corridor rejected frame: " .. reason
        }
    end

    return {
        accepted = true,
        route = "STANDARD_CANAL_FLOW",
        reason = "FOG, energy, and ALN workload corridors verified."
    }
end

function M.route(frame)
    assert(type(frame) == "table", "route requires a telemetry frame")
    return route_decision(frame)
end

function M.evaluate_route(fog_conc_mgL, unmodeled_media_flag, residual_R, energyreqJ)
    return route_decision({
        fog_concentration_mgL = fog_conc_mgL,
        unmodeled_media_flag = unmodeled_media_flag,
        R = residual_R,
        energyreqJ = energyreqJ
    })
end

function M.run(db_path)
    assert(type(db_path) == "string" and #db_path > 0, "database path is required")

    local db = sqlite3.open(db_path)
    if not db then
        error("unable to open SQLite database: " .. db_path)
    end

    local sql = [[
        SELECT f.hex_id,
               f.canal_node_id,
               f.fog_concentration_mgL,
               f.unmodeled_media_flag,
               w.R,
               w.energyreqJ,
               w.owner_did,
               w.deltaVt,
               w.K,
               w.E,
               w.fog_confidence,
               w.eco_impact_value
        FROM fog_flow AS f
        LEFT JOIN workload_cycle AS w
          ON f.hex_id = w.hex_id
         AND f.canal_node_id = w.canal_node_id;
    ]]

    local success, failure = pcall(function()
        for row in db:nrows(sql) do
            local decision = route_decision({
                fog_concentration_mgL = row.fog_concentration_mgL,
                unmodeled_media_flag = row.unmodeled_media_flag,
                R = row.R,
                energyreqJ = row.energyreqJ,
                owner_did = row.owner_did,
                canal_node_id = row.canal_node_id,
                deltaVt = row.deltaVt,
                K = row.K,
                E = row.E,
                fog_confidence = row.fog_confidence,
                eco_impact_value = row.eco_impact_value
            })

            print(string.format(
                "hex=%s node=%s accepted=%s route=%s reason=%s",
                tostring(row.hex_id),
                tostring(row.canal_node_id),
                tostring(decision.accepted),
                decision.route,
                decision.reason
            ))
        end
    end)

    db:close()
    if not success then
        error(failure)
    end
end

if ... == nil then
    if #arg < 1 then
        io.stderr:write("Usage: luajit lua/cyboquatic/fog_router.lua <db_path>\n")
        os.exit(64)
    end
    M.run(arg[1])
end

return M
