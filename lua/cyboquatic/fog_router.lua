-- File: lua/cyboquatic/fog_router.lua
local sqlite3 = require("lsqlite3")
local corridor = require("cyboquatic.generated.workload_corridor_validator")

local M = {}
local service_path = os.getenv("FOG_ROUTER_STDIO")
    or "./target/release/fog_router_stdio"

local function finite_number(value)
    return type(value) == "number"
        and value == value
        and value ~= math.huge
        and value ~= -math.huge
end

local function numeric(value)
    local converted = tonumber(value)
    if finite_number(converted) then
        return converted
    end
    return nil
end

local function nonempty_text(value)
    return type(value) == "string"
        and value:match("%S") ~= nil
        and not value:find("[\t\r\n]")
end

local function shell_quote(value)
    return "'" .. value:gsub("'", "'\\''") .. "'"
end

local function review(route, reason)
    return {
        accepted = false,
        route = route or "SAFE_TREATMENT_BASIN",
        reason = reason
    }
end

local function normalized_frame(frame, energy_requirement, residual_r)
    return {
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
end

local function request_core(required, predicted_radius_m)
    if not finite_number(predicted_radius_m) or predicted_radius_m < 0.0 then
        return nil, "predicted surcharge radius is unavailable"
    end

    local values = {
        required.canal_node,
        required.energyreqJ,
        required.deltaVt,
        required.K,
        required.E,
        required.R,
        required.fog_confidence,
        predicted_radius_m
    }

    if not nonempty_text(values[1]) then
        return nil, "invalid canal node identifier"
    end
    for index = 2, #values do
        if not finite_number(values[index]) then
            return nil, "invalid fog-router computation field"
        end
    end

    local request = table.concat({
        values[1],
        tostring(values[2]),
        tostring(values[3]),
        tostring(values[4]),
        tostring(values[5]),
        tostring(values[6]),
        tostring(values[7]),
        tostring(values[8])
    }, "\t") .. "\n"

    local command = "printf %s " .. shell_quote(request)
        .. " | " .. shell_quote(service_path)
    local handle = io.popen(command, "r")
    if not handle then
        return nil, "unable to start Rust fog-router service"
    end

    local response = handle:read("*l")
    local closed, _, status = handle:close()
    if not closed or status ~= 0 or not response then
        return nil, "Rust fog-router service failed"
    end

    local verdict, reason = response:match("^([A-Z]+)\t(.+)$")
    if verdict == "ACCEPT" then
        return true, reason
    end
    if verdict == "REVIEW" then
        return false, reason
    end
    return nil, "Rust fog-router returned an invalid response"
end

local function route_decision(frame)
    if type(frame) ~= "table" then
        return review(nil, "Telemetry frame must be a table.")
    end

    local fog_concentration = numeric(frame.fog_concentration_mgL)
    local unmodeled_media = numeric(frame.unmodeled_media_flag)
    local residual_r = numeric(frame.R or frame.residual_R or frame.ker_r)
    local energy_requirement = numeric(frame.energyreqJ or frame.energyreq_j)

    if not fog_concentration or not unmodeled_media or not residual_r or not energy_requirement then
        return review(nil, "Incomplete FOG, media, residual, or energy telemetry.")
    end
    if fog_concentration < 0.0 or unmodeled_media < 0.0 or residual_r < 0.0
        or energy_requirement < 0.0 then
        return review(nil, "Negative FOG, media, residual, or energy telemetry.")
    end
    if fog_concentration > 100.0 or unmodeled_media > 0.5 or residual_r > 0.7 then
        return review(nil, "FOG, unmodeled media, or residual risk exceeds its corridor.")
    end
    if energy_requirement > 50000.0 then
        return review("EFFICIENCY_RETUNING",
            "Energy requirement exceeds the machinery efficiency corridor.")
    end

    local required = normalized_frame(frame, energy_requirement, residual_r)
    local valid, reason = corridor.validate_workload_frame(required)
    if not valid then
        return review(nil, "ALN workload corridor rejected frame: " .. reason)
    end

    local predicted_radius = numeric(frame.predicted_surcharge_radius_m)
    local accepted, core_reason = request_core(required, predicted_radius)
    if accepted == nil then
        return review(nil, core_reason)
    end
    if not accepted then
        return review(nil, core_reason)
    end

    return {
        accepted = true,
        route = "STANDARD_CANAL_FLOW",
        reason = core_reason
    }
end

function M.route(frame)
    return route_decision(frame)
end

function M.evaluate_route(fog_concentration_mgL, unmodeled_media_flag, residual_r,
        energyreq_j, frame)
    frame = frame or {}
    frame.fog_concentration_mgL = fog_concentration_mgL
    frame.unmodeled_media_flag = unmodeled_media_flag
    frame.R = residual_r
    frame.energyreqJ = energyreq_j
    return route_decision(frame)
end

function M.run(db_path)
    assert(nonempty_text(db_path), "database path is required")

    local db = sqlite3.open(db_path)
    if not db then
        error("unable to open SQLite database: " .. db_path)
    end

    local sql = [[
        SELECT f.hex_id,
               f.canal_node_id,
               f.fog_concentration_mgL,
               f.unmodeled_media_flag,
               f.predicted_surcharge_radius_m,
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
                predicted_surcharge_radius_m = row.predicted_surcharge_radius_m,
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
