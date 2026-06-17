-- filename: eco_restoration_shard/db/db_responsibility_metrics.sql
-- destination: eco_restoration_shard/db/db_responsibility_metrics.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Core ResponsibilityAxis metrics table
--
-- This merges the earlier shard-scoped rresponsibility fields with the
-- richer, multi-axis responsibilitymetric design.
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS responsibilitymetric (
    responsibility_id      INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Scope of the responsibility row
    scopetype              TEXT    NOT NULL,  -- ACTOR, SHARD, REPO, NODE, REGION, PORTFOLIO
    scoperef               TEXT    NOT NULL,  -- e.g. actorid, shardid, reponame, nodeid, Phoenix-AZ

    -- Links into existing grammar spine where possible
    shardinstance_id       INTEGER,           -- FK into shardinstance.id when scopetype = 'SHARD'
    knowledgeecoscore_id   INTEGER,           -- FK into knowledgeecoscore.id when metric is derived
    region                 TEXT,              -- cached region for fast queries
    lane                   TEXT,              -- RESEARCH, EXPPROD, PROD, GOV, etc.
    window_start_utc       TEXT    NOT NULL,  -- ISO8601 window start (responsibility window)
    window_end_utc         TEXT    NOT NULL,  -- ISO8601 window end

    -- Aggregate responsibility coordinate r_responsibility in [0,1].
    rresponsibility        REAL    NOT NULL CHECK (rresponsibility >= 0.0 AND rresponsibility <= 1.0),

    -- Legacy single-axis decomposition for healthcare / MT6883 lanes.
    rpharma                REAL CHECK (rpharma      >= 0.0 AND rpharma      <= 1.0),
    rtoxicity              REAL CHECK (rtoxicity    >= 0.0 AND rtoxicity    <= 1.0),
    roverride              REAL CHECK (roverride    >= 0.0 AND roverride    <= 1.0),

    -- Multi-axis ResponsibilityAxis scores (normalized 0..1).
    energy_duty_score      REAL,
    ecoimpact_score        REAL,
    lifeforce_stewardship  REAL,
    roh_stewardship        REAL,
    topology_stewardship   REAL,
    data_quality_score     REAL,
    restoration_duty_score REAL,
    justice_duty_score     REAL,

    -- Aggregate responsibility index, for ranking (0..1, higher = better).
    responsibility_index   REAL,

    -- Meta / governance (aligned with knowledgeecoscore and residual grammar).
    kfactor                REAL,
    efactor                REAL,
    rfactor                REAL,
    vt_residual_est        REAL,

    evidence_shardid       TEXT,
    issuing_system         TEXT    NOT NULL,
    author_bostrom         TEXT,
    author_contract_id     TEXT,
    rationale_markdown     TEXT,

    created_utc            TEXT    NOT NULL,
    updated_utc            TEXT    NOT NULL,

    CHECK (responsibility_index   IS NULL OR (responsibility_index   >= 0.0 AND responsibility_index   <= 1.0)),
    CHECK (energy_duty_score      IS NULL OR (energy_duty_score      >= 0.0 AND energy_duty_score      <= 1.0)),
    CHECK (ecoimpact_score        IS NULL OR (ecoimpact_score        >= 0.0 AND ecoimpact_score        <= 1.0)),
    CHECK (lifeforce_stewardship  IS NULL OR (lifeforce_stewardship  >= 0.0 AND lifeforce_stewardship  <= 1.0)),
    CHECK (roh_stewardship        IS NULL OR (roh_stewardship        >= 0.0 AND roh_stewardship        <= 1.0)),
    CHECK (topology_stewardship   IS NULL OR (topology_stewardship   >= 0.0 AND topology_stewardship   <= 1.0)),
    CHECK (data_quality_score     IS NULL OR (data_quality_score     >= 0.0 AND data_quality_score     <= 1.0)),
    CHECK (restoration_duty_score IS NULL OR (restoration_duty_score >= 0.0 AND restoration_duty_score <= 1.0)),
    CHECK (justice_duty_score     IS NULL OR (justice_duty_score     >= 0.0 AND justice_duty_score     <= 1.0)),

    UNIQUE (scopetype, scoperef, window_start_utc, window_end_utc),

    FOREIGN KEY (shardinstance_id)     REFERENCES shardinstance(id),
    FOREIGN KEY (knowledgeecoscore_id) REFERENCES knowledgeecoscore(id)
);

CREATE INDEX IF NOT EXISTS idx_responsibilitymetric_scope
    ON responsibilitymetric (scopetype, scoperef, window_end_utc);

CREATE INDEX IF NOT EXISTS idx_responsibilitymetric_region_lane
    ON responsibilitymetric (region, lane, window_end_utc);

CREATE INDEX IF NOT EXISTS idx_responsibilitymetric_index
    ON responsibilitymetric (responsibility_index DESC, window_end_utc);

CREATE INDEX IF NOT EXISTS idx_responsibilitymetric_links
    ON responsibilitymetric (shardinstance_id, knowledgeecoscore_id);

-------------------------------------------------------------------------------
-- 2. Axis dictionary: responsibilityaxisdefinition
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS responsibilityaxisdefinition (
    axis_id            INTEGER PRIMARY KEY AUTOINCREMENT,
    axis_name          TEXT    NOT NULL,
    axis_label         TEXT    NOT NULL,
    axis_description   TEXT    NOT NULL,
    value_range_min    REAL    NOT NULL,
    value_range_max    REAL    NOT NULL,
    higher_is_better   INTEGER NOT NULL DEFAULT 1 CHECK (higher_is_better IN (0,1)),
    default_weight     REAL    NOT NULL,
    nonoffsettable     INTEGER NOT NULL DEFAULT 0 CHECK (nonoffsettable IN (0,1)),
    formula_family_id  TEXT    NOT NULL,
    created_utc        TEXT    NOT NULL,
    updated_utc        TEXT    NOT NULL,
    author_bostrom     TEXT,
    author_contract_id TEXT,
    UNIQUE (axis_name)
);

CREATE INDEX IF NOT EXISTS idx_responsibilityaxisdefinition_name
    ON responsibilityaxisdefinition (axis_name);

-------------------------------------------------------------------------------
-- 3. Portfolio diversity metric per shard / portfolio window
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS portfoliodiversitymetric (
    diversity_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    scopetype             TEXT    NOT NULL,  -- SHARD, PORTFOLIO, ACTOR
    scoperef              TEXT    NOT NULL,  -- shardid, portfolio id, actorid
    shardinstance_id      INTEGER,
    region                TEXT,
    lane                  TEXT,
    window_start_utc      TEXT    NOT NULL,
    window_end_utc        TEXT    NOT NULL,
    rportfoliodiversity   REAL    NOT NULL CHECK (rportfoliodiversity >= 0.0 AND rportfoliodiversity <= 1.0),
    issuing_system        TEXT    NOT NULL,
    author_bostrom        TEXT,
    author_contract_id    TEXT,
    created_utc           TEXT    NOT NULL,
    updated_utc           TEXT    NOT NULL,
    UNIQUE (scopetype, scoperef, window_start_utc, window_end_utc),
    FOREIGN KEY (shardinstance_id) REFERENCES shardinstance(id)
);

CREATE INDEX IF NOT EXISTS idx_portfolio_diversity_scope
    ON portfoliodiversitymetric (scopetype, scoperef, window_end_utc);

CREATE INDEX IF NOT EXISTS idx_portfolio_diversity_region_lane
    ON portfoliodiversitymetric (region, lane, window_end_utc);

-------------------------------------------------------------------------------
-- 4. Actor and shard bridges
-------------------------------------------------------------------------------

DROP VIEW IF EXISTS v_responsibility_shard_window;
CREATE VIEW v_responsibility_shard_window AS
SELECT
    r.responsibility_id,
    r.scopetype,
    r.scoperef,
    r.window_start_utc,
    r.window_end_utc,
    r.region,
    r.lane,
    r.rresponsibility,
    r.rpharma,
    r.rtoxicity,
    r.roverride,
    r.energy_duty_score,
    r.ecoimpact_score,
    r.lifeforce_stewardship,
    r.roh_stewardship,
    r.topology_stewardship,
    r.data_quality_score,
    r.restoration_duty_score,
    r.justice_duty_score,
    r.responsibility_index,
    r.kfactor,
    r.efactor,
    r.rfactor,
    r.vt_residual_est,
    r.issuing_system,
    r.author_bostrom,
    r.author_contract_id,
    r.created_utc,
    r.updated_utc
FROM responsibilitymetric AS r
WHERE r.scopetype = 'SHARD';

DROP VIEW IF EXISTS v_responsibility_actor_window;
CREATE VIEW v_responsibility_actor_window AS
SELECT
    r.responsibility_id,
    r.scopetype,
    r.scoperef AS actorid,
    r.window_start_utc,
    r.window_end_utc,
    r.region,
    r.lane,
    r.rresponsibility,
    r.energy_duty_score,
    r.ecoimpact_score,
    r.lifeforce_stewardship,
    r.roh_stewardship,
    r.topology_stewardship,
    r.data_quality_score,
    r.restoration_duty_score,
    r.justice_duty_score,
    r.responsibility_index,
    r.kfactor,
    r.efactor,
    r.rfactor,
    r.vt_residual_est,
    r.issuing_system,
    r.author_bostrom,
    r.author_contract_id,
    r.created_utc,
    r.updated_utc
FROM responsibilitymetric AS r
WHERE r.scopetype = 'ACTOR';

-------------------------------------------------------------------------------
-- 5. Responsibility-aware view over knowledgeecoscore (if attached)
-------------------------------------------------------------------------------

DROP VIEW IF EXISTS v_responsibility_from_knowledgeecoscore;
CREATE VIEW v_responsibility_from_knowledgeecoscore AS
SELECT
    k.id           AS knowledgeecoscore_id,
    k.scopetype    AS k_scopetype,
    k.scoperefid   AS k_scoperef,
    k.kfactor      AS kfactor,
    k.efactor      AS efactor,
    k.rfactor      AS rfactor,
    k.createdutc   AS k_created_utc
FROM knowledgeecoscore AS k;
