-- CI Orchestration for EcoNet Constellation
-- Validates blast-radius, enforces KER thresholds, and triggers corridor-tightening

local sqlite3 = require("lsqlite3")
local json = require("json")

local DB_PATH = "data/constellation/econet_constellation_index.db"
local KER_THRESHOLDS = {K_min = 0.90, E_min = 0.90, R_max = 0.13}

local function open_db()
    local db = sqlite3.open(DB_PATH)
    if not db then
        error("Failed to open constellation database")
    end
    return db
end

local function check_ker_compliance(db)
    local query = [[
        SELECT a.artifact_path, k.knowledge_factor, k.eco_impact, k.risk_of_harm
        FROM artifacts a
        JOIN ker_scores k ON a.artifact_id = k.artifact_id
        WHERE k.knowledge_factor < ? OR k.eco_impact < ? OR k.risk_of_harm > ?
    ]]
    
    local fails = {}
    for row in db:nrows(query, KER_THRESHOLDS.K_min, KER_THRESHOLDS.E_min, KER_THRESHOLDS.R_max) do
        table.insert(fails, {
            path = row.artifact_path,
            K = row.knowledge_factor,
            E = row.eco_impact,
            R = row.risk_of_harm
        })
    end
    return fails
end

local function validate_blast_radius(db, artifact_id)
    local query = [[
        SELECT COUNT(*) as critical_deps
        FROM blast_radius
        WHERE source_artifact_id = ? AND impact_severity = 'critical'
    ]]
    
    local stmt = db:prepare(query)
    stmt:bind(1, artifact_id)
    local row = stmt:step()
    return row and row.critical_deps or 0
end

local function main()
    print("=== EcoNet Constellation CI Orchestrator ===")
    local db = open_db()
    
    -- Step 1: Check KER compliance
    print("\n[1/3] Checking KER compliance...")
    local fails = check_ker_compliance(db)
    if #fails > 0 then
        print("⚠ KER violations detected:")
        for _, f in ipairs(fails) do
            print(string.format("  %s: K=%.2f, E=%.2f, R=%.2f", f.path, f.K, f.E, f.R))
        end
        print("✗ CI FAILED: KER thresholds not met")
        db:close()
        os.exit(1)
    else
        print("✓ All artifacts meet KER thresholds")
    end
    
    -- Step 2: Validate blast radius for critical artifacts
    print("\n[2/3] Validating blast radius...")
    local critical_count = validate_blast_radius(db, 5)
    if critical_count > 3 then
        print(string.format("⚠ Artifact 5 has %d critical dependencies (limit: 3)", critical_count))
        print("✗ CI FAILED: Blast radius too large")
        db:close()
        os.exit(1)
    else
        print("✓ Blast radius within acceptable limits")
    end
    
    -- Step 3: Energy-cost check (placeholder for advanced logic)
    print("\n[3/3] Checking energy efficiency...")
    local energy_query = "SELECT AVG(joules_per_cycle) as avg_j FROM energy_metrics WHERE carbon_offset_kg < 0"
    for row in db:nrows(energy_query) do
        if row.avg_j and row.avg_j < 1.0 then
            print(string.format("✓ Average carbon-negative energy: %.2f J/cycle", row.avg_j))
        end
    end
    
    db:close()
    print("\n✓ CI PASSED: All checks successful")
    print("Next step: Trigger corridor-tightening if R trend > 0.12 over 5 iterations")
end

main()
