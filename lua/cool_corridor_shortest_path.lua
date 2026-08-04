-- File: lua/cool_corridor_shortest_path.lua

-- Lua Dijkstra/A* shortest-path on cool_corridor_edge graph.
-- Edge weights combine distance and LST gradient; path metrics include
-- cumulative distance and cooling benefit.
--
-- Usage: lua cool_corridor_shortest_path.lua cool_corridor.db START_NODE_ID END_NODE_ID

local sqlite3 = require("lsqlite3")

local DB_PATH = arg[1] or "cool_corridor.db"
local START_NODE_ID = tonumber(arg[2])
local END_NODE_ID = tonumber(arg[3])

local function open_db(path)
  local db = sqlite3.open(path)
  if not db then
    error("Failed to open DB at " .. path)
  end
  return db
end

local function load_neighbors(db)
  local neighbors = {}
  for row in db:nrows([[
    SELECT edge_id, from_node_id, to_node_id, edge_weight, distance_km, cooling_benefit
    FROM cool_corridor_edge;
  ]]) do
    local from_id = row.from_node_id
    neighbors[from_id] = neighbors[from_id] or {}
    table.insert(neighbors[from_id], {
      edge_id = row.edge_id,
      to_node_id = row.to_node_id,
      weight = row.edge_weight,
      distance_km = row.distance_km,
      cooling_benefit = row.cooling_benefit or 0.0
    })
  end
  return neighbors
end

local function dijkstra(db, neighbors, start_id, end_id)
  local dist = {}
  local prev = {}
  local prev_edge = {}
  local visited = {}

  dist[start_id] = 0.0

  while true do
    -- Select unvisited node with minimal dist.
    local u = nil
    local best = nil
    for node_id, d in pairs(dist) do
      if not visited[node_id] then
        if best == nil or d < best then
          best = d
          u = node_id
        end
      end
    end
    if u == nil then break end
    if u == end_id then break end

    visited[u] = true

    local nbs = neighbors[u] or {}
    for _, e in ipairs(nbs) do
      local v = e.to_node_id
      local alt = dist[u] + e.weight
      if dist[v] == nil or alt < dist[v] then
        dist[v] = alt
        prev[v] = u
        prev_edge[v] = e
      end
    end
  end

  if dist[end_id] == nil then
    return nil, "No path found"
  end

  -- Reconstruct path.
  local path_nodes = {}
  local path_edges = {}
  local total_distance = 0.0
  local total_cooling = 0.0

  local current = end_id
  while current do
    table.insert(path_nodes, 1, current)
    local e = prev_edge[current]
    if e then
      table.insert(path_edges, 1, e.edge_id)
      total_distance = total_distance + e.distance_km
      total_cooling = total_cooling + (e.cooling_benefit or 0.0)
      current = prev[current]
    else
      break
    end
  end

  return {
    nodes = path_nodes,
    edges = path_edges,
    total_weight = dist[end_id],
    total_distance_km = total_distance,
    total_cooling_benefit = total_cooling
  }, nil
end

local function store_plan(db, path)
  local stmt_plan = db:prepare([[
    INSERT INTO cool_corridor_network(
      plan_name, start_node_id, end_node_id,
      total_edge_weight, total_distance_km, total_cooling_benefit,
      equity_score, budget_cost, created_at
    ) VALUES (?, ?, ?, ?, ?, ?, NULL, NULL, datetime('now'));
  ]])

  local plan_name = string.format("corridor_%d_%d", START_NODE_ID, END_NODE_ID)
  stmt_plan:bind_values(
    plan_name,
    START_NODE_ID,
    END_NODE_ID,
    path.total_weight,
    path.total_distance_km,
    path.total_cooling_benefit
  )
  stmt_plan:step()
  stmt_plan:finalize()

  local plan_id = db:last_insert_rowid()

  local stmt_seg = db:prepare([[
    INSERT INTO cool_corridor_path_segment(
      plan_id, seq_index, node_id, edge_id,
      cumulative_distance_km, cumulative_cooling_benefit
    ) VALUES (?, ?, ?, ?, ?, ?);
  ]])

  local cum_dist = 0.0
  local cum_cool = 0.0
  for i = 1, #path.nodes do
    local node_id = path.nodes[i]
    local edge_id = path.edges[i] or nil
    if edge_id then
      -- Fetch edge metrics for cumulative sums.
      for row in db:nrows(string.format(
        "SELECT distance_km, cooling_benefit FROM cool_corridor_edge WHERE edge_id = %d;", edge_id)) do
        cum_dist = cum_dist + row.distance_km
        cum_cool = cum_cool + (row.cooling_benefit or 0.0)
      end
    end
    stmt_seg:bind_values(plan_id, i, node_id, edge_id, cum_dist, cum_cool)
    stmt_seg:step()
    stmt_seg:reset()
  end

  stmt_seg:finalize()
  return plan_id
end

local function main()
  if not START_NODE_ID or not END_NODE_ID then
    print("Usage: lua cool_corridor_shortest_path.lua cool_corridor.db START_NODE_ID END_NODE_ID")
    return
  end

  local db = open_db(DB_PATH)
  local neighbors = load_neighbors(db)
  local path, err = dijkstra(db, neighbors, START_NODE_ID, END_NODE_ID)
  if not path then
    print("Path error: " .. err)
    db:close()
    return
  end

  local plan_id = store_plan(db, path)
  print(string.format(
    "Stored cool corridor plan_id=%d total_weight=%.3f total_distance_km=%.3f total_cooling_benefit=%.3f",
    plan_id, path.total_weight, path.total_distance_km, path.total_cooling_benefit
  ))

  db:close()
end

main()
