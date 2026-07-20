-- filename: ecorestorationshard/cyboquaticprogress/20260719/lua/workload_cli_20260719.lua
-- destination: ecorestorationshard/cyboquaticprogress/20260719/lua/workload_cli_20260719.lua
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
-- Purpose: Lightweight Lua CLI to inspect ΔVt and KER flags for the 20260719
-- workload shard. Uses lsqlite3, which is widely available and verifiable. [web:23][file:2]

local sqlite3 = require("lsqlite3")

local db_path = "ecorestorationshard/db/dbcyboquaticdailyprogress.sqlite"

local db = sqlite3.open(db_path)

local sql = [[
SELECT node_id, vt_prev, vt_curr, delta_vt,
       k_factor, e_factor, r_factor,
       evidence_hex, hex_logical_name
FROM dailyprogress_workload_20260719
WHERE workday_yyyymmdd = '20260719';
]]

print("Cyboquatic workload diagnostics for 2026-07-19 (non-actuating):")

for row in db:nrows(sql) do
    print(string.format(
        "node=%s ΔVt=%.4f (prev=%.4f, curr=%.4f) K=%.3f E=%.3f R=%.3f hex=%s (%s)",
        row.node_id,
        row.delta_vt,
        row.vt_prev,
        row.vt_curr,
        row.k_factor,
        row.e_factor,
        row.r_factor,
        row.evidence_hex,
        row.hex_logical_name
    ))
end

db:close()
