-- filename: eco_restoration_shard/db/db_econet_stake_terminal_batch.sql
PRAGMA foreign_keys = ON;

-- Band-1, Phoenix-specific stake terminal batch for EcoNet Stake Terminal Q2 2026.
-- This table is read-mostly and non-actuating. It is designed to be round-trippable
-- with EcoNetStakeTerminalBatch2026Q2Phoenix.csv and ALN qpudatashards.

CREATE TABLE IF NOT EXISTS econet_stake_terminal_batch_2026q2_phx (
    batch_id TEXT NOT NULL,
    batch_seq INTEGER NOT NULL,
    batch_version TEXT NOT NULL,
    batch_window_start_utc TEXT NOT NULL,
    batch_window_end_utc TEXT NOT NULL,
    identity_id TEXT NOT NULL,
    steward_did TEXT NOT NULL,
    region_code TEXT NOT NULL,
    geohex_id TEXT NOT NULL,
    node_id TEXT NOT NULL,
    asset_class TEXT NOT NULL,
    lane TEXT NOT NULL CHECK (lane IN ('RESEARCH','EXPPROD','PROD','VIEW')),
    ceim_mass_tco2e REAL NOT NULL CHECK (ceim_mass_tco2e >= 0.0),
    karma_factor REAL NOT NULL CHECK (karma_factor >= 0.0),
    ker_k REAL NOT NULL CHECK (ker_k BETWEEN 0.0 AND 1.0),
    ker_e REAL NOT NULL CHECK (ker_e BETWEEN 0.0 AND 1.0),
    ker_r REAL NOT NULL CHECK (ker_r BETWEEN 0.0 AND 1.0),
    ker_contract_id TEXT NOT NULL,
    ker_window_id TEXT NOT NULL,
    karma_units_total REAL NOT NULL DEFAULT 0.0,
    karma_units_liquid REAL NOT NULL DEFAULT 0.0,
    karma_units_restricted REAL NOT NULL DEFAULT 0.0,
    eco_units_credited REAL NOT NULL DEFAULT 0.0,
    eco_units_liquid REAL NOT NULL DEFAULT 0.0,
    eco_units_restricted REAL NOT NULL DEFAULT 0.0,
    eco_liability_units REAL NOT NULL DEFAULT 0.0,
    eco_liability_reason TEXT NOT NULL DEFAULT 'NONE',
    t03_ledger_id TEXT NOT NULL,
    t03_ledger_window_id TEXT NOT NULL,
    t03_tx_id TEXT NOT NULL,
    t03_row_hash_hex TEXT NOT NULL,
    stake_contract_id TEXT NOT NULL,
    stake_instrument_id TEXT NOT NULL,
    stake_notional_ecounits REAL NOT NULL CHECK (stake_notional_ecounits >= 0.0),
    stake_lockup_days INTEGER NOT NULL CHECK (stake_lockup_days >= 0),
    stake_maturity_utc TEXT NOT NULL,
    stake_status TEXT NOT NULL CHECK (stake_status IN ('PENDING','ACTIVE','MATURED','CANCELLED','DEFAULTED')),
    k_delta REAL NOT NULL DEFAULT 0.0,
    e_delta REAL NOT NULL DEFAULT 0.0,
    r_delta REAL NOT NULL DEFAULT 0.0,
    vt_before REAL NOT NULL DEFAULT 0.0,
    vt_after REAL NOT NULL DEFAULT 0.0,
    vt_delta REAL NOT NULL DEFAULT 0.0,
    plane_energy_r REAL NOT NULL DEFAULT 0.0,
    plane_carbon_r REAL NOT NULL DEFAULT 0.0,
    plane_biodiversity_r REAL NOT NULL DEFAULT 0.0,
    plane_materials_r REAL NOT NULL DEFAULT 0.0,
    trust_plane_r REAL NOT NULL DEFAULT 0.0,
    dataquality_plane_r REAL NOT NULL DEFAULT 0.0,
    topology_plane_r REAL NOT NULL DEFAULT 0.0,
    resid_plane_r REAL NOT NULL DEFAULT 0.0,
    ecosafety_contract_id TEXT NOT NULL,
    lane_policy_id TEXT NOT NULL,
    lane_verdict_id TEXT NOT NULL,
    kerdeployable_window_flag INTEGER NOT NULL CHECK (kerdeployable_window_flag IN (0,1)),
    kerdeployable_reason TEXT NOT NULL DEFAULT 'UNKNOWN',
    evidence_hex TEXT NOT NULL,
    alnspec_hash TEXT NOT NULL,
    created_utc TEXT NOT NULL,
    created_by_did TEXT NOT NULL,
    ker_window_hash_hex TEXT NOT NULL,
    ker_recompute_flag INTEGER NOT NULL CHECK (ker_recompute_flag IN (0,1)),
    ker_recompute_reason TEXT NOT NULL DEFAULT 'NONE',
    ai_chat_session_id TEXT,
    ai_chat_turn_id TEXT,
    ai_chat_surface_id TEXT,
    dp_budget_id TEXT,
    dp_budget_delta REAL NOT NULL DEFAULT 0.0,
    knowledge_factor_before REAL NOT NULL DEFAULT 0.0,
    knowledge_factor_after REAL NOT NULL DEFAULT 0.0,
    ecoimpact_before REAL NOT NULL DEFAULT 0.0,
    ecoimpact_after REAL NOT NULL DEFAULT 0.0,
    risk_of_harm_before REAL NOT NULL DEFAULT 0.0,
    risk_of_harm_after REAL NOT NULL DEFAULT 0.0,
    ker_window_lane TEXT NOT NULL,
    ker_window_region TEXT NOT NULL,
    ker_window_geohex_id TEXT NOT NULL,
    ker_window_residual REAL NOT NULL DEFAULT 0.0,
    ker_window_r_max REAL NOT NULL DEFAULT 0.0,
    ecowealth_quarantine_state TEXT NOT NULL DEFAULT 'NONE',
    ecowealth_quarantine_cause TEXT NOT NULL DEFAULT 'NONE',
    ecowealth_quarantine_rule_id TEXT,
    ecowealth_quarantine_block_id TEXT,
    steward_portfolio_id TEXT NOT NULL,
    steward_portfolio_lane TEXT NOT NULL,
    steward_portfolio_ker_k REAL NOT NULL DEFAULT 0.0,
    steward_portfolio_ker_e REAL NOT NULL DEFAULT 0.0,
    steward_portfolio_ker_r REAL NOT NULL DEFAULT 0.0,
    steward_portfolio_vt REAL NOT NULL DEFAULT 0.0,
    phoenix_batch_label TEXT NOT NULL,
    phoenix_ceim_window_tag TEXT NOT NULL,
    phoenix_daily_surface_tag TEXT NOT NULL,
    phoenix_sensor_band TEXT NOT NULL,
    phoenix_trust_band TEXT NOT NULL,
    eco_units_floor_applied REAL NOT NULL DEFAULT 0.0,
    eco_units_floor_reason TEXT NOT NULL DEFAULT 'NONE',
    eco_units_cap_applied REAL NOT NULL DEFAULT 0.0,
    eco_units_cap_reason TEXT NOT NULL DEFAULT 'NONE',
    ker_window_ci_run_id TEXT NOT NULL,
    ker_window_ci_hash_hex TEXT NOT NULL,
    PRIMARY KEY (batch_id, batch_seq, identity_id, ker_window_id)
);

CREATE INDEX IF NOT EXISTS idx_econet_stake_batch_steward
    ON econet_stake_terminal_batch_2026q2_phx (steward_did, batch_window_start_utc);

CREATE INDEX IF NOT EXISTS idx_econet_stake_batch_region_lane
    ON econet_stake_terminal_batch_2026q2_phx (region_code, lane, ker_window_id);

CREATE INDEX IF NOT EXISTS idx_econet_stake_batch_t03
    ON econet_stake_terminal_batch_2026q2_phx (t03_ledger_id, t03_ledger_window_id, t03_tx_id);

CREATE VIEW IF NOT EXISTS v_econet_stake_karma_daily_phx AS
SELECT
    region_code,
    phoenix_daily_surface_tag AS daily_tag,
    DATE(batch_window_start_utc) AS day,
    SUM(karma_units_total) AS karma_units_total_sum,
    SUM(karma_units_liquid) AS karma_units_liquid_sum,
    SUM(karma_units_restricted) AS karma_units_restricted_sum,
    SUM(eco_units_credited) AS eco_units_credited_sum,
    SUM(eco_units_liquid) AS eco_units_liquid_sum,
    SUM(eco_units_restricted) AS eco_units_restricted_sum,
    SUM(eco_liability_units) AS eco_liability_units_sum
FROM econet_stake_terminal_batch_2026q2_phx
GROUP BY region_code, phoenix_daily_surface_tag, DATE(batch_window_start_utc);

CREATE VIEW IF NOT EXISTS v_econet_stake_ker_window_phx AS
SELECT
    ker_window_id,
    ker_window_lane,
    ker_window_region,
    ker_window_geohex_id,
    ker_window_residual,
    ker_window_r_max,
    AVG(ker_k) AS ker_k_avg,
    AVG(ker_e) AS ker_e_avg,
    MAX(ker_r) AS ker_r_max,
    SUM(karma_units_total) AS karma_units_total_sum,
    SUM(eco_units_credited) AS eco_units_credited_sum
FROM econet_stake_terminal_batch_2026q2_phx
GROUP BY ker_window_id, ker_window_lane, ker_window_region, ker_window_geohex_id, ker_window_residual, ker_window_r_max;
