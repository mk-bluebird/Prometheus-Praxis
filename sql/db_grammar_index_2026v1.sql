-- filename: db_grammar_index_2026v1.sql
-- destination: eco_restoration_shard/sql/db_grammar_index_2026v1.sql

PRAGMA foreign_keys = ON ;

----------------------------------------------------------------------
-- 1. ALN grammar artifact registry (EcoNet / Eco-Fort compatible)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS grammar_contract (
    contract_id     TEXT PRIMARY KEY,          -- e.g. 'ALNGrammarCore2026v1'
    scope           TEXT NOT NULL,             -- e.g. 'ALN_CORE', 'TELEMETRY_KERNEL'
    registry_version TEXT NOT NULL,            -- e.g. '2026v1'
    description     TEXT NOT NULL,
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS grammar_artifact (
    artifact_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    contract_id     TEXT NOT NULL REFERENCES grammar_contract(contract_id) ON DELETE CASCADE,
    kind            TEXT NOT NULL,             -- 'EBNF','BNF','ALN','SQL'
    repo_hint       TEXT NOT NULL,             -- e.g. 'eco_restoration_shard'
    destination     TEXT NOT NULL,             -- repo-relative path
    filename        TEXT NOT NULL,
    language        TEXT NOT NULL,             -- 'EBNF','BNF','SQLite'
    version_tag     TEXT NOT NULL,             -- '2026v1'
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    summary         TEXT NOT NULL,
    UNIQUE (contract_id, kind, destination, filename, version_tag)
);

CREATE INDEX IF NOT EXISTS idx_grammar_artifact_contract
    ON grammar_artifact (contract_id, active);

----------------------------------------------------------------------
-- 2. Seed rows for minimal ALN grammar coverage
----------------------------------------------------------------------

INSERT INTO grammar_contract (contract_id, scope, registry_version, description, created_utc, updated_utc)
VALUES
    ('ALNGrammarCore2026v1',
     'ALN_CORE',
     '2026v1',
     'Core ALN grammar for enums, records, sections, and meta blocks; minimal interface for any agent framework.',
     '2026-05-18T22:00:00Z',
     '2026-05-18T22:00:00Z'
    )
ON CONFLICT (contract_id) DO NOTHING;

INSERT INTO grammar_contract (contract_id, scope, registry_version, description, created_utc, updated_utc)
VALUES
    ('ALNTelemetryKernel2026v1',
     'TELEMETRY_KERNEL',
     '2026v1',
     'Minimal telemetry and residual snapshot particles for Lyapunov-aware KER aggregation.',
     '2026-05-18T22:00:00Z',
     '2026-05-18T22:00:00Z'
    )
ON CONFLICT (contract_id) DO NOTHING;

INSERT INTO grammar_artifact
    (contract_id, kind, repo_hint, destination, filename, language, version_tag, summary)
VALUES
    ('ALNGrammarCore2026v1',
     'EBNF',
     'eco_restoration_shard',
     'spec/aln/aln_grammar_core_2026v1.ebnf',
     'aln_grammar_core_2026v1.ebnf',
     'EBNF',
     '2026v1',
     'EBNF definition of the core ALN grammar for EcoNet/Eco-Fort.'
    ),
    ('ALNGrammarCore2026v1',
     'BNF',
     'eco_restoration_shard',
     'spec/aln/aln_grammar_core_2026v1.bnf',
     'aln_grammar_core_2026v1.bnf',
     'BNF',
     '2026v1',
     'BNF mirror of the core ALN grammar to support diverse parser generators.'
    ),
    ('ALNTelemetryKernel2026v1',
     'EBNF',
     'eco_restoration_shard',
     'spec/aln/aln_telemetry_kernel_2026v1.ebnf',
     'aln_telemetry_kernel_2026v1.ebnf',
     'EBNF',
     '2026v1',
     'Domain particle definitions for telemetry aggregation policy and shard residual snapshots.'
    )
ON CONFLICT (contract_id, kind, destination, filename, version_tag) DO NOTHING;
