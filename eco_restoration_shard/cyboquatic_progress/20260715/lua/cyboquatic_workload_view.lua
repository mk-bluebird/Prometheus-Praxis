-- filename: eco_restoration_shard/cyboquatic_progress/20260715/lua/cyboquatic_workload_view.lua
-- purpose: Lightweight Lua helper to fetch daily cyboquatic workload summaries
--          via SQLite CLI, for AI-chat platforms needing compact, text-friendly views.

local M = {}

-- Executes a shell command and returns its output as a string.
local function run(cmd)
    local handle = io.popen(cmd)
    if not handle then
        return nil, "failed to open process"
    end
    local result = handle:read("*a")
    handle:close()
    return result, nil
end

-- Returns a plain-text summary for a given date.
function M.day_summary(db_path, yyyymmdd)
    local sql = [[
SELECT segment_id,
       printf('%.2f', AVG(energyreq_j)) AS avg_energyreq_j,
       printf('%.4f', AVG(deltavt))     AS avg_deltavt,
       printf('%.3f', AVG(k_factor))    AS k_avg,
       printf('%.3f', AVG(e_factor))    AS e_avg,
       printf('%.3f', AVG(r_factor))    AS r_avg
FROM daily_progress
WHERE yyyymmdd = ']] .. yyyymmdd .. [['
  AND domain = 'cyboquatic_workload'
GROUP BY segment_id;
]]
    local cmd = string.format("sqlite3 %q %q", db_path, sql)
    local out, err = run(cmd)
    if not out then
        return nil, err
    end
    return out, nil
end

return M
