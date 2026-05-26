-- .econet/econet_repo_index.sql (per‑repo master index)
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS econet_repo_index (
    repo_name        TEXT PRIMARY KEY,
    github_slug      TEXT NOT NULL,
    role_band        TEXT NOT NULL, -- SPINE, RESEARCH, ENGINE, MATERIAL, GOV, APP
    visibility       TEXT NOT NULL, -- Public, Private
    language_primary TEXT NOT NULL,
    description      TEXT,
    ecosafety_binding TEXT NOT NULL, -- e.g. cyboquatic-ecosafety-core/EcosafetyGrammar2026v1.aln
    shard_protocol   TEXT NOT NULL, -- e.g. ALN-RFC4180/EcoNetSchemaShard2026v1
    lane_default     TEXT NOT NULL, -- RESEARCH, EXPPROD, PROD
    ker_target_k     REAL NOT NULL,
    ker_target_e     REAL NOT NULL,
    ker_target_r     REAL NOT NULL,
    non_actuating_only INTEGER NOT NULL CHECK (non_actuating_only IN (0,1)),
    signing_did      TEXT,
    evidence_hex     TEXT
);

CREATE TABLE IF NOT EXISTS econet_layer (
    layer_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_name  TEXT NOT NULL REFERENCES econet_repo_index(repo_name) ON DELETE CASCADE,
    layer_name TEXT NOT NULL,
    layer_tier TEXT NOT NULL, -- GRAMMAR, KERNEL, EDGESCRIPT, UI, GOVERNANCE, MATERIAL, OTHER
    languages  TEXT NOT NULL, -- comma‑separated list
    description TEXT,
    contracts  TEXT           -- human‑readable invariants, e.g. "SafeKernel,NonActuatingWorkload"
);

CREATE TABLE IF NOT EXISTS econet_role_hint (
    hint_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_name TEXT NOT NULL REFERENCES econet_repo_index(repo_name) ON DELETE CASCADE,
    key       TEXT NOT NULL,
    value     TEXT NOT NULL
);
