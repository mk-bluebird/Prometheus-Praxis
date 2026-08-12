-- File: lua/ppx_ai_workload/carbon_uncertainty.lua
local sqlite3 = require("lsqlite3")
local carbon_uncertainty = {}

local function clamp(value, lower, upper)
    return math.max(lower, math.min(upper, value))
end

local function normal(mean, standard_deviation)
    local u1 = math.max(math.random(), 1e-12)
    local u2 = math.random()
    return mean + standard_deviation * math.sqrt(-2.0 * math.log(u1)) *
        math.cos(2.0 * math.pi * u2)
end

local function quantile(sorted, probability)
    local index = math.max(1, math.min(#sorted, math.ceil(probability * #sorted)))
    return sorted[index]
end

function carbon_uncertainty.simulate(record, samples, seed)
    assert(type(record) == "table" and record.energy_kwh >= 0.0)
    assert(record.forecast_renewable_fraction >= 0.0 and record.forecast_renewable_fraction <= 1.0)
    assert(record.renewable_sigma >= 0.0 and record.grid_carbon_g_per_kwh >= 0.0)
    assert(samples > 0)

    math.randomseed(seed or os.time())
    local values, total = {}, 0.0
    for i = 1, samples do
        local renewable = clamp(
            normal(record.forecast_renewable_fraction, record.renewable_sigma), 0.0, 1.0)
        local carbon_g = record.energy_kwh * (1.0 - renewable) * record.grid_carbon_g_per_kwh
        values[i], total = carbon_g, total + carbon_g
    end
    table.sort(values)
    return {
        mean_carbon_g = total / samples,
        p50_carbon_g = quantile(values, 0.50),
        p95_carbon_g = quantile(values, 0.95),
        samples = samples,
    }
end

function carbon_uncertainty.store(database_path, workload_id, observed_utc, result)
    local db = assert(sqlite3.open(database_path))
    assert(db:exec([[
        CREATE TABLE IF NOT EXISTS ppx_carbon_uncertainty (
            workload_id TEXT NOT NULL,
            observed_utc TEXT NOT NULL,
            sample_count INTEGER NOT NULL CHECK(sample_count > 0),
            mean_carbon_g REAL NOT NULL CHECK(mean_carbon_g >= 0.0),
            p50_carbon_g REAL NOT NULL CHECK(p50_carbon_g >= 0.0),
            p95_carbon_g REAL NOT NULL CHECK(p95_carbon_g >= p50_carbon_g),
            PRIMARY KEY(workload_id, observed_utc)
        ) STRICT;
    ]]) == sqlite3.OK)

    local statement = assert(db:prepare([[
        INSERT INTO ppx_carbon_uncertainty VALUES(?,?,?,?,?,?)
        ON CONFLICT(workload_id, observed_utc) DO UPDATE SET
          sample_count=excluded.sample_count, mean_carbon_g=excluded.mean_carbon_g,
          p50_carbon_g=excluded.p50_carbon_g, p95_carbon_g=excluded.p95_carbon_g;
    ]]))
    statement:bind_values(workload_id, observed_utc, result.samples, result.mean_carbon_g,
        result.p50_carbon_g, result.p95_carbon_g)
    assert(statement:step() == sqlite3.DONE)
    statement:finalize()
    db:close()
end

return carbon_uncertainty
