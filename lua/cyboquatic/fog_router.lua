-- File: lua/cyboquatic/fog_router.lua
local sqlite3 = require("lsqlite3")

local function evaluate_route(fog_conc_mgL, unmodeled_media_flag, residual_R, energyreqJ)
    local high_fog = fog_conc_mgL > 100.0
    local high_unmodeled = unmodeled_media_flag > 0.5
    local high_risk = residual_R > 0.7
    local high_energy = energyreqJ > 50000.0

    if high_fog or high_unmodeled or high_risk then
        return "SAFE_TREATMENT_BASIN", "High FOG/unmodeled media or residual risk."
    elseif high_energy then
        return "EFFICIENCY_RETUNING", "High energy requirement; retune machinery."
    else
        return "STANDARD_CANAL_FLOW", "Parameters within safe corridors."
    end
end

local function main(db_path)
    local db = sqlite3.open(db_path)
    local sql = [[
        SELECT f.hex_id, f.canal_node_id,
               f.fog_concentration_mgL,
               f.unmodeled_media_flag,
               w.R, w.energyreqJ
        FROM fog_flow f
        LEFT JOIN workload_cycle w
          ON f.hex_id = w.hex_id
         AND f.canal_node_id = w.canal_node_id;
    ]]
    for row in db:nrows(sql) do
        local route, reason = evaluate_route(
            row.fog_concentration_mgL,
            row.unmodeled_media_flag,
            row.R or 0.0,
            row.energyreqJ or 0.0
        )
        print(string.format(
            "hex=%s node=%s route=%s reason=%s",
            row.hex_id, row.canal_node_id, route, reason
        ))
    end
    db:close()
end

if #arg < 1 then
    print("Usage: lua fog_router.lua <db_path>")
else
    main(arg[1])
end
