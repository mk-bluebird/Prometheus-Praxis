-- eco_repo_index_lane_promotion.lua
-- Lane-promotion evaluator based on EcoRepoIndex2026v1 predicates.
--
-- Inputs:
--   argv[1] = path to JSON file with KER snapshots (array)
-- Outputs:
--   stdout = SQLite migration script updating lane assignments

local json = require("dkjson")

local function read_file(path)
  local f = assert(io.open(path, "r"))
  local content = f:read("*a")
  f:close()
  return content
end

local function parse_snapshots(path)
  local content = read_file(path)
  local data, pos, err = json.decode(content, 1, nil)
  if err then
    error("JSON decode error: " .. err)
  end
  return data
end

-- Example promotion predicate:
--   - average K over window >= k_threshold
--   - average R over window <= r_threshold
--   - current lane == RESEARCH -> promote to PILOT
--   - current lane == PILOT    -> promote to PROD
local K_THRESHOLD = 0.75
local R_THRESHOLD = 0.30

local function compute_window_stats(snapshots)
  local sum_k = 0.0
  local sum_r = 0.0
  local count = 0

  for _, s in ipairs(snapshots) do
    if s.k and s.r then
      sum_k = sum_k + s.k
      sum_r = sum_r + s.r
      count = count + 1
    end
  end

  if count == 0 then
    return 0.0, 1.0
  end

  return sum_k / count, sum_r / count
end

local function determine_promotion_lane(current_lane)
  if current_lane == "RESEARCH" then
    return "PILOT"
  elseif current_lane == "PILOT" then
    return "PROD"
  else
    return nil
  end
end

local function build_migration(snapshots)
  local avg_k, avg_r = compute_window_stats(snapshots)
  if avg_k < K_THRESHOLD or avg_r > R_THRESHOLD then
    return "-- No lane promotions: predicates not satisfied\n"
  end

  local promote_ids = {}

  for _, s in ipairs(snapshots) do
    local target = determine_promotion_lane(s.lane)
    if target ~= nil then
      table.insert(promote_ids, { id = s.particle_id, lane = target })
    end
  end

  if #promote_ids == 0 then
    return "-- No lane promotions: no eligible particles\n"
  end

  local lines = {}
  table.insert(lines, "BEGIN TRANSACTION;")
  for _, p in ipairs(promote_ids) do
    local stmt = string.format(
      "UPDATE ker_particles SET lane = '%s' WHERE particle_id = '%s';",
      p.lane,
      p.id
    )
    table.insert(lines, stmt)
  end
  table.insert(lines, "COMMIT;")

  return table.concat(lines, "\n") .. "\n"
end

local function main()
  local args = {...}
  if #args < 1 then
    io.stderr:write("Usage: lua eco_repo_index_lane_promotion.lua snapshots.json\n")
    os.exit(1)
  end

  local snapshots = parse_snapshots(args[1])
  local migration = build_migration(snapshots)
  io.stdout:write(migration)
end

main()
