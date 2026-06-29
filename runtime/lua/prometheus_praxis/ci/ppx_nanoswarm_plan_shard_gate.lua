-- Filename: runtime/lua/prometheus_praxis/ci/ppx_nanoswarm_plan_shard_gate.lua
-- Domain: Prometheus-Praxis
-- Purpose: CI preflight for nanoswarm urban plans: "no shard row, no mission".

local sqlite3 = require("lsqlite3")

local M = {}

local function open_db(db_path)
  local db = sqlite3.open(db_path)
  if not db then
    return nil, "failed to open db at " .. db_path
  end
  return db, nil
end

local function fetch_shard_rows(db, plan_ko_id, shard_table)
  local rows = {}
  local sql = string.format([[
    SELECT
      id,
      plan_ko_id,
      ker_id,
      vbarrier_prev,
      vbarrier_next,
      roh_global_max,
      lipschitz_spatial,
      lipschitz_temporal,
      aliasing_margin_concentration,
      fpic_decision
    FROM %s
    WHERE plan_ko_id = ?
  ]], shard_table)

  local stmt = db:prepare(sql)
  if not stmt then
    return nil, "failed to prepare shard query"
  end

  stmt:bind_values(plan_ko_id)
  for row in stmt:nrows() do
    table.insert(rows, row)
  end
  stmt:finalize()
  return rows, nil
end

local function shard_row_has_required_fields(row)
  if row.ker_id == nil or row.ker_id == "" then
    return false, "missing ker_id"
  end
  if row.vbarrier_prev == nil or row.vbarrier_next == nil then
    return false, "missing Lyapunov barrier fields"
  end
  if row.roh_global_max == nil then
    return false, "missing RoH global max"
  end
  if row.lipschitz_spatial == nil or row.lipschitz_temporal == nil then
    return false, "missing Lipschitz diagnostics"
  end
  if row.aliasing_margin_concentration == nil then
    return false, "missing aliasing margin"
  end
  if row.fpic_decision == nil or row.fpic_decision == "" then
    return false, "missing FPIC decision"
  end
  return true, nil
end

-- CI entrypoint: input is a Lua table with fields matching ppx.nanoswarm.plan.shard.ci.input.v1.
function M.check_nanoswarm_plan_shard(input)
  local plan_ko_id = input.plan_ko_id
  local db_path = input.db_path
  local shard_table = input.shard_table_name or "nanoswarm_urban_shards"

  local result = {
    ok = false,
    violations = {}
  }

  if not plan_ko_id or plan_ko_id == "" then
    table.insert(result.violations, "PLAN_KO_ID_MISSING")
    return result
  end
  if not db_path or db_path == "" then
    table.insert(result.violations, "DB_PATH_MISSING")
    return result
  end

  local db, err = open_db(db_path)
  if not db then
    table.insert(result.violations, "DB_OPEN_FAILED: " .. err)
    return result
  end

  local rows, qerr = fetch_shard_rows(db, plan_ko_id, shard_table)
  db:close()

  if not rows then
    table.insert(result.violations, "SHARD_QUERY_FAILED: " .. qerr)
    return result
  end

  if #rows == 0 then
    table.insert(result.violations, "NO_SHARD_ROWS_FOR_PLAN_KO: " .. plan_ko_id)
    return result
  end

  for _, row in ipairs(rows) do
    local ok, why = shard_row_has_required_fields(row)
    if not ok then
      table.insert(result.violations,
        string.format("INCOMPLETE_SHARD_ROW id=%s plan_ko_id=%s reason=%s",
          tostring(row.id), tostring(row.plan_ko_id), why))
    end
  end

  if #result.violations == 0 then
    result.ok = true
  end

  return result
end

return M
