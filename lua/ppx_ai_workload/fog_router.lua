-- File: lua/ppx_ai_workload/fog_router.lua
local sqlite3 = require("lsqlite3")

local fog_router = {}

local function finite_number(value)
    return type(value) == "number" and value == value
        and value ~= math.huge and value ~= -math.huge
end

local function unit_interval(value)
    return finite_number(value) and value >= 0.0 and value <= 1.0
end

local function nonempty_text(value)
    return type(value) == "string" and value:match("%S") ~= nil
        and value:find("[\r\n\t]") == nil
end

local function numeric(value)
    local converted = tonumber(value)
    return finite_number(converted) and converted or nil
end

local function parse_record(serialized)
    if type(serialized) == "table" then
        return serialized
    end
    if type(serialized) ~= "string" or serialized == "" then
        return nil, "record_not_serialized"
    end

    local record = {}
    for pair in serialized:gmatch("[^\t]+") do
        local key, value = pair:match("^([^=]+)=(.*)$")
        if not nonempty_text(key) then
            return nil, "invalid_key_value_record"
        end
        record[key] = numeric(value) or value
    end
    return record
end

local function normalize_record(record)
    return {
        hex_anchor = record.hex_anchor,
        source_id = record.source_id,
        action = record.action,
        lane = record.lane,
        fog_media_class = record.fog_media_class,
        canal_node_parameter = numeric(record.canal_node_parameter),
        canal_threshold = numeric(record.canal_threshold),
        k_knowledge = numeric(record.k_knowledge or record.K),
        e_eco_impact = numeric(record.e_eco_impact or record.E),
        r_risk = numeric(record.r_risk or record.R),
        roh = numeric(record.roh),
        delta_vt = numeric(record.delta_vt or record.deltaVt),
        reason_code = record.reason_code
    }
end

local function validate(record)
    if not nonempty_text(record.action) or not nonempty_text(record.lane)
        or not nonempty_text(record.fog_media_class) then
        return false, "missing_decision_fields"
    end
    if record.action ~= "PROCEED" and record.action ~= "DERATE"
        and record.action ~= "HALT" then
        return false, "invalid_action"
    end
    if record.lane ~= "RESEARCH" and record.lane ~= "PILOT"
        and record.lane ~= "PRODUCTION" then
        return false, "invalid_lane"
    end
    if record.fog_media_class ~= "AIR" and record.fog_media_class ~= "WATER"
        and record.fog_media_class ~= "SOIL" then
        return false, "invalid_fog_media_class"
    end
    for name, value in pairs({
        k_knowledge = record.k_knowledge,
        e_eco_impact = record.e_eco_impact,
        r_risk = record.r_risk,
        roh = record.roh
    }) do
        if not unit_interval(value) then
            return false, "invalid_" .. name
        end
    end
    if not finite_number(record.delta_vt)
        or not finite_number(record.canal_node_parameter)
        or not finite_number(record.canal_threshold)
        or record.canal_node_parameter < 0.0
        or record.canal_threshold <= 0.0 then
        return false, "invalid_residual_or_canal_parameter"
    end
    return true, "valid"
end

local function media_limit(media)
    if media == "WATER" then return 1.00 end
    if media == "SOIL" then return 1.25 end
    return 1.50
end

function fog_router.select(serialized)
    local parsed, parse_reason = parse_record(serialized)
    if not parsed then
        return "operator_review_queue", parse_reason
    end

    local record = normalize_record(parsed)
    local accepted, validation_reason = validate(record)
    if not accepted then
        return "operator_review_queue", validation_reason
    end

    local canal_ratio = record.canal_node_parameter / record.canal_threshold
    local residual_component = math.max(record.delta_vt, 0.0)
    local combined_risk = math.max(record.r_risk, record.roh, residual_component)

    if record.action == "HALT" then
        return "operator_review_queue", "halt_action"
    end
    if record.action == "DERATE" then
        return "reduced_resource_queue", "derate_action"
    end
    if record.e_eco_impact < 0.55 or combined_risk > 0.20 then
        return "reduced_resource_queue", "proceed_requires_resource_reduction"
    end
    if canal_ratio > media_limit(record.fog_media_class) then
        return "reduced_resource_queue", "fog_canal_corridor_exceeded"
    end
    return "low_impact_queue", "all_routing_corridors_passed"
end

local function file_exists(path)
    local handle = io.open(path, "rb")
    if handle then
        handle:close()
        return true
    end
    return false
end

function fog_router.wiring_status(repository_root)
    local root = repository_root or os.getenv("PPX_REPOSITORY_ROOT") or "."
    local required = {
        root .. "/sql/ppx_ai_workload/ker_fog_canal_shards.sql",
        root .. "/cpp/eco_restoration/ppx_ai_energy_risk.cpp",
        root .. "/crates/prometheus-praxis-ai/src/lanes/sdk.rs"
    }
    local missing = {}
    for _, path in ipairs(required) do
        if not file_exists(path) then
            table.insert(missing, path)
        end
    end
    return #missing == 0, missing
end

local function database_has_required_schema(db)
    local statement = db:prepare([[
        SELECT COUNT(*) AS count
        FROM sqlite_master
        WHERE type IN ('table', 'view')
          AND name IN ('ppx_ker_fog_canal_shard', 'v_ppx_hex_anchor_status');
    ]])
    if not statement then
        return false
    end
    local result = statement:nrows()()
    statement:finalize()
    return result ~= nil and result.count == 2
end

function fog_router.run(database_path)
    if not nonempty_text(database_path) then
        return nil, "database_path_required"
    end

    local db = sqlite3.open(database_path)
    if not db then
        return nil, "database_open_failed"
    end
    if not database_has_required_schema(db) then
        db:close()
        return nil, "ppx_ker_fog_canal_schema_missing"
    end

    local routed = {}
    local query = [[
        SELECT hex_anchor, source_id, lane, action, fog_media_class,
               canal_node_parameter, canal_threshold, k_knowledge,
               e_eco_impact, r_risk, roh, delta_vt, reason_code
        FROM ppx_ker_fog_canal_shard
        ORDER BY observed_utc ASC, shard_id ASC;
    ]]

    local success, failure = pcall(function()
        for row in db:nrows(query) do
            local queue, reason = fog_router.select(row)
            table.insert(routed, {
                hex_anchor = row.hex_anchor,
                source_id = row.source_id,
                queue = queue,
                reason = reason
            })
        end
    end)
    db:close()

    if not success then
        return nil, tostring(failure)
    end
    return routed, nil
end

if ... == nil then
    local database_path = arg[1]
    if not database_path then
        io.stderr:write("Usage: lua lua/ppx_ai_workload/fog_router.lua <database_path>\n")
        os.exit(64)
    end

    local routes, error_message = fog_router.run(database_path)
    if not routes then
        io.stderr:write(error_message .. "\n")
        os.exit(65)
    end

    for _, route in ipairs(routes) do
        print(string.format(
            "hex_anchor=%s\tsource_id=%s\tqueue=%s\treason=%s",
            tostring(route.hex_anchor),
            tostring(route.source_id),
            route.queue,
            route.reason
        ))
    end
end

return fog_router
