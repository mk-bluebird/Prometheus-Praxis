-- filename: healthdata_tcr_schema.sql
-- repo: mk-bluebird/eco_restoration_shard
-- destination_hint: PATH_TO_BE_CHOSEN_IN_REPO_ROOT

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS brain_identity (
    id INTEGER PRIMARY KEY,
    external_ref TEXT NOT NULL,            -- e.g., Bostrom address, DID
    created_at TEXT NOT NULL,
    psychological_continuity_score REAL NOT NULL,  -- 0.0–1.0
    neurorights_ok INTEGER NOT NULL CHECK (neurorights_ok IN (0,1))
);

CREATE TABLE IF NOT EXISTS labor_event (
    id INTEGER PRIMARY KEY,
    brain_id INTEGER NOT NULL REFERENCES brain_identity(id),
    occurred_at TEXT NOT NULL,
    category TEXT NOT NULL,       -- "health_metric","ai_chat_summary","cyboquatic_telemetry"
    duration_seconds INTEGER NOT NULL,
    effort_score REAL NOT NULL,   -- local-only score; never monetized directly
    aln_document TEXT NOT NULL    -- serialized ALN fragment with rich attributes
);

CREATE TABLE IF NOT EXISTS sovereignty_rule (
    id INTEGER PRIMARY KEY,
    brain_id INTEGER NOT NULL REFERENCES brain_identity(id),
    rule_name TEXT NOT NULL,
    rule_version TEXT NOT NULL,
    rule_body TEXT NOT NULL,      -- ALN-encoded machine-readable policy
    active INTEGER NOT NULL CHECK (active IN (0,1))
);

CREATE TABLE IF NOT EXISTS eco_credit_ledger (
    id INTEGER PRIMARY KEY,
    brain_id INTEGER NOT NULL REFERENCES brain_identity(id),
    labor_event_id INTEGER NOT NULL REFERENCES labor_event(id),
    issued_at TEXT NOT NULL,
    amount REAL NOT NULL,
    non_transferable INTEGER NOT NULL CHECK (non_transferable = 1),
    responsibility_scalar REAL NOT NULL,
    cumulative_eco_credit REAL NOT NULL,
    CHECK (amount >= 0.0),
    CHECK (responsibility_scalar >= 0.0),
    CHECK (cumulative_eco_credit >= 0.0)
);

CREATE TABLE IF NOT EXISTS qf_project (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT NOT NULL,
    created_at TEXT NOT NULL,
    neurorights_compliant INTEGER NOT NULL CHECK (neurorights_compliant IN (0,1)),
    eco_focus TEXT NOT NULL,    -- "energy_reduction","waste_cleanup","governance_continuity", etc.
    active INTEGER NOT NULL CHECK (active IN (0,1))
);

CREATE TABLE IF NOT EXISTS qf_allocation (
    id INTEGER PRIMARY KEY,
    project_id INTEGER NOT NULL REFERENCES qf_project(id),
    brain_id INTEGER NOT NULL REFERENCES brain_identity(id),
    allocated_eco_credits REAL NOT NULL,
    responsibility_scalar REAL NOT NULL,
    effective_weight REAL NOT NULL,
    allocated_at TEXT NOT NULL,
    CHECK (allocated_eco_credits >= 0.0),
    CHECK (responsibility_scalar >= 0.0),
    CHECK (effective_weight >= 0.0)
);

CREATE TABLE IF NOT EXISTS qf_round (
    id INTEGER PRIMARY KEY,
    label TEXT NOT NULL,
    matching_pool REAL NOT NULL,
    starts_at TEXT NOT NULL,
    ends_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS qf_round_result (
    id INTEGER PRIMARY KEY,
    round_id INTEGER NOT NULL REFERENCES qf_round(id),
    project_id INTEGER NOT NULL REFERENCES qf_project(id),
    matched_amount REAL NOT NULL,
    total_effective_support REAL NOT NULL,
    CHECK (matched_amount >= 0.0),
    CHECK (total_effective_support >= 0.0)
);
