-- aln_particle_registry_diff.lua
--
-- Compare canonical aln_particle_registry.aln with a live SQLite mirror.
-- Inputs:
--   argv[1] = path to aln_particle_registry.aln
--   argv[2] = path to SQLite DB (mirror)
--
-- Output:
--   JSON report on stdout:
--     {
--       "diverges": true|false,
--       "missing_in_sqlite": [...],
--       "extra_in_sqlite": [...],
--       "mismatched_paths": [...]
--     }
--
-- CI can use this JSON to decide whether to open a GitHub issue.

local json = require("dkjson")
local sqlite3 = require("lsqlite3")

local function read_registry_aln(path)
  local f = assert(io.open(path, "r"))
  local content = f:read("*a")
  f:close()

  local in_block = false
  local registry = {}

  for line in content:gmatch("[^\r\n]+") do
    local trimmed = line:match("^%s*(.-)%s*$")
    if trimmed:match("^block%s+registry_rows") then
      in_block = true
    elseif in_block and trimmed:match("^content") then
      -- start of content; continue
    elseif in_block and trimmed:match("^endcontent") then
      in_block = false
    elseif in_block then
      if trimmed:sub(1, 1) == ";" then
        -- comment line, skip
      else
        -- Expect TSV row: particle_name\taln_file_path\t...
        local cols = {}
        for field in trimmed:gmatch("[^\t]+") do
          table.insert(cols, field)
        end
        if #cols >= 2 then
          local particle_name = cols[1]
          local aln_file_path = cols[2]
          registry[particle_name] = {
            particle_name = particle_name,
            aln_file_path = aln_file_path
          }
        end
      end
    end
  end

  return registry
end

local function read_registry_sqlite(path)
  local db = sqlite3.open(path)
  local registry = {}

  local stmt = db:prepare("SELECT particle_name, aln_file_path FROM aln_particle_registry")
  if stmt == nil then
    db:close()
    error("Failed to prepare statement; ensure table aln_particle_registry exists.")
  end

  for row in stmt:nrows() do
    registry[row.particle_name] = {
      particle_name = row.particle_name,
      aln_file_path = row.aln_file_path
    }
  end

  stmt:finalize()
  db:close()

  return registry
end

local function diff_registry(aln_reg, sql_reg)
  local missing_in_sqlite = {}
  local extra_in_sqlite = {}
  local mismatched_paths = {}

  -- ALN -> SQLite
  for name, aln_row in pairs(aln_reg) do
    local sql_row = sql_reg[name]
    if not sql_row then
      table.insert(missing_in_sqlite, aln_row)
    else
      if aln_row.aln_file_path ~= sql_row.aln_file_path then
        table.insert(mismatched_paths, {
          particle_name = name,
          aln_file_path_aln = aln_row.aln_file_path,
          aln_file_path_sqlite = sql_row.aln_file_path
        })
      end
    end
  end

  -- SQLite -> ALN
  for name, sql_row in pairs(sql_reg) do
    if not aln_reg[name] then
      table.insert(extra_in_sqlite, sql_row)
    end
  end

  local diverges = (#missing_in_sqlite > 0) or (#extra_in_sqlite > 0) or (#mismatched_paths > 0)

  return {
    diverges = diverges,
    missing_in_sqlite = missing_in_sqlite,
    extra_in_sqlite = extra_in_sqlite,
    mismatched_paths = mismatched_paths
  }
end

local function main()
  local args = {...}
  if #args < 2 then
    io.stderr:write("Usage: lua aln_particle_registry_diff.lua registry.aln registry.sqlite\n")
    os.exit(1)
  end

  local aln_path = args[1]
  local sqlite_path = args[2]

  local aln_reg = read_registry_aln(aln_path)
  local sql_reg = read_registry_sqlite(sqlite_path)

  local report = diff_registry(aln_reg, sql_reg)
  local encoded, _, err = json.encode(report, { indent = true })
  if err then
    error("JSON encode error: " .. err)
  end

  io.stdout:write(encoded)
  io.stdout:write("\n")

  if report.diverges then
    os.exit(2)
  else
    os.exit(0)
  end
end

main()
