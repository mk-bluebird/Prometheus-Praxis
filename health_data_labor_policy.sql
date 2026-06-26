-- filename: health_data_labor_policy.sql
-- destination: ecorestorationshard/health_data_labor_policy.sql
--
-- HealthData labor, union governance, and neurorights non-downgrade policy tables.
-- Tightly coupled to existing HealthDataTCR and continuity schemas.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Labor policy core: categories, base rates, and sovereignty factors
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS labor_category_policy (
    category_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    category_code       TEXT NOT NULL UNIQUE,  -- e.g. 'aichatsummary', 'biosignalstream'
    description         TEXT NOT NULL,
    base_ecocredit_rate REAL NOT NULL CHECK (base_ecocredit_rate >= 0.0),
    sovereignty_cost_factor REAL NOT NULL CHECK (sovereignty_cost_factor >= 0.0),
    max_daily_credits   REAL NOT NULL CHECK (max_daily_credits >= 0.0),
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL
);

INSERT OR IGNORE INTO labor_category_policy (
    category_id, category_code, description,
    base_ecocredit_rate, sovereignty_cost_factor, max_daily_credits,
    created_utc, updated_utc
) VALUES
    (1, 'aichatsummary',
        'AI chat summaries used as HASHONLY labor events.',
        1.0, 1.0, 50.0,
        datetime('now'), datetime('now')),
    (2, 'biosignalstream',
        'Time-bounded biosignal events subject to strict neurorights corridors.',
        1.0, 2.0, 100.0,
        datetime('now'), datetime('now'));

----------------------------------------------------------------------
-- 2. HealthDataLaborCredit issuance log
--    Links to existing laborevent table (id, brainid, occurredat, category, ...).
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS health_data_labor_credit (
    credit_id           INTEGER PRIMARY KEY AUTOINCREMENT,
    laborevent_id       INTEGER NOT NULL,
    brainid             INTEGER NOT NULL,  -- REFERENCES brainidentity(id)
    category_code       TEXT NOT NULL,
    issued_utc          TEXT NOT NULL,
    effort_score_local  REAL NOT NULL CHECK (effort_score_local >= 0.0),
    responsibility_scalar REAL NOT NULL CHECK (responsibility_scalar >= 0.0),
    marginal_knowledge_gain REAL NOT NULL CHECK (marginal_knowledge_gain >= 0.0),
    ecocredits_issued   REAL NOT NULL CHECK (ecocredits_issued >= 0.0),
    corridor_safe       INTEGER NOT NULL CHECK (corridor_safe IN (0,1)),
    notes               TEXT,
    UNIQUE (laborevent_id),
    FOREIGN KEY (laborevent_id) REFERENCES laborevent(id) ON DELETE CASCADE
);

-- View to help Rust check daily caps per brain and category.
CREATE VIEW IF NOT EXISTS v_daily_labor_credit_summary AS
SELECT
    h.brainid,
    h.category_code,
    date(h.issued_utc) AS day_utc,
    SUM(h.ecocredits_issued) AS daily_credits
FROM health_data_labor_credit AS h
GROUP BY h.brainid, h.category_code, day_utc;

----------------------------------------------------------------------
-- 3. Data unions and collective bargaining primitives
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS data_union (
    union_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    union_code      TEXT NOT NULL UNIQUE,
    display_name    TEXT NOT NULL,
    description     TEXT NOT NULL,
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS data_union_membership (
    membership_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    union_id        INTEGER NOT NULL,
    brainid         INTEGER NOT NULL,     -- REFERENCES brainidentity(id)
    joined_utc      TEXT NOT NULL,
    left_utc        TEXT,
    mandate_scope   TEXT NOT NULL,        -- ALN ref to allowed negotiation scope
    active          INTEGER NOT NULL CHECK (active IN (0,1)),
    UNIQUE(union_id, brainid),
    FOREIGN KEY (union_id) REFERENCES data_union(union_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS data_union_corridor_proposal (
    proposal_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    union_id        INTEGER NOT NULL,
    label           TEXT NOT NULL,
    description     TEXT NOT NULL,
    aln_ref         TEXT NOT NULL,      -- ALN shard or record id describing corridor change
    created_utc     TEXT NOT NULL,
    status          TEXT NOT NULL CHECK (status IN ('PENDING','APPROVED','REJECTED','WITHDRAWN')),
    neurorights_ok  INTEGER NOT NULL CHECK (neurorights_ok IN (0,1)),
    psychological_continuity_ok INTEGER NOT NULL CHECK (psychological_continuity_ok IN (0,1)),
    FOREIGN KEY (union_id) REFERENCES data_union(union_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS data_union_vote (
    vote_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    proposal_id     INTEGER NOT NULL,
    brainid         INTEGER NOT NULL,
    union_id        INTEGER NOT NULL,
    cast_utc        TEXT NOT NULL,
    vote_value      INTEGER NOT NULL CHECK (vote_value IN (-1,0,1)), -- no, abstain, yes
    ecocredit_weight REAL NOT NULL CHECK (ecocredit_weight >= 0.0),
    responsibility_scalar REAL NOT NULL CHECK (responsibility_scalar >= 0.0),
    UNIQUE(proposal_id, brainid),
    FOREIGN KEY (proposal_id) REFERENCES data_union_corridor_proposal(proposal_id) ON DELETE CASCADE,
    FOREIGN KEY (union_id) REFERENCES data_union(union_id) ON DELETE CASCADE
);

----------------------------------------------------------------------
-- 4. Neurorights non-downgrade and psychological continuity guarantees
----------------------------------------------------------------------

-- Table enumerating non-downgrade rules that must hold for labor and unions.
CREATE TABLE IF NOT EXISTS neurorights_non_downgrade_rule (
    rule_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    rule_code       TEXT NOT NULL UNIQUE,  -- e.g. 'NeurorightsNoDowngradeForNonDisclosure'
    description     TEXT NOT NULL,
    active          INTEGER NOT NULL CHECK (active IN (0,1)),
    created_utc     TEXT NOT NULL,
    updated_utc     TEXT NOT NULL
);

INSERT OR IGNORE INTO neurorights_non_downgrade_rule (
    rule_code, description, active, created_utc, updated_utc
) VALUES
    ('NeurorightsNoDowngradeForNonDisclosure',
     'Refusal to share new data or deletion of existing data must never reduce neurorights protections, access to baseline care, or psychological continuity guarantees.',
     1, datetime('now'), datetime('now')),
    ('PsychContinuityNoWeaken',
     'Any policy change, union corridor negotiation, or labor contract must not reduce psychologicalcontinuityscore or weaken continuity corridors for any BrainDid.',
     1, datetime('now'), datetime('now'));

----------------------------------------------------------------------
-- 5. Optional helper: prevent recording credits if psychological continuity worsens
--
-- This is advisory; Rust invariants should enforce it, but the view helps audits.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_psych_continuity_labor_guard AS
SELECT
    h.credit_id,
    h.brainid,
    h.laborevent_id,
    h.ecocredits_issued,
    b.psychologicalcontinuityscore AS current_psych_score
FROM health_data_labor_credit AS h
JOIN brainidentity AS b ON b.id = h.brainid;
