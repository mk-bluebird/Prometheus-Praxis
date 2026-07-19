-- filename: ecorestorationshard/cyboquaticprogress/20260718/lua/FOG_workload_view.lua
-- purpose: Lua CLI view over dailyprogress, emitting JSON-friendly lines for non-actuating nodes,
--          with explicit authorization hooks aligned to a ReBAC model.
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis

local sqlite3 = require("lsqlite3")

local DB_PATH = "ecorestorationshard/cyboquaticprogress/db_cyboquatic_daily_progress.sqlite"

local function check_can_view_workloads(auth_ctx)
    if auth_ctx == nil then
        return false
    end
    if type(auth_ctx.identity_id) ~= "string" then
        return false
    end
    if type(auth_ctx.org_id) ~= "string" then
        return false
    end
    return auth_ctx.allowed == true
end

local function open_db()
    local db = sqlite3.open(DB_PATH)
    return db
end

local function view_workloads(yyyymmdd, domainId, auth_ctx)
    if not check_can_view_workloads(auth_ctx) then
        io.stderr:write(
            "authorization_denied: identity=" ..
            tostring(auth_ctx and auth_ctx.identity_id or "nil") ..
            " org=" .. tostring(auth_ctx and auth_ctx.org_id or "nil") ..
            " relation=can_view_workloads\n"
        )
        return
    end

    local db = open_db()
    if not db then
        io.stderr:write("db_open_failed: path=" .. DB_PATH .. "\n")
        return
    end

    local stmt = db:prepare([[
        SELECT nodeid,
               energyreqJ,
               deltaVt,
               kscore,
               escore,
               rscore,
               evidencehex
        FROM dailyprogress
        WHERE yyyymmdd = ? AND domain = ?
        ORDER BY nodeid
    ]])

    if not stmt then
        io.stderr:write("stmt_prepare_failed\n")
        db:close()
        return
    end

    stmt:bind_values(yyyymmdd, domainId)

    for row in stmt:nrows() do
        local line = string.format(
            '{"nodeid":"%s","energyreqJ":%.6f,"deltaVt":%.6f,"K":%.3f,"E":%.3f,"R":%.3f,"evidencehex":"%s"}',
            row.nodeid,
            row.energyreqJ,
            row.deltaVt,
            row.kscore,
            row.escore,
            row.rscore,
            row.evidencehex
        )
        print(line)
    end

    stmt:finalize()
    db:close()
end

local function main()
    local yyyymmdd = arg[1] or "20260718"
    local domainId = arg[2] or "WORKLOADENERGYDV"

    local identity_id = os.getenv("CYBO_IDENTITY_ID") or "identity:local-cli"
    local org_id      = os.getenv("CYBO_ORG_ID") or "organization:phoenix-eco"
    local allowed_env = os.getenv("CYBO_CAN_VIEW_WORKLOADS") or "false"
    local allowed     = (allowed_env == "true")

    local auth_ctx = {
        identity_id = identity_id,
        org_id      = org_id,
        allowed     = allowed
    }

    view_workloads(yyyymmdd, domainId, auth_ctx)
end

main()
