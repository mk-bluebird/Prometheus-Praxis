-- ALE-KO-MINTING-DAEMON-001.lua
-- Daily eco workload window KO minting loop (non-actuating).

local sqlite3 = require("lsqlite3")
local crypto  = require("aletheion.crypto")        -- PQ-safe hash/hex (no blacklisted stacks). [file:13]
local fs      = require("fs")

local db_path = "eco_restoration_shard/db/ecorestorationindex.sqlite"
local output_dir = "eco_restoration_shard/aln/workloads"

local function fetch_daily_nodes(db, day_ts)
  local stmt = db:prepare([[
    SELECT nodeid, region, Ei, Ci, Si, Ki, evidencehex
    FROM eco_restorationindex
    WHERE day_ts = ?
  ]])
  stmt:bind_values(day_ts)
  local rows = {}
  for row in stmt:nrows() do
    table.insert(rows, row)
  end
  stmt:finalize()
  return rows
end

local function build_aln_window(day_ts, rows)
  local lines = {}
  table.insert(lines, "-- eco.diagnostic.node.workload.window.v1")
  table.insert(lines, "type EcoDiagnosticWindow = {")
  table.insert(lines, "  day_ts: Integer,")
  table.insert(lines, "  nodes: List<NodeWorkload>")
  table.insert(lines, "}")
  table.insert(lines, "")
  table.insert(lines, "type NodeWorkload = {")
  table.insert(lines, "  nodeid: Text, region: Text, Ei: Real, Ci: Real, Si: Real, Ki: Real, evidencehex: Text")
  table.insert(lines, "}")
  table.insert(lines, "")
  table.insert(lines, "let window: EcoDiagnosticWindow = {")
  table.insert(lines, ("  day_ts: %d,"):format(day_ts))
  table.insert(lines, "  nodes: [")
  for i, row in ipairs(rows) do
    local line = ("    { nodeid: \"%s\", region: \"%s\", Ei: %.6f, Ci: %.6f, Si: %.6f, Ki: %.6f, evidencehex: \"%s\" }")
      :format(row.nodeid, row.region, row.Ei, row.Ci, row.Si, row.Ki, row.evidencehex)
    if i < #rows then
      line = line .. ","
    end
    table.insert(lines, line)
  end
  table.insert(lines, "  ]")
  table.insert(lines, "}")
  return table.concat(lines, "\n")
end

local function hex_stamp(aln_text)
  local hashhex = crypto.hash_hex(aln_text)    -- PQ-safe hash function, no blake/argon2/SHA3-256. [file:13]
  return hashhex
end

local function write_window(day_ts, aln_text, hashhex)
  local filename = ("%s/eco.diagnostic.node.workload.window.v1-%d.aln"):format(output_dir, day_ts)
  local f = assert(fs.open(filename, "w"))
  f:write(aln_text)
  f:write(("\n\n-- veritas.hex: %s\n"):format(hashhex))
  f:close()
end

local function main()
  local db = sqlite3.open(db_path)
  while true do
    local day_ts = os.time() // 86400 * 86400
    local rows = fetch_daily_nodes(db, day_ts)
    if #rows > 0 then
      local aln_text = build_aln_window(day_ts, rows)
      local hashhex  = hex_stamp(aln_text)
      write_window(day_ts, aln_text, hashhex)
    end
    os.execute("sleep 86400")   -- run once per day.
  end
end

main()
