-- filename: ecorestorationshard/cyboquatic_progress/20260720/lua/blastradius_cli_20260720.lua
-- destination: ecorestorationshard/cyboquatic_progress/20260720/lua/blastradius_cli_20260720.lua
-- domain: g (blast-radius surcharge envelopes).[file:2]

local sqlite3 = require("lsqlite3")  -- widely used Lua SQLite binding.[file:2]

local DB_PATH = "ecorestorationshard/db/dbcyboquaticdailyprogress.sqlite"

local function list_blastradius(day)
    local db = sqlite3.open(DB_PATH)
    local stmt = db:prepare([[
        SELECT canal_segment_id, surcharge_level_m, breach_prob, radius_m, impact_class
        FROM blastradius_surcharge
        WHERE yyyymmdd = ?
        ORDER BY canal_segment_id;
    ]])
    stmt:bind_values(day)

    for row in stmt:nrows() do
        print(string.format(
            "%s surcharge=%.3f breach=%.3f radius=%.2f impact=%s",
            row.canal_segment_id,
            row.surcharge_level_m,
            row.breach_prob,
            row.radius_m,
            row.impact_class
        ))
    end

    stmt:finalize()
    db:close()
end

local day = arg[1] or "20260720"
list_blastradius(day)
