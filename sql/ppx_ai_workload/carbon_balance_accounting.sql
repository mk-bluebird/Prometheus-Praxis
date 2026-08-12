-- File: sql/ppx_ai_workload/carbon_balance_accounting.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS ppx_workload_emissions (
    workload_id TEXT PRIMARY KEY,
    completed_utc TEXT NOT NULL,
    emissions_g REAL NOT NULL CHECK(emissions_g >= 0.0)
) STRICT;

CREATE TABLE IF NOT EXISTS ppx_verified_offset (
    offset_id TEXT PRIMARY KEY,
    verified_g REAL NOT NULL CHECK(verified_g > 0.0),
    issued_utc TEXT NOT NULL,
    expires_utc TEXT NOT NULL,
    verification_reference TEXT NOT NULL,
    state TEXT NOT NULL CHECK(state IN ('ACTIVE', 'EXHAUSTED', 'EXPIRED'))
) STRICT;

CREATE TABLE IF NOT EXISTS ppx_offset_allocation (
    offset_id TEXT NOT NULL REFERENCES ppx_verified_offset(offset_id),
    workload_id TEXT NOT NULL REFERENCES ppx_workload_emissions(workload_id),
    allocated_g REAL NOT NULL CHECK(allocated_g > 0.0),
    PRIMARY KEY(offset_id, workload_id)
) STRICT;

CREATE TRIGGER IF NOT EXISTS ppx_offset_expiry_guard
BEFORE INSERT ON ppx_offset_allocation
FOR EACH ROW
WHEN NOT EXISTS (
    SELECT 1
    FROM ppx_verified_offset AS offset
    JOIN ppx_workload_emissions AS workload ON workload.workload_id = NEW.workload_id
    WHERE offset.offset_id = NEW.offset_id
      AND offset.state = 'ACTIVE'
      AND offset.expires_utc >= workload.completed_utc
      AND NEW.allocated_g <= offset.verified_g - COALESCE((
          SELECT SUM(allocated_g) FROM ppx_offset_allocation
          WHERE offset_id = NEW.offset_id
      ), 0.0)
)
BEGIN
    SELECT RAISE(ABORT, 'offset unavailable, expired, or over-allocated');
END;

CREATE TRIGGER IF NOT EXISTS ppx_offset_state_after_allocation
AFTER INSERT ON ppx_offset_allocation
FOR EACH ROW
BEGIN
    UPDATE ppx_verified_offset
    SET state = 'EXHAUSTED'
    WHERE offset_id = NEW.offset_id
      AND verified_g <= (
          SELECT COALESCE(SUM(allocated_g), 0.0)
          FROM ppx_offset_allocation
          WHERE offset_id = NEW.offset_id
      );
END;

CREATE VIEW IF NOT EXISTS v_ppx_monthly_carbon_balance AS
WITH emissions AS (
    SELECT strftime('%Y-%m', completed_utc) AS month_utc, SUM(emissions_g) AS emissions_g
    FROM ppx_workload_emissions
    GROUP BY month_utc
),
offsets AS (
    SELECT
        strftime('%Y-%m', workload.completed_utc) AS month_utc,
        SUM(allocation.allocated_g) AS verified_offsets_g
    FROM ppx_offset_allocation AS allocation
    JOIN ppx_workload_emissions AS workload ON workload.workload_id = allocation.workload_id
    JOIN ppx_verified_offset AS offset ON offset.offset_id = allocation.offset_id
    WHERE offset.expires_utc >= workload.completed_utc
      AND offset.verification_reference <> ''
    GROUP BY month_utc
)
SELECT
    emissions.month_utc,
    emissions.emissions_g,
    COALESCE(offsets.verified_offsets_g, 0.0) AS verified_offsets_g,
    emissions.emissions_g - COALESCE(offsets.verified_offsets_g, 0.0) AS net_carbon_g
FROM emissions
LEFT JOIN offsets USING(month_utc);

CREATE TABLE IF NOT EXISTS ppx_monthly_carbon_close (
    month_utc TEXT PRIMARY KEY,
    closed_utc TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
) STRICT;

CREATE TRIGGER IF NOT EXISTS ppx_carbon_negative_month_close
BEFORE INSERT ON ppx_monthly_carbon_close
FOR EACH ROW
WHEN COALESCE((
    SELECT net_carbon_g FROM v_ppx_monthly_carbon_balance
    WHERE month_utc = NEW.month_utc
), 0.0) > 0.0
BEGIN
    SELECT RAISE(ABORT, 'monthly workload ledger is not carbon-negative');
END;
