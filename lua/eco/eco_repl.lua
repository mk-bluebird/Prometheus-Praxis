-- File: lua/eco/eco_repl.lua
-- Simple eco-restoration REPL helper.
-- Provides commands:
--   ker <db> <hex_id>
--   pfas <db> <node_code>
--   fog  <csv> <node_code>
--
-- Uses only Lua standard libraries and shell calls to sqlite3,
-- allowing interactive exploration of KER and PFAS corridors.

local function trim(s)
    return (s:gsub("^%s+", ""):gsub("%s+$", ""))
end

local function exec_sqlite(db, sql)
    local cmd = string.format("sqlite3 '%s' \"%s\"", db, sql)
    local fh = io.popen(cmd, "r")
    if not fh then
        return nil
    end
    local out = fh:read("*a")
    fh:close()
    return out
end

local function ker_query(db, hex_id)
    local sql = string.format(
        "SELECT r_hydraulics,r_energy,r_topology,r_biodiversity,w_h,w_e,w_t,w_b " ..
        "FROM phoenix_hex_registry WHERE hex_id='%s';",
        hex_id
    )
    local out = exec_sqlite(db, sql)
    if not out or #out == 0 then
        print("No hex entry for " .. hex_id)
        return
    end
    local fields = {}
    for f in string.gmatch(out, "([^|]+)") do
        fields[#fields + 1] = f
    end
    if #fields < 8 then
        print("Unexpected hex row format.")
        return
    end
    local r_h = tonumber(fields[1]) or 0.0
    local r_e = tonumber(fields[2]) or 0.0
    local r_t = tonumber(fields[3]) or 0.0
    local r_b = tonumber(fields[4]) or 0.0
    local w_h = tonumber(fields[5]) or 0.0
    local w_e = tonumber(fields[6]) or 0.0
    local w_t = tonumber(fields[7]) or 0.0
    local w_b = tonumber(fields[8]) or 0.0

    local Vt = w_h * r_h * r_h +
               w_e * r_e * r_e +
               w_t * r_t * r_t +
               w_b * r_b * r_b

    local r_max = math.max(math.max(r_h, r_e), math.max(r_t, r_b))
    local k = 0.9
    local e = math.max(0.0, 1.0 - r_max)
    local s = k * e - r_max

    print("KER for hex " .. hex_id)
    print(string.format("  r_hydraulics  = %.3f", r_h))
    print(string.format("  r_energy      = %.3f", r_e))
    print(string.format("  r_topology    = %.3f", r_t))
    print(string.format("  r_biodiversity= %.3f", r_b))
    print(string.format("  Vt            = %.4f", Vt))
    print(string.format("  r_max         = %.3f", r_max))
    print(string.format("  k,e,ker_score = k=%.3f e=%.3f ker=%.4f", k, e, s))
end

local function pfas_query(db, node_code)
    local sql = string.format(
        "SELECT mass_kg,sorbed_fraction,cold_survival_factor " ..
        "FROM pfas_corridor_state ps " ..
        "JOIN canal_node cn ON ps.node_id = cn.node_id " ..
        "WHERE cn.node_code='%s';",
        node_code
    )
    local out = exec_sqlite(db, sql)
    if not out or #out == 0 then
        print("No PFAS state for node " .. node_code)
        return
    end
    local fields = {}
    for f in string.gmatch(out, "([^|]+)") do
        fields[#fields + 1] = f
    end
    if #fields < 3 then
        print("Unexpected PFAS row format.")
        return
    end
    local mass_kg = tonumber(fields[1]) or 0.0
    local sorbed_fraction = tonumber(fields[2]) or 0.0
    local cold_survival_factor = tonumber(fields[3]) or 0.0

    print("PFAS corridor for node " .. node_code)
    print(string.format("  mass_kg             = %.6f", mass_kg))
    print(string.format("  sorbed_fraction     = %.3f", sorbed_fraction))
    print(string.format("  cold_survival_factor= %.3f", cold_survival_factor))
end

local function fog_query(csv, node_code)
    local fh = io.open(csv, "r")
    if not fh then
        print("Cannot open CSV: " .. csv)
        return
    end
    local FogRouter = require("cyboquatic.fog_router_predicates")

    for line in fh:lines() do
        if #line > 0 then
            local cols = {}
            for f in string.gmatch(line, "([^,]+)") do
                cols[#cols + 1] = f
            end
            if #cols >= 5 then
                local nc = cols[1]
                if nc == node_code then
                    local sample = {
                        mediumType = "water",
                        temperatureC = tonumber(cols[2]) or 0.0,
                        pfasConcentrationUgL = tonumber(cols[3]) or 0.0,
                        dissolvedO2MgL = tonumber(cols[4]) or 0.0,
                        turbidityNTU = tonumber(cols[5]) or 0.0
                    }
                    local route = FogRouter.route(sample)
                    print("FOG route for " .. node_code .. ": " .. route)
                    fh:close()
                    return
                end
            end
        end
    end

    fh:close()
    print("No telemetry row for node " .. node_code)
end

local function repl()
    print("Eco REPL (ker / pfas / fog). Type 'quit' to exit.")
    while true do
        io.write("> ")
        local line = io.read("*l")
        if not line then break end
        line = trim(line)
        if line == "quit" or line == "exit" then
            break
        end
        if #line == 0 then
            -- ignore
        else
            local cmd, a1, a2 = line:match("^(%S+)%s*(%S*)%s*(%S*)")
            if cmd == "ker" and a1 ~= "" and a2 ~= "" then
                ker_query(a1, a2)
            elseif cmd == "pfas" and a1 ~= "" and a2 ~= "" then
                pfas_query(a1, a2)
            elseif cmd == "fog" and a1 ~= "" and a2 ~= "" then
                fog_query(a1, a2)
            else
                print("Commands:")
                print("  ker  <db> <hex_id>     - query KER/Lyapunov for a hex")
                print("  pfas <db> <node_code>  - show PFAS corridor state for a canal node")
                print("  fog  <csv> <node_code> - route telemetry row into FOG band")
                print("  quit                   - exit")
            end
        end
    end
end

repl()
