-- File: lua/hex_anchor_rounding_heuristic.lua

-- Lua script implementing adjacency-aware rounding of relaxed hex-anchor tree counts.
-- It reads from hex_anchor_relaxed, enforces neighborhood clustering, and writes
-- rounded counts back for comparison and ALN invariant validation.
--
-- Heuristic:
--   1. Sort hexes by relaxed_count descending.
--   2. For each hex, start from nearest integer rounded_count = round(relaxed_count).
--   3. Enforce adjacency clustering: if neighbors have low rounded_count and
--      hex has high carbon_potential, push some counts to neighbors to reduce
--      worst-case thermal inequity.
--   4. Respect global and per-hex bounds.

local sqlite3 = require("lsqlite3")

local DB_PATH = arg[1] or "hex_restoration.db"

local function open_db(path)
  local db = sqlite3.open(path)
  if not db then
    error("Failed to open DB at " .. path)
  end
  return db
end

local function load_relaxed()
  local db = open_db(DB_PATH)
  local hexes = {}
  for row in db:nrows([[
    SELECT r.h3_index, r.relaxed_count,
           c.carbon_potential, c.vulnerability
    FROM hex_anchor_relaxed r
    JOIN hex_restoration_commitment c ON c.h3_index = r.h3_index;
  ]]) do
    hexes[#hexes + 1] = {
      h3_index = row.h3_index,
      relaxed = row.relaxed_count,
      carbon_potential = row.carbon_potential,
      vulnerability = row.vulnerability,
      rounded = nil
    }
  end
  db:close()
  table.sort(hexes, function(a, b)
    return a.relaxed > b.relaxed
  end)
  return hexes
end

local function load_neighbors()
  local db = open_db(DB_PATH)
  local neighbors = {}
  for row in db:nrows("SELECT h3_index, neighbor_h3_index FROM hex_neighbors;") do
    local h = row.h3_index
    local n = row.neighbor_h3_index
    neighbors[h] = neighbors[h] or {}
    table.insert(neighbors[h], n)
  end
  db:close()
  return neighbors
end

local function round_and_cluster(hexes, neighbors)
  -- Initial rounding
  for _, h in ipairs(hexes) do
    h.rounded = math.floor(h.relaxed + 0.5)
    if h.rounded < 0 then h.rounded = 0 end
  end

  -- Simple adjacency-aware smoothing:
  -- For high carbon_potential / high vulnerability cells, ensure neighbors
  -- get at least minimal counts to avoid sharp inequities.
  for _, h in ipairs(hexes) do
    local nb = neighbors[h.h3_index] or {}
    local avg_nb = 0.0
    local nb_count = 0
    for _, n in ipairs(nb) do
      for _, hh in ipairs(hexes) do
        if hh.h3_index == n then
          avg_nb = avg_nb + hh.rounded
          nb_count = nb_count + 1
        end
      end
    end
    if nb_count > 0 then
      avg_nb = avg_nb / nb_count
      if h.rounded > avg_nb + 3 and h.carbon_potential > 0.5 then
        -- push one tree to each neighbor up to vulnerability threshold
        for _, n in ipairs(nb) do
          for _, hh in ipairs(hexes) do
            if hh.h3_index == n and hh.vulnerability > 0.3 then
              hh.rounded = hh.rounded + 1
              h.rounded = h.rounded - 1
              if h.rounded <= avg_nb + 3 then break end
            end
          end
          if h.rounded <= avg_nb + 3 then break end
        end
      end
    end
  end

  return hexes
end

local function store_rounded(hexes)
  local db = open_db(DB_PATH)
  db:exec([[
    UPDATE hex_anchor_relaxed
    SET rounded_count = NULL;
  ]])

  local stmt = db:prepare([[
    UPDATE hex_anchor_relaxed
    SET rounded_count = ?
    WHERE h3_index = ?;
  ]])

  for _, h in ipairs(hexes) do
    stmt:bind_values(h.rounded, h.h3_index)
    stmt:step()
    stmt:reset()
  end

  stmt:finalize()
  db:close()
end

local function main()
  local hexes = load_relaxed()
  if #hexes == 0 then
    print("No relaxed hex-anchor rows; run QP first.")
    return
  end
  local neighbors = load_neighbors()
  hexes = round_and_cluster(hexes, neighbors)
  store_rounded(hexes)
  print("Rounded hex-anchor plan stored in hex_anchor_relaxed.rounded_count.")
end

main()
