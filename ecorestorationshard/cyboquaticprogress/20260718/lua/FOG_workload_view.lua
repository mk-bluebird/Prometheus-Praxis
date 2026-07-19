-- filename: ecorestorationshard/cyboquaticprogress/20260718/lua/FOG_workload_view.lua
-- purpose: Lua CLI view over dailyprogress, emitting JSON-friendly lines for non-actuating nodes.
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis

local sqlite3 = require("lsqlite3")

local DB_PATH = "ecorestorationshard/cyboquaticprogress/db_cyboquatic_daily_progress.sqlite"

local function open_db()
    local db = sqlite3.open(DB_PATH)
    return db
end

local function view_workloads(yyyymmdd, domainId)
    local db = open_db()
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
    view_workloads(yyyymmdd, domainId)
end

main()
