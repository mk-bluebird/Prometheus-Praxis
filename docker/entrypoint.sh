# File: docker/entrypoint.sh
#!/bin/sh
set -eu

database_path="${PROMETHEUS_PRAXIS_DB:-/data/prometheus_praxis.db}"
database_directory=$(dirname "$database_path")

mkdir -p "$database_directory"

sqlite3 "$database_path" <<'SQL'
PRAGMA journal_mode=WAL;
PRAGMA foreign_keys=ON;

CREATE TABLE IF NOT EXISTS hex_stability_carbon (
    hex_id TEXT PRIMARY KEY,
    band TEXT NOT NULL,
    ker_k REAL NOT NULL CHECK(ker_k BETWEEN 0.0 AND 1.0),
    ker_e REAL NOT NULL CHECK(ker_e BETWEEN 0.0 AND 1.0),
    ker_r REAL NOT NULL CHECK(ker_r BETWEEN 0.0 AND 1.0),
    carbon_intensity REAL NOT NULL CHECK(carbon_intensity >= 0.0),
    max_carbon REAL NOT NULL CHECK(max_carbon > 0.0),
    ker_s REAL NOT NULL CHECK(ker_s BETWEEN 0.0 AND 1.0),
    carbon_corridor REAL NOT NULL CHECK(carbon_corridor BETWEEN 0.0 AND 1.0),
    v_residual REAL NOT NULL CHECK(v_residual >= 0.0)
) STRICT;

CREATE TABLE IF NOT EXISTS schedule_cache (
    hex_id TEXT NOT NULL,
    hour INTEGER NOT NULL CHECK(hour BETWEEN 0 AND 23),
    priority REAL NOT NULL,
    ts TEXT NOT NULL,
    PRIMARY KEY(hex_id, hour, ts)
) STRICT;

CREATE INDEX IF NOT EXISTS schedule_cache_hex_time
ON schedule_cache(hex_id, ts);
SQL

export PROMETHEUS_PRAXIS_DB="$database_path"
exec "$@"
