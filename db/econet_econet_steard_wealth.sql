-- filename: .econet_econet_steard_wealth.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS steward_eco_wealth_statement (
  statement_id      INTEGER PRIMARY KEY AUTOINCREMENT,
  steward_did       TEXT    NOT NULL, -- e.g. "bostrom18sd2u..."
  region_code       TEXT    NOT NULL, -- e.g. "Phoenix-AZ"
  ker_context       TEXT    NOT NULL, -- e.g. "PhoenixEcoSafetySpine2026v1"
  epoch_start_utc   TEXT    NOT NULL, -- ISO8601
  epoch_end_utc     TEXT    NOT NULL, -- ISO8601

  -- Linkage into discovery spine (optional, but strongly recommended).
  primary_repo_id   INTEGER,         -- REFERENCES repo(repoid)
  primary_shard_id  INTEGER,         -- REFERENCES shardinstance(shardid)
  evidencehex       TEXT    NOT NULL,
  signing_did       TEXT    NOT NULL,

  -- Aggregated KER over this window for this steward-region.
  k_metric          REAL    NOT NULL, -- 0..1
  e_metric          REAL    NOT NULL, -- 0..1
  r_metric          REAL    NOT NULL, -- 0..1
  vt_max            REAL    NOT NULL, -- max residual

  -- Regional scaling factor and corridor tier.
  s_region          REAL    NOT NULL, -- calibrated S_region
  wealth_tier       TEXT    NOT NULL CHECK (
                    wealth_tier IN ('SAFE','GOLD','HARD','BLOCKED')
                  ),

  -- Wealth components (all non-actuating accounting numbers).
  eco_units_gross   REAL    NOT NULL, -- before risk discounts
  eco_units_net     REAL    NOT NULL, -- after KER / data-quality discounts
  carbon_benefit_t  REAL    NOT NULL, -- metric tons CO2e avoided
  water_benefit_m3  REAL    NOT NULL, -- cubic meters water saved
  materials_offset_kg REAL  NOT NULL, -- hazardous mass avoided
  biodiversity_score REAL   NOT NULL, -- normalized positive biodiversity index

  -- Penalty factors (all in 0..1).
  data_quality_factor REAL  NOT NULL, -- e.g. D_combined = 1 - r_calib [file:16]
  uncertainty_factor   REAL NOT NULL, -- plane uncertainty caps
  governance_penalty   REAL NOT NULL, -- from r_topology [file:11]

  -- Turn-level KER/wealth audit flags.
  ker_deployable      INTEGER NOT NULL CHECK (ker_deployable IN (0,1)),
  corridor_breaches   INTEGER NOT NULL, -- count in window
  notes               TEXT
);

CREATE INDEX IF NOT EXISTS idx_steward_region_epoch
  ON steward_eco_wealth_statement (steward_did, region_code, epoch_start_utc, epoch_end_utc);

CREATE INDEX IF NOT EXISTS idx_steward_ker
  ON steward_eco_wealth_statement (k_metric, e_metric, r_metric);

CREATE INDEX IF NOT EXISTS idx_steward_sregion
  ON steward_eco_wealth_statement (region_code, s_region);

CREATE INDEX IF NOT EXISTS idx_steward_evidence
  ON steward_eco_wealth_statement (evidencehex);

-- Optional integration with existing discovery spine tables:
-- (Assumes tables repo(repoid) and shardinstance(shardid) already exist.) [file:11]
