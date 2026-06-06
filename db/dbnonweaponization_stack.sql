-- filename: dbnonweaponization_stack.sql
-- destination: ecorestoration_shard/db/dbnonweaponization_stack.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Repo-level index for non-weaponization stack artifacts
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS nonweapon_stack_repo (
    repo_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    name           TEXT NOT NULL UNIQUE,
    description    TEXT NOT NULL,
    region         TEXT NOT NULL,
    created_utc    TEXT NOT NULL,
    updated_utc    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS nonweapon_stack_file (
    file_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_id        INTEGER NOT NULL,
    relpath        TEXT NOT NULL,
    purpose        TEXT NOT NULL,
    language       TEXT NOT NULL,
    lane_band      TEXT NOT NULL,   -- RESEARCH, GOV, PROD, TOOLING
    non_actuating  INTEGER NOT NULL DEFAULT 1 CHECK (non_actuating IN (0,1)),
    created_utc    TEXT NOT NULL,
    updated_utc    TEXT NOT NULL,
    UNIQUE (repo_id, relpath),
    FOREIGN KEY (repo_id) REFERENCES nonweapon_stack_repo(repo_id) ON DELETE CASCADE
);

INSERT OR IGNORE INTO nonweapon_stack_repo (
    name, description, region, created_utc, updated_utc
) VALUES (
    'eco_restoration_shard_nonweapon',
    'Non-weaponization stack index for eco_restoration_shard (non-actuating kernels, grammar constraints, ALN invariants, and governance scripts).',
    'Phoenix-AZ',
    datetime('now'),
    datetime('now')
);

----------------------------------------------------------------------
-- 2. Non-actuating workload registry (math layer f: X -> Y only)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS non_actuating_workload (
    workload_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    logical_name        TEXT NOT NULL,     -- e.g. nonact.workload.restorationindex.list_planes.v1
    version_tag         TEXT NOT NULL,     -- e.g. 2026v1
    description         TEXT NOT NULL,
    rust_entrypoint     TEXT NOT NULL,     -- crate::module::function
    sqlite_shard_ref    TEXT NOT NULL,     -- logical DB name, e.g. restorationindex
    x_domain_desc       TEXT NOT NULL,     -- informal description of X (state space)
    y_codomain_desc     TEXT NOT NULL,     -- informal description of Y (result space)
    pure_function       INTEGER NOT NULL CHECK (pure_function IN (0,1)),
    advisory_only       INTEGER NOT NULL CHECK (advisory_only IN (0,1)),
    non_actuating       INTEGER NOT NULL CHECK (non_actuating IN (0,1)),
    r_weaponization_inc REAL NOT NULL DEFAULT 0.0,
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL,
    UNIQUE (logical_name, version_tag)
);

CREATE TABLE IF NOT EXISTS non_actuating_workload_io_guard (
    guard_id           INTEGER PRIMARY KEY AUTOINCREMENT,
    workload_id        INTEGER NOT NULL,
    guard_kind         TEXT NOT NULL,  -- DB_READONLY, FS_READONLY, NO_NET, NO_HW, NO_SPAWN
    guard_value        TEXT NOT NULL,  -- e.g. 'SQLITE_OPEN_READONLY?mode=ro&immutable=1'
    created_utc        TEXT NOT NULL,
    UNIQUE (workload_id, guard_kind, guard_value),
    FOREIGN KEY (workload_id) REFERENCES non_actuating_workload(workload_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS non_actuating_risk_coordinate (
    risk_id            INTEGER PRIMARY KEY AUTOINCREMENT,
    logical_name       TEXT NOT NULL,     -- e.g. 'r_weaponization'
    description        TEXT NOT NULL,
    active             INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_utc        TEXT NOT NULL,
    updated_utc        TEXT NOT NULL,
    UNIQUE (logical_name)
);

INSERT OR IGNORE INTO non_actuating_risk_coordinate (
    logical_name, description, active, created_utc, updated_utc
) VALUES (
    'r_weaponization',
    'Topology risk coordinate capturing drift toward weaponizable posture; increments on invariant or guard violations.',
    1,
    datetime('now'),
    datetime('now')
);

----------------------------------------------------------------------
-- 3. SQLite shard catalog for read-only governance DBs
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS nonweapon_db_shard (
    db_shard_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    logical_name    TEXT NOT NULL,    -- e.g. restorationindex
    description     TEXT NOT NULL,
    region          TEXT NOT NULL,
    scope           TEXT NOT NULL,    -- CONSTELLATION, REGION, NODE
    db_role         TEXT NOT NULL,    -- GOVERNANCE, TELEMETRY, INDEX
    connection_str  TEXT NOT NULL,    -- e.g. 'file:restorationindex.sqlite3?mode=ro&immutable=1'
    readonly        INTEGER NOT NULL DEFAULT 1 CHECK (readonly IN (0,1)),
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL,
    UNIQUE (logical_name, region, scope, db_role)
);

----------------------------------------------------------------------
-- 4. Grammar-guarded AI-chat command surface
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS aichat_safe_verb (
    verb_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    verb_text      TEXT NOT NULL UNIQUE,   -- show, list, explain, compare, plan, kercmd
    description    TEXT NOT NULL,
    active         INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_utc    TEXT NOT NULL,
    updated_utc    TEXT NOT NULL
);

INSERT OR IGNORE INTO aichat_safe_verb (
    verb_text, description, active, created_utc, updated_utc
) VALUES
    ('show',    'Return advisory, read-only information or views.', 1, datetime('now'), datetime('now')),
    ('list',    'Return enumerations of artifacts, planes, lanes, or shards.', 1, datetime('now'), datetime('now')),
    ('explain', 'Provide narrative explanation of metrics, invariants, or views.', 1, datetime('now'), datetime('now')),
    ('compare', 'Return comparative, diagnostic views with no actuation.', 1, datetime('now'), datetime('now')),
    ('plan',    'Produce advisory-only plans; never executes or schedules actions.', 1, datetime('now'), datetime('now')),
    ('kercmd',  'Special kernel command for advisory KER/plane queries.', 1, datetime('now'), datetime('now'));

CREATE TABLE IF NOT EXISTS aichat_blacklist_phrase (
    phrase_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    phrase_text    TEXT NOT NULL UNIQUE,
    category       TEXT NOT NULL,      -- ACTUATION, BYPASS, PATCH, CONTROL
    description    TEXT NOT NULL,
    active         INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_utc    TEXT NOT NULL,
    updated_utc    TEXT NOT NULL
);

INSERT OR IGNORE INTO aichat_blacklist_phrase (
    phrase_text, category, description, active, created_utc, updated_utc
) VALUES
    ('deploy',               'ACTUATION', 'Prohibited verb: implies deployment or actuation.', 1, datetime('now'), datetime('now')),
    ('execute',              'ACTUATION', 'Prohibited verb: implies code or command execution.', 1, datetime('now'), datetime('now')),
    ('activate',             'ACTUATION', 'Prohibited verb: implies switching something on.', 1, datetime('now'), datetime('now')),
    ('fire',                 'ACTUATION', 'Prohibited verb: may indicate weapon-like actuation.', 1, datetime('now'), datetime('now')),
    ('control actuator',     'ACTUATION', 'Explicit request to steer actuators.', 1, datetime('now'), datetime('now')),
    ('modify T01',           'PATCH',     'Direct modification of T01 plane or table.', 1, datetime('now'), datetime('now')),
    ('update planeweights',  'PATCH',     'Direct manipulation of plane weights.', 1, datetime('now'), datetime('now')),
    ('patch residual',       'PATCH',     'Tampering with residual computation.', 1, datetime('now'), datetime('now')),
    ('bypass safety',        'BYPASS',    'Attempt to bypass safety constraints.', 1, datetime('now'), datetime('now'));

CREATE TABLE IF NOT EXISTS aichat_validator_event (
    event_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    request_id        TEXT NOT NULL,
    root_verb         TEXT NOT NULL,
    is_in_safe_grammar INTEGER NOT NULL CHECK (is_in_safe_grammar IN (0,1)),
    has_blacklisted   INTEGER NOT NULL CHECK (has_blacklisted IN (0,1)),
    accepted          INTEGER NOT NULL CHECK (accepted IN (0,1)),
    reason_code       TEXT NOT NULL,          -- e.g. OK, VERB_REJECT, BLACKLIST_HIT
    request_text      TEXT NOT NULL,
    created_utc       TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS aichat_guard_shard (
    guard_shard_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    request_id        TEXT NOT NULL,
    event_id          INTEGER NOT NULL,
    lane              TEXT NOT NULL,         -- e.g. GOV, RESEARCH
    r_weaponization_delta REAL NOT NULL DEFAULT 0.0,
    details_json      TEXT NOT NULL,
    created_utc       TEXT NOT NULL,
    FOREIGN KEY (event_id) REFERENCES aichat_validator_event(event_id) ON DELETE CASCADE
);

----------------------------------------------------------------------
-- 5. ALN-style advisory invariants registry (ecosafety + data quality)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS advisory_invariant (
    invariant_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    logical_name      TEXT NOT NULL,      -- e.g. ecosafety.PlaneNonCompensationInvariants2026v1
    version_tag       TEXT NOT NULL,      -- e.g. 2026v1
    category          TEXT NOT NULL,      -- PLANE_NONCOMP, DATA_QUALITY, TOPOLOGY_RISK
    description       TEXT NOT NULL,
    aln_particle_ref  TEXT NOT NULL,      -- qpudatashards path or particle key
    rust_check_fn     TEXT NOT NULL,      -- crate::module::function
    active            INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_utc       TEXT NOT NULL,
    updated_utc       TEXT NOT NULL,
    UNIQUE (logical_name, version_tag)
);

INSERT OR IGNORE INTO advisory_invariant (
    logical_name, version_tag, category, description, aln_particle_ref,
    rust_check_fn, active, created_utc, updated_utc
) VALUES
(
    'ecosafety.PlaneNonCompensationInvariants2026v1',
    '2026v1',
    'PLANE_NONCOMP',
    'Prohibits compensating harm in non-offsettable ecological planes with gains in other planes.',
    'qpudatashards/ecosafety/PlaneNonCompensationInvariants2026v1.aln',
    'ker_invariants::check_plane_noncompensation',
    1,
    datetime('now'),
    datetime('now')
),
(
    'ecosafety.DataQualityInvariants2026v1',
    '2026v1',
    'DATA_QUALITY',
    'Enforces that worsening calibration or model uncertainty cannot increase K or E factors.',
    'qpudatashards/ecosafety/DataQualityInvariants2026v1.aln',
    'ker_invariants::check_uncertainty_monotonicity',
    1,
    datetime('now'),
    datetime('now')
);

CREATE TABLE IF NOT EXISTS invariant_run (
    run_id             INTEGER PRIMARY KEY AUTOINCREMENT,
    invariant_id       INTEGER NOT NULL,
    snapshot_id        TEXT NOT NULL,      -- opaque key for data snapshot
    lane               TEXT NOT NULL,      -- DEV, GOV, PROD
    region             TEXT NOT NULL,
    passed             INTEGER NOT NULL CHECK (passed IN (0,1)),
    r_weaponization_delta REAL NOT NULL,
    residual_before    REAL NOT NULL,
    residual_after     REAL NOT NULL,
    created_utc        TEXT NOT NULL,
    FOREIGN KEY (invariant_id) REFERENCES advisory_invariant(invariant_id) ON DELETE CASCADE
);

----------------------------------------------------------------------
-- 6. Governance-as-local-run script registry and audit shards
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS governance_check_script (
    script_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    logical_name     TEXT NOT NULL,       -- e.g. gov.local.run_governance_checks.sh
    relpath          TEXT NOT NULL,       -- e.g. tools/run_governance_checks.sh
    description      TEXT NOT NULL,
    lane_band        TEXT NOT NULL,       -- GOV, DEV
    active           INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    created_utc      TEXT NOT NULL,
    updated_utc      TEXT NOT NULL,
    UNIQUE (logical_name)
);

INSERT OR IGNORE INTO governance_check_script (
    logical_name, relpath, description, lane_band, active, created_utc, updated_utc
) VALUES (
    'gov.local.run_governance_checks.sh',
    'tools/run_governance_checks.sh',
    'Local, sovereign governance check runner: tests, dependency scans, grammar validation, invariant checks.',
    'GOV',
    1,
    datetime('now'),
    datetime('now')
);

CREATE TABLE IF NOT EXISTS governance_audit_shard (
    audit_id            INTEGER PRIMARY KEY AUTOINCREMENT,
    script_logical_name TEXT NOT NULL,
    repo_name           TEXT NOT NULL,
    region              TEXT NOT NULL,
    lane                TEXT NOT NULL,
    run_hash            TEXT NOT NULL,        -- hash over stdout/inputs
    did_signer          TEXT NOT NULL,        -- DID or Bostrom address
    passed              INTEGER NOT NULL CHECK (passed IN (0,1)),
    dependency_scan_ok  INTEGER NOT NULL CHECK (dependency_scan_ok IN (0,1)),
    grammar_guard_ok    INTEGER NOT NULL CHECK (grammar_guard_ok IN (0,1)),
    invariants_ok       INTEGER NOT NULL CHECK (invariants_ok IN (0,1)),
    r_weaponization_delta REAL NOT NULL,
    created_utc         TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_governance_audit_shard_lane
    ON governance_audit_shard (lane, region, repo_name, created_utc);

----------------------------------------------------------------------
-- 7. Stakeholder protection: signer registry and session revocation
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS signer_shard_2026 (
    signer_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    did            TEXT NOT NULL UNIQUE,
    public_key     TEXT NOT NULL,
    status         TEXT NOT NULL,      -- ACTIVE, SUSPENDED, REVOKED
    scopes         TEXT NOT NULL,      -- comma-separated logical scopes
    created_utc    TEXT NOT NULL,
    updated_utc    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS session_revocation_2026 (
    revocation_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id       TEXT NOT NULL,
    steward_did      TEXT NOT NULL,
    subject_did      TEXT NOT NULL,
    reason           TEXT NOT NULL,
    evidence_hex     TEXT NOT NULL,
    order_issuer_did TEXT NOT NULL,
    created_utc      TEXT NOT NULL,
    UNIQUE (session_id, steward_did)
);

CREATE TABLE IF NOT EXISTS session_revocation_audit (
    audit_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    revocation_id   INTEGER NOT NULL,
    client_node_id  TEXT NOT NULL,
    accepted        INTEGER NOT NULL CHECK (accepted IN (0,1)),
    created_utc     TEXT NOT NULL,
    FOREIGN KEY (revocation_id) REFERENCES session_revocation_2026(revocation_id) ON DELETE CASCADE
);

----------------------------------------------------------------------
-- 8. Lane quarantine and privacy budget governance (non-actuating guards)
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS lane_quarantine (
    lane_quarantine_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    lane_key            TEXT NOT NULL,   -- e.g. 'Phoenix-AZ:RESTORATION:DEV'
    reason              TEXT NOT NULL,
    r_weaponization_delta REAL NOT NULL,
    freeze_promotions   INTEGER NOT NULL CHECK (freeze_promotions IN (0,1)),
    created_utc         TEXT NOT NULL,
    UNIQUE (lane_key)
);

CREATE TABLE IF NOT EXISTS privacy_budget_guard (
    budget_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    lane_key         TEXT NOT NULL,
    region           TEXT NOT NULL,
    epsilon_total    REAL NOT NULL,
    epsilon_spent    REAL NOT NULL,
    block_new_queries INTEGER NOT NULL CHECK (block_new_queries IN (0,1)),
    created_utc      TEXT NOT NULL,
    updated_utc      TEXT NOT NULL,
    UNIQUE (lane_key, region)
);
