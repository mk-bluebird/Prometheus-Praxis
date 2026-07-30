-- filename Eco-Fort/db/db_planeweights_schema.sql
-- destination Eco-Fort/db/db_planeweights_schema.sql
-- repo-target https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose
--   Canonical, non-actuating registry for per-plane Lyapunov/KER weights used
--   across Eco-Fort / EcoNet corridor grammars:
--     - RiskVector + LyapunovWeights + LyapunovStep backbone for KER residuals.[4]
--     - NonOffsettablePlanes (carbon, biodiversity, neurorights) governance locks.[4]
--     - FOG predicates and blast-radius guards over multi-plane work.[6]
--   This schema upgrades the original planeweights table to:
--     - Track Tree-of-Life bundles and KER profiles explicitly.
--     - Bind weight rows to Phoenix Hex Anchors and Bostrom DID for provenance.[15]
--     - Preserve append-only governance via versioning and immutability flags.
--
--   All logic is non-actuating and safe for CI, replay, and governance tooling.

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. Core planeweights table
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS planeweights (
    -- Surrogate key for this weight row.
    planeweightsid      INTEGER PRIMARY KEY AUTOINCREMENT,

    -- Contract / constitution identifier (e.g., Phoenix Constitution shard).[4]
    contractid          TEXT    NOT NULL,

    -- Plane identifier (e.g., ENERGY, HYDRAULICS, CARBON, BIODIVERSITY, NEURORIGHTS).[4]
    planeid             TEXT    NOT NULL,

    -- Scalar weight w_j in Lyapunov residual V_t = Σ w_j * g_j(r_j),
    -- with V_t computed by shared residual engines across domains.[4]
    weight              REAL    NOT NULL
                                CHECK (weight >= 0.0),

    -- Non-offsettable plane flag: 1 if this plane is non-tradeable (e.g., CARBON,
    -- BIODIVERSITY, NEURORIGHTS) in governance logic; 0 otherwise.[4]
    nonoffsettable      INTEGER NOT NULL
                                CHECK (nonoffsettable IN (0, 1)),

    -- Soft corridor band for normalized risk r_j ∈ [0,1]; values below softband
    -- are treated as "safe/green" in KER and FOG predicates.[4][6]
    softband            REAL    NOT NULL
                                CHECK (softband >= 0.0 AND softband <= 1.0),

    -- Hard corridor band for r_j; values above hardband are treated as "red" and
    -- trigger block-stress guards and lane downgrades.[4][6]
    hardband            REAL    NOT NULL
                                CHECK (hardband >= 0.0 AND hardband <= 1.0),

    -- Maximum tolerated uncertainty for this plane in [0,1]; used by KER
    -- uncertainty planes and FOG predicates.[4]
    uncertaintycap      REAL    NOT NULL
                                CHECK (uncertaintycap >= 0.0 AND uncertaintycap <= 1.0),

    -- Tree-of-Life bundle identifier for weight sets that belong to a shared
    -- ecological grammar (e.g., TREEOFLIFEWEIGHTSPHXV1).[6]
    treeoflife_bundle   TEXT    NOT NULL,

    -- KER profile identifier (e.g., a governance particle or ALN shard name)
    -- that this weight row participates in.[4]
    kerprofileid        TEXT    NOT NULL,

    -- Version tag for this (contractid, planeid) weight row; used to keep
    -- append-only evolution of weights.[4]
    versiontag          TEXT    NOT NULL,

    -- Phoenix Hex Anchor reference for this weight row (evidence hex string),
    -- consistent with Eco-Fort phoenixhexanchor registry.[15]
    proofrefhex         TEXT    NOT NULL,

    -- Bostrom DID for provenance binding at the weight-row level.[4][15]
    signing_did         TEXT    NOT NULL,

    -- Governance flags
    --   active_flag: 1 if this row is active, 0 if retired but preserved.
    --   immutable_flag: 1 if this row MUST NOT be updated; evolution must use
    --   a new versiontag row instead, preserving append-only semantics.[4]
    active_flag         INTEGER NOT NULL
                                CHECK (active_flag IN (0, 1)),
    immutable_flag      INTEGER NOT NULL
                                CHECK (immutable_flag IN (0, 1)),

    -- Timestamps (UTC ISO-8601, stored as TEXT for portability).
    created_utc         TEXT    NOT NULL
                                DEFAULT (datetime('now')),
    updated_utc         TEXT    NOT NULL
                                DEFAULT (datetime('now')),

    -- Uniqueness constraints:
    --   - (contractid, planeid, versiontag) uniquely identifies a weight row.
    --   - (treeoflife_bundle, kerprofileid, planeid) enforces bundle/profile
    --     coherence for each plane.[4][6]
    UNIQUE (contractid, planeid, versiontag),
    UNIQUE (treeoflife_bundle, kerprofileid, planeid)
);

----------------------------------------------------------------------
-- 2. Indices for KER / Lyapunov queries
----------------------------------------------------------------------

CREATE INDEX IF NOT EXISTS idx_planeweights_contract_plane
    ON planeweights (contractid, planeid);

CREATE INDEX IF NOT EXISTS idx_planeweights_tree_bundle
    ON planeweights (treeoflife_bundle, kerprofileid);

CREATE INDEX IF NOT EXISTS idx_planeweights_nonoffsettable
    ON planeweights (nonoffsettable);

CREATE INDEX IF NOT EXISTS idx_planeweights_active_version
    ON planeweights (active_flag, versiontag);

----------------------------------------------------------------------
-- 3. Triggers enforcing append-only and DID / hex invariants
----------------------------------------------------------------------

-- Ensure updated_utc is refreshed on any UPDATE that is allowed.[4]
CREATE TRIGGER IF NOT EXISTS trg_planeweights_set_updated_utc
AFTER UPDATE ON planeweights
FOR EACH ROW
BEGIN
    UPDATE planeweights
    SET updated_utc = datetime('now')
    WHERE planeweightsid = NEW.planeweightsid;
END;

-- Prevent updates to immutable rows to keep Lyapunov/KER weights append-only.[4]
CREATE TRIGGER IF NOT EXISTS trg_planeweights_block_immutable_update
BEFORE UPDATE ON planeweights
FOR EACH ROW
BEGIN
    SELECT
        CASE
            WHEN OLD.immutable_flag = 1
            THEN RAISE(ABORT, 'planeweights: immutable row update prohibited; create new versiontag instead')
        END;
END;

-- Optional governance check: require signing_did to match project Bostrom DID
-- for Eco-Fort core contracts; this is consistent with ALN BostromBinding
-- constraints used in the Phoenix Hex Registry.[4][15]
CREATE TRIGGER IF NOT EXISTS trg_planeweights_bostrom_binding
BEFORE INSERT ON planeweights
FOR EACH ROW
BEGIN
    SELECT
        CASE
            WHEN NEW.signing_did NOT LIKE 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7%'
            THEN RAISE(ABORT, 'planeweights: signing_did must bind to primary Bostrom DID for Eco-Fort contracts')
        END;
END;
