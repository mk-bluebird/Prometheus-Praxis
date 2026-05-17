-- filename: eco_restoration_shard/db/dbdemurrage_and_keys.sql
-- destination: Eco-Fort/db/dbdemurrage_and_keys.sql
-- Purpose:
--   Answers DR-31, 33, 35 and supports 32, 34 by:
--   - Defining DemurrageEligibility view with windowed K/E/R and quarantine.
--   - Defining EcoWealthSnapshot hash-chain over shardinstance.
--   - Defining DIDKeyHistory shard and hybrid signature policy hints.
--   All math and tables reuse the existing EcoNet/Eco-Fort spine.[file:7][file:11]

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 31. DemurrageEligibility shard view
-------------------------------------------------------------------------------
-- Assumptions (all already present or implied in the spine):
--   steward:
--     steward_id TEXT PRIMARY KEY
--   eco_wealth_ledger:
--     entry_id INTEGER PRIMARY KEY
--     steward_id TEXT NOT NULL
--     shardid   INTEGER NOT NULL
--     amount    REAL NOT NULL          -- eco-wealth units, can be negative
--     timestamputc TEXT NOT NULL       -- ISO-8601
--   shardinstance:
--     shardid    INTEGER PRIMARY KEY
--     region     TEXT
--     lane       TEXT
--     kmetric    REAL
--     emetric    REAL
--     rmetric    REAL
--   dr9knowledgewindowsview:
--     shardid,
--     kshort, eshort, rshort,
--     kmedium, emedium, rmedium,
--     klong, elong, rlong
--   shardlanequarantine:
--     shardid INTEGER PRIMARY KEY
--     lanecode TEXT NOT NULL
--     lanequarantined INTEGER NOT NULL DEFAULT 0
--     quarantinereason TEXT
--
-- Additional helper: DemurrageConfig for window sizes and thresholds.[file:7]

CREATE TABLE IF NOT EXISTS demurrageconfig (
  config_id INTEGER PRIMARY KEY CHECK (config_id = 1),
  -- Rolling window in days for "active" eligibility (e.g., 30 days).
  activity_window_days INTEGER NOT NULL DEFAULT 30,
  -- Minimum net-wealth engagement (absolute sum of inflows+outflows).
  min_wealth_turnover REAL NOT NULL DEFAULT 1.0,
  -- Minimum long-window K/E and maximum R for "healthy" participation.
  k_long_min REAL NOT NULL DEFAULT 0.90,
  e_long_min REAL NOT NULL DEFAULT 0.90,
  r_long_max REAL NOT NULL DEFAULT 0.13
);

INSERT OR IGNORE INTO demurrageconfig (config_id) VALUES (1);

-- Helper view: aggregate steward K/E/R over a rolling window using shardinstance
-- and knowledge windows. This is non-actuating and uses existing windows.[file:7][file:11]

DROP VIEW IF EXISTS v_steward_ker_window;

CREATE VIEW v_steward_ker_window AS
WITH cfg AS (
  SELECT activity_window_days,
         k_long_min,
         e_long_min,
         r_long_max
  FROM demurrageconfig
  WHERE config_id = 1
),
recent_wealth AS (
  SELECT
    l.steward_id,
    l.shardid,
    l.amount,
    l.timestamputc
  FROM eco_wealth_ledger AS l,
       cfg
  WHERE l.timestamputc >= datetime('now', '-' || cfg.activity_window_days || ' days')
),
-- Join each ledger touch to its shard-level K/E/R and long windows.
joined AS (
  SELECT
    rw.steward_id,
    rw.shardid,
    rw.amount,
    rw.timestamputc,
    s.kmetric,
    s.emetric,
    s.rmetric,
    kw.klong,
    kw.elong,
    kw.rlong
  FROM recent_wealth AS rw
  JOIN shardinstance AS s
    ON s.shardid = rw.shardid
  LEFT JOIN dr9knowledgewindowsview AS kw
    ON kw.shardid = rw.shardid
)
SELECT
  j.steward_id,
  COUNT(DISTINCT j.shardid)        AS shard_count,
  COUNT(*)                         AS tx_count,
  SUM(ABS(j.amount))               AS wealth_turnover,
  AVG(j.kmetric)                   AS k_avg,
  AVG(j.emetric)                   AS e_avg,
  AVG(j.rmetric)                   AS r_avg,
  AVG(COALESCE(j.klong, j.kmetric)) AS k_long_avg,
  AVG(COALESCE(j.elong, j.emetric)) AS e_long_avg,
  AVG(COALESCE(j.rlong, j.rmetric)) AS r_long_avg
FROM joined AS j
GROUP BY j.steward_id;

-- Helper view: quarantine exemption at shard level. A quarantined shard
-- still counts as "active contribution" if the lane quarantine rules DR7
-- say MUSTNOTDECREASELANE and continuity improved.[file:7]

DROP VIEW IF EXISTS v_shard_quarantine_effective;

CREATE VIEW v_shard_quarantine_effective AS
SELECT
  s.shardid,
  q.lanequarantined,
  q.quarantinereason,
  CASE
    WHEN q.lanequarantined = 0 THEN 1
    -- Exempt quarantined shards from penalizing demurrage eligibility if
    -- quarantine is for safety tightening only (no downgrade).[file:7]
    WHEN q.lanequarantined = 1 AND
         q.quarantinereason LIKE '%safety-tightening%' THEN 1
    ELSE 0
  END AS counts_for_activity
FROM shardinstance AS s
LEFT JOIN shardlanequarantine AS q
  ON q.shardid = s.shardid;

-- DemurrageEligibility view:
--   status:
--     'ACTIVE'  : meets turnover + K/E/R + not all work on excluded shards
--     'IDLE'    : insufficient turnover but has some ledger activity
--     'INACTIVE': no ledger entries in window
--   quarantine_exempt:
--     1 if all touched shards are quarantine-exempt or normal.
--
-- This is the shard view to use in Paycomp when deciding if demurrage applies.[file:11]

DROP VIEW IF EXISTS demurrageeligibility;

CREATE VIEW demurrageeligibility AS
WITH cfg AS (
  SELECT
    activity_window_days,
    min_wealth_turnover,
    k_long_min,
    e_long_min,
    r_long_max
  FROM demurrageconfig
  WHERE config_id = 1
),
ker AS (
  SELECT * FROM v_steward_ker_window
),
touches AS (
  SELECT
    l.steward_id,
    l.shardid
  FROM eco_wealth_ledger AS l,
       cfg
  WHERE l.timestamputc >= datetime('now', '-' || cfg.activity_window_days || ' days')
  GROUP BY l.steward_id, l.shardid
),
touch_with_quarantine AS (
  SELECT
    t.steward_id,
    t.shardid,
    q.counts_for_activity
  FROM touches AS t
  LEFT JOIN v_shard_quarantine_effective AS q
    ON q.shardid = t.shardid
),
per_steward_quarantine AS (
  SELECT
    steward_id,
    MIN(counts_for_activity) AS all_shards_activity_flag
  FROM touch_with_quarantine
  GROUP BY steward_id
)
SELECT
  s.steward_id,
  COALESCE(k.shard_count, 0)        AS shard_count,
  COALESCE(k.tx_count, 0)           AS tx_count,
  COALESCE(k.wealth_turnover, 0.0)  AS wealth_turnover,
  COALESCE(k.k_long_avg, 0.0)       AS k_long_avg,
  COALESCE(k.e_long_avg, 0.0)       AS e_long_avg,
  COALESCE(k.r_long_avg, 0.0)       AS r_long_avg,
  COALESCE(q.all_shards_activity_flag, 0) AS quarantine_exempt,
  CASE
    WHEN k.steward_id IS NULL THEN 'INACTIVE'
    WHEN k.wealth_turnover < cfg.min_wealth_turnover THEN 'IDLE'
    WHEN k.k_long_avg >= cfg.k_long_min
         AND k.e_long_avg >= cfg.e_long_min
         AND k.r_long_avg <= cfg.r_long_max
         AND COALESCE(q.all_shards_activity_flag, 0) = 1
      THEN 'ACTIVE'
    ELSE 'IDLE'
  END AS status
FROM (SELECT DISTINCT steward_id FROM eco_wealth_ledger) AS s
LEFT JOIN ker AS k
  ON k.steward_id = s.steward_id
LEFT JOIN per_steward_quarantine AS q
  ON q.steward_id = s.steward_id,
cfg;

-- Sample queries for DR-31:
--   Find all active stewards:
--     SELECT * FROM demurrageeligibility WHERE status = 'ACTIVE';
--   Count idle vs active:
--     SELECT status, COUNT(*) FROM demurrageeligibility GROUP BY status;
--   Stewards that are active but only due to quarantine exemption:
--     SELECT * FROM demurrageeligibility
--      WHERE status = 'ACTIVE' AND quarantine_exempt = 1;


-------------------------------------------------------------------------------
-- 32. Deterministic replay epsilon and fixed-point commitments (concept)
-------------------------------------------------------------------------------
-- This section is a formal pattern, not executable code, to avoid floating-point
-- slashing ambiguity in CI-staked replay.[file:11]
--
-- Pattern (to be implemented in Rust, not here):
--
--   - All K/E/R and residual calculations used for slashing are mapped into a
--     fixed-point integer domain with a contract-bound scale factor S
--     (e.g., S = 10^6 for 1e-6 resolution).
--   - CI and agents commit not to raw floats but to:
--       ker_fp = {
--         k_fp = round(K * S),
--         e_fp = round(E * S),
--         r_fp = round(R * S),
--         vt_fp = round(Vt * S)
--       }
--   - The eco_wealth_ledger and virtaupgradeledger store these integer
--     commitments in separate columns:
--       k_fp, e_fp, r_fp, vt_fp INTEGER NOT NULL.
--   - Slashing replay compares integer tuples exactly; "within ε" is then
--     defined as matching the integer commitment, eliminating cross-arch
--     float variance.
--
-- Risk near boundaries is handled by corridor-buffer:
--   - Thresholds (e.g., K >= 0.90) are encoded as integers with a safety
--     margin, e.g., K_min_fp = ceil(0.9005 * S).
--   - CI uses strictly stronger conditions than governance (one extra ULP),
--     so legitimate boundary-compliant runs are never slashed.[file:11]
--
-- Fallback rule:
--   - If a replay recomputes floats that differ but still quantize to the same
--     integer tuple, the replay is accepted.
--   - If integer tuples differ, the CI stake is at risk and an on-chain appeal
--     must show an ALN-encoded kernel change or a contract version bump,
--     not a float rounding argument.


-------------------------------------------------------------------------------
-- 33. PlaneWeightsHistory snapshot hash-chain linkage
-------------------------------------------------------------------------------
-- We mirror the artifact-provenance pattern (DR6) for eco-wealth snapshots.[file:7]
-- Goal: allow verifying that an EcoWealthSnapshot row is consistent with the
-- exact shardinstance set used, without rerunning all K/E/R.

-- EcoWealthSnapshot:
--   One row per Paycomp snapshot window and plane weights contract.

CREATE TABLE IF NOT EXISTS ecowealthsnapshot (
  snapshot_id INTEGER PRIMARY KEY AUTOINCREMENT,
  window_id   TEXT NOT NULL,       -- logical KerWindow identifier
  contractid  INTEGER NOT NULL,    -- planeweightscontract.contractid
  ts_start    TEXT NOT NULL,
  ts_end      TEXT NOT NULL,
  steward_count INTEGER NOT NULL,
  total_wealth REAL NOT NULL,
  vt_max       REAL NOT NULL,
  -- Hash over contributing shardinstance rows, see ecowealthshardsnapshot below.
  shard_chain_hash BLOB NOT NULL,
  -- Hash over plane weights contract rows at this snapshot.
  planeweights_hash BLOB NOT NULL,
  -- Optional governance evidence hex / DID for signing.
  evidencehex TEXT,
  signingdid  TEXT,
  timestamputc TEXT NOT NULL,
  FOREIGN KEY (contractid)
    REFERENCES planeweightscontract(contractid)
    ON DELETE CASCADE
);

-- EcoWealthShardSnapshot:
--   Hash-chain over the ordered set of shardinstance rows included in a snapshot.
--   This allows later verification that a given shard set was used.[file:7]

CREATE TABLE IF NOT EXISTS ecowealthshardsnapshot (
  -- Logical grouping key:
  snapshot_id INTEGER NOT NULL,
  -- Sequential index in canonical order (shardid ascending, then ts ascending).
  ordinal     INTEGER NOT NULL,
  shardid     INTEGER NOT NULL,
  ts          TEXT NOT NULL,
  kmetric     REAL NOT NULL,
  emetric     REAL NOT NULL,
  rmetric     REAL NOT NULL,
  vt          REAL NOT NULL,
  -- Forward hash linking, similar to DR6 provenance chain:
  --   h_0 = H("ecowealthshard" || snapshot_id || ordinal || shardid || ts || k || e || r || vt)
  --   h_n = H(h_{n-1} || snapshot_id || ordinal || shardid || ts || k || e || r || vt)
  h_prev      BLOB,
  h_curr      BLOB NOT NULL,
  PRIMARY KEY (snapshot_id, ordinal),
  FOREIGN KEY (snapshot_id)
    REFERENCES ecowealthsnapshot(snapshot_id)
    ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_ecowealthshardsnapshot_shard
  ON ecowealthshardsnapshot (shardid, ts);

-- Usage:
--   - When Paycomp builds a snapshot for window_id W:
--       1) SELECT all shardinstance rows participating in the KerWindow W,
--          ordered by (shardid, ts).
--       2) Construct the hash-chain in application code using a frozen hash
--          (must not be any blacklisted hash; use an allowed, audited hash).
--       3) Insert rows into ecowealthshardsnapshot with h_prev/h_curr.
--       4) Set ecowealthsnapshot.shard_chain_hash = h_curr of the last row.
--       5) Compute planeweights_hash as a Merkle-like reduction over
--          planeweightsplane rows for this contractid and store in snapshot.

-- Later validation:
--   - Given snapshot_id:
--       * Read ecowealthsnapshot.shard_chain_hash and contractid.
--       * Read ecowealthshardsnapshot rows, recompute chain, compare final hash.
--       * Independently query planeweightsplane for contractid at that time,
--         recompute planeweights_hash and compare.
--   - No need to access original eco_wealth_ledger; the K/E/R materialization
--     is attested via the hash-chain.[file:7][file:11]


-------------------------------------------------------------------------------
-- 34. Zero-knowledge proofs for off-chain eco-wealth (concept)
-------------------------------------------------------------------------------
-- This section is descriptive and does not add SQL to avoid premature coupling.
--
-- Feasibility for a Groth16 proof of a single KerWindow:
--   - Statement:
--       "Given window input shardinstance rows with (k, e, r, vt) and a fixed
--        planeweightscontract, the published (K_window, E_window, R_window, Vt_window)
--        are computed according to frozen Lyapunov/KER grammar DR1/DR9."[file:7][file:11]
--   - Circuit:
--       * Inputs: n <= N_max per-window shard rows, each with normalized r_j,
--         and plane weights w_j from planeweightsplane.
--       * Constraints:
--           Vt = Σ_j w_j * r_j^2
--           R  = max_j r_j
--           E  = 1 - R
--         plus consistency checks against corridor bands.
--   - Overhead:
--       * For modest N_max (e.g., 32–128 risk coordinates per window),
--         a Groth16 circuit would require O(10^4–10^5) constraints.
--       * Proof generation in a modern Rust SNARK library is on the order of
--         tens to hundreds of milliseconds per window on commodity hardware.
--       * Verification is constant-time (few pairings) and easily in the
--         sub-millisecond range.[file:11]
--
-- Practical stance:
--   - Use ZK proofs for *aggregated* windows (KerWindow per region/plane),
--     not for every raw telemetry row; IoT volumes would make per-row ZK
--     intractable.[file:11]
--   - Keep Groth16 in a RESEARCH lane for now, with off-chain verifiers,
--     while CI continues to rely on deterministic replay plus hash-chains
--     (see ecowealthsnapshot tables).
--   - If future performance profiling within EcoNet shows sustainable
--     overhead, a dedicated "KerWindowProofShard" ALN/SQL pair can be added
--     to track proof artifacts alongside snapshots.


-------------------------------------------------------------------------------
-- 35. DIDKeyHistory shard and hybrid signature policy
-------------------------------------------------------------------------------
-- This shard tracks Ed25519 and PQ keys over time, with explicit rotation
-- semantics and policy flags for eco_wealth_ledger and related tables.[file:11]

CREATE TABLE IF NOT EXISTS didkeyhistory (
  didkey_id    INTEGER PRIMARY KEY AUTOINCREMENT,
  did          TEXT NOT NULL,      -- e.g., bostrom18sd2... or other DIDs
  key_id       TEXT NOT NULL,      -- logical key label, e.g., "main", "payout"
  alg          TEXT NOT NULL,      -- 'Ed25519', 'PQ-ALG-V1', etc.
  public_key   BLOB NOT NULL,
  valid_from   TEXT NOT NULL,
  valid_until  TEXT,               -- NULL means "until revoked/rotated"
  revoked      INTEGER NOT NULL DEFAULT 0 CHECK (revoked IN (0,1)),
  -- Governance flags:
  require_pq   INTEGER NOT NULL DEFAULT 0 CHECK (require_pq IN (0,1)),
  allow_legacy INTEGER NOT NULL DEFAULT 1 CHECK (allow_legacy IN (0,1)),
  -- Hash of an ALN governance particle describing the key policy.
  policy_hash  BLOB,
  UNIQUE (did, key_id, alg, valid_from)
);

CREATE INDEX IF NOT EXISTS idx_didkeyhistory_did_alg
  ON didkeyhistory (did, alg, valid_from);

-- DIDKeyPolicy view: latest active keys per DID and purpose, with policy.[file:11]

DROP VIEW IF EXISTS didkeypolicy;

CREATE VIEW didkeypolicy AS
WITH ranked AS (
  SELECT
    d.*,
    ROW_NUMBER() OVER (
      PARTITION BY d.did, d.key_id, d.alg
      ORDER BY d.valid_from DESC
    ) AS rn
  FROM didkeyhistory AS d
  WHERE (d.valid_until IS NULL OR d.valid_until >= datetime('now'))
    AND d.revoked = 0
)
SELECT
  didkey_id,
  did,
  key_id,
  alg,
  public_key,
  valid_from,
  valid_until,
  require_pq,
  allow_legacy,
  policy_hash
FROM ranked
WHERE rn = 1;

-- Hybrid signature verification algorithm (conceptual, to be implemented
-- in the ecosafety spine / eco_wealth_ledger code, not SQL):
--
--   For each eco_wealth_ledger entry or ecowealthsnapshot row:
--     - Fields: signingdid, sig_primary, sig_pq (optional), sig_alg_primary,
--               sig_alg_pq.
--     - Lookup didkeypolicy rows for (signingdid, key_id='payout', alg='Ed25519')
--       and (alg='PQ-ALG-V1').
--
--   Policy rules:
--     1) PROD lane, long-horizon eco-wealth (snapshots, planeweight changes):
--          - require_pq = 1 implies:
--              * sig_pq MUST be present and valid under latest PQ key.
--              * If allow_legacy = 1 and sig_primary present, Ed25519 is
--                checked but treated as auxiliary.
--          - If no PQ key exists for DID or sig_pq missing → reject.[file:11]
--     2) RESEARCH or EXPPROD lanes:
--          - If allow_legacy = 1:
--              * Accept Ed25519 signatures alone, provided they are valid
--                and key is not revoked.
--          - If require_pq = 1 is accidentally set for a research DID,
--            CI should fail lane admission rather than override policy.
--     3) Rotation:
--          - To rotate keys, insert a new didkeyhistory row with later
--            valid_from; optionally set valid_until on the old row.
--          - Verifiers always use didkeypolicy (rn = 1) as the active key.
--          - Historical records verify against keys whose [valid_from, valid_until]
--            window covered their timestamputc; this requires a lookup that
--            ignores rn and filters by timestamp in audit mode.
--
--   This keeps signature requirements data-driven and discoverable through
--   didkeyhistory, while allowing a gradual migration from Ed25519-only to
--   hybrid PQ + Ed25519 for eco-wealth critical paths.[file:11]
