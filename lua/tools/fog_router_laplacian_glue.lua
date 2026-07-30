-- File: lua/tools/fog_router_laplacian_glue.lua
-- Purpose:
--   Lua glue script that:
--     - Invokes the C++ Laplacian/eigenvalue tool.
--     - Parses its output.
--     - Writes results into fog_router_laplacian_spectrum for ALN obligations.

local function run_command(cmd)
  local handle = io.popen(cmd)
  if not handle then
    error("failed to popen command")
  end
  local output = handle:read("*a")
  handle:close()
  return output
end

local function parse_output(output)
  local lambda_min = output:match("lambda_min.-:%s*([%d%.%-]+)")
  local lambda_max = output:match("lambda_max.-:%s*([%d%.%-]+)")
  local chern      = output:match("Chern%-like index.-:%s*(%d+)")

  return tonumber(lambda_min), tonumber(lambda_max), tonumber(chern)
end

local function write_spectrum_to_sqlite(graph_id, lambda_min, lambda_max, chern_index)
  local sqlite3 = require("lsqlite3")
  local db = sqlite3.open("dbcyboquaticdailyprogress.sqlite")

  local stmt = db:prepare([[
    INSERT INTO fog_router_laplacian_spectrum
      (graph_id, timestep, lambda_min, lambda_max, chern_index, evidence_hex, created_utc)
    VALUES (?, ?, ?, ?, ?, '0x20260729PHXFOGRouterTopology2026v1', datetime('now'));
  ]])

  stmt:bind_values(graph_id, 0, lambda_min, lambda_max, chern_index)
  stmt:step()
  stmt:finalize()
  db:close()
end

local function main()
  local output = run_command("./fog_router_laplacian")
  local lambda_min, lambda_max, chern_index = parse_output(output)
  write_spectrum_to_sqlite("PHX-CANAL-FOG-GRAPH-001", lambda_min, lambda_max, chern_index)
end

main()
