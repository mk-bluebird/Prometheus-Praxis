-- File: lua/ppx_ai_workload/constellation_workload_audit.lua
local sqlite3 = require("lsqlite3")

local DEFAULT_DATABASE_PATH = "data/constellation/econet_constellation_index.db"

local POLICY = {
    K_min = 0.90,
    E_min = 0.90,
    R_max = 0.13,
    roh_max = 0.13,
    max_delta_vt = 0.02,
    maximum_critical_dependencies = 3,
    risk_trend_window = 5,
    risk_trend_trigger = 0.12,
    minimum_eco_impact_for_canal = 0.55,
}

local function fail(message)
    io.stderr:write("PPX workload audit: " .. message .. "\n")
    os.exit(1)
end

local function open_database(path)
    local database = sqlite3.open(path)
    if not database then
        fail("cannot open SQLite database at " .. path)
    end
    return database
end

local function prepare(database, sql)
    local statement = database:prepare(sql)
    if not statement then
        fail("cannot prepare required query: " .. database:errmsg())
    end
    return statement
end

local function collect_rows(database, sql, values)
    local statement = prepare(database, sql)
    if values then
        statement:bind_values(table.unpack(values))
    end

    local rows = {}
    for row in statement:nrows() do
        rows[#rows + 1] = row
    end
    statement:finalize()
    return rows
end

local function scalar(database, sql, values)
    local rows = collect_rows(database, sql, values)
    if #rows == 0 then
        return 0
    end
    for _, value in pairs(rows[1]) do
        if type(value) == "number" then
            return value
        end
    end
    return 0
end

local function check_artifact_ker(database)
    return collect_rows(database, [[
        SELECT
            a.artifact_path AS artifact_path,
            k.knowledge_factor AS k_knowledge,
            k.eco_impact AS e_eco_impact,
            k.risk_of_harm AS r_risk
        FROM artifacts AS a
        JOIN ker_scores AS k ON k.artifact_id = a.artifact_id
        WHERE k.knowledge_factor < ?
           OR k.eco_impact < ?
           OR k.risk_of_harm > ?
    ]], { POLICY.K_min, POLICY.E_min, POLICY.R_max })
end

local function check_ppx_workload_corridor(database)
    return collect_rows(database, [[
        SELECT
            workload_id,
            lane,
            action,
            k_knowledge,
            e_eco_impact,
            r_risk,
            roh,
            delta_vt,
            canal_node_parameter,
            canal_threshold,
            reason_code
        FROM ppx_ker_fog_canal_shard
        WHERE roh > ?
           OR r_risk > ?
           OR delta_vt > ?
           OR (
               lane <> 'RESEARCH'
               AND canal_node_parameter > canal_threshold
               AND e_eco_impact < ?
           )
           OR (
               lane = 'RESEARCH'
               AND canal_node_parameter > canal_threshold
               AND e_eco_impact < ?
               AND action = 'PROCEED'
           )
    ]], {
        POLICY.roh_max,
        POLICY.R_max,
        POLICY.max_delta_vt,
        POLICY.minimum_eco_impact_for_canal,
        POLICY.minimum_eco_impact_for_canal,
    })
end

local function critical_dependency_count(database, artifact_id)
    return scalar(database, [[
        SELECT COUNT(*)
        FROM blast_radius
        WHERE source_artifact_id = ?
          AND impact_severity = 'critical'
    ]], { artifact_id })
end

local function risk_trend_requires_attention(database)
    local rows = collect_rows(database, [[
        SELECT r_risk
        FROM ppx_ker_fog_canal_shard
        ORDER BY observed_utc DESC
        LIMIT ?
    ]], { POLICY.risk_trend_window })

    if #rows < POLICY.risk_trend_window then
        return false, 0.0
    end

    local oldest = tonumber(rows[#rows].r_risk)
    local newest = tonumber(rows[1].r_risk)
    if not oldest or not newest then
        return true, 1.0
    end
    return newest - oldest > POLICY.risk_trend_trigger, newest - oldest
end

local function print_violations(title, rows)
    if #rows == 0 then
        print("  PASS " .. title)
        return true
    end

    print("  FAIL " .. title .. ": " .. #rows .. " violating record(s)")
    for _, row in ipairs(rows) do
        print(string.format(
            "    id=%s lane=%s action=%s K=%s E=%s R=%s RoH=%s deltaV=%s reason=%s",
            tostring(row.workload_id or row.artifact_path),
            tostring(row.lane or "N/A"),
            tostring(row.action or "N/A"),
            tostring(row.k_knowledge or "N/A"),
            tostring(row.e_eco_impact or "N/A"),
            tostring(row.r_risk or "N/A"),
            tostring(row.roh or "N/A"),
            tostring(row.delta_vt or "N/A"),
            tostring(row.reason_code or "artifact_ker_violation")
        ))
    end
    return false
end

local function main()
    local database_path = arg[1] or DEFAULT_DATABASE_PATH
    local artifact_id = tonumber(arg[2]) or 5
    local database = open_database(database_path)

    print("=== PPX Eco-Restoration Workload Audit ===")
    print("[1/4] Artifact KER policy")
    local artifacts_ok = print_violations("artifact KER", check_artifact_ker(database))

    print("[2/4] Workload corridor policy")
    local workloads_ok = print_violations(
        "K/E/R, RoH, residual, and canal corridor",
        check_ppx_workload_corridor(database)
    )

    print("[3/4] Dependency exposure")
    local critical_count = critical_dependency_count(database, artifact_id)
    local dependencies_ok = critical_count <= POLICY.maximum_critical_dependencies
    print(string.format(
        "  %s artifact=%d critical_dependencies=%d maximum=%d",
        dependencies_ok and "PASS" or "FAIL",
        artifact_id,
        critical_count,
        POLICY.maximum_critical_dependencies
    ))

    print("[4/4] Risk trend advisory")
    local attention_required, trend = risk_trend_requires_attention(database)
    print(string.format(
        "  %s five_window_risk_change=%.6f threshold=%.6f",
        attention_required and "ADVISORY_REVIEW" or "PASS",
        trend,
        POLICY.risk_trend_trigger
    ))

    database:close()
    if not artifacts_ok or not workloads_ok or not dependencies_ok then
        fail("audit failed; no workload admission recommendation emitted")
    end
    print("PASS audit complete; workload decisions remain non-actuating")
end

main()
