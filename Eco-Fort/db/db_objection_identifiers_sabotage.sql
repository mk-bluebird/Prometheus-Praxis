-- filename: db_objection_identifiers_sabotage.sql
-- destination: Eco-Fort/db/db_objection_identifiers_sabotage.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

--------------------------------------------------------------------
-- 39. Fake eco-restoration claims by AI agents
--    - Require at least one hardware-secured oracle signature
--    - Add decentralized eco-jury with random audits and slashing
--------------------------------------------------------------------

-- Core eco-evidence aggregation message table.
CREATE TABLE IF NOT EXISTS eco_evidence_aggregate (
    evidence_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    region               TEXT NOT NULL,
    lane                 TEXT NOT NULL,             -- RESEARCH / EXPPROD / PROD
    tx_hash              TEXT NOT NULL,
    oracle_bundle_hash   TEXT NOT NULL,             -- hash over all oracle attestations
    eco_payload_hash     TEXT NOT NULL,             -- hash over eco evidence payload
    eco_plane            TEXT NOT NULL,             -- e.g. "HYDROLOGY", "BIOTA", "CARBON"
    created_utc          TEXT NOT NULL,
    author_bostrom       TEXT NOT NULL,
    author_contract      TEXT NOT NULL,
    UNIQUE(tx_hash, eco_plane)
);

-- Hardware-secured oracle signatures; at least one must be present.
CREATE TABLE IF NOT EXISTS eco_evidence_oracle_sig (
    oracle_sig_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    evidence_id          INTEGER NOT NULL REFERENCES eco_evidence_aggregate(evidence_id)
                           ON DELETE CASCADE,
    oracle_id            TEXT NOT NULL,          -- DID of oracle, e.g. sat provider
    oracle_class         TEXT NOT NULL,          -- "HSM_SATELLITE", "HSM_LIDAR", etc.
    attested_api_id      TEXT NOT NULL,          -- attested API name/version
    signature_hex        TEXT NOT NULL,          -- signature over eco_payload_hash
    hardware_attested    INTEGER NOT NULL CHECK (hardware_attested IN (0,1)),
    created_utc          TEXT NOT NULL
);

-- Enforces "at least one hardware-secured signature" as a governance surface:
-- v_eco_evidence_secure filters to evidence with >= 1 hardware_attested = 1.
CREATE VIEW IF NOT EXISTS v_eco_evidence_secure AS
SELECT
    e.evidence_id,
    e.region,
    e.lane,
    e.tx_hash,
    e.oracle_bundle_hash,
    e.eco_payload_hash,
    e.eco_plane,
    e.created_utc,
    e.author_bostrom,
    e.author_contract,
    COUNT(*)             AS hardware_sig_count
FROM eco_evidence_aggregate AS e
JOIN eco_evidence_oracle_sig AS s
    ON s.evidence_id = e.evidence_id
   AND s.hardware_attested = 1
GROUP BY
    e.evidence_id,
    e.region,
    e.lane,
    e.tx_hash,
    e.oracle_bundle_hash,
    e.eco_payload_hash,
    e.eco_plane,
    e.created_utc,
    e.author_bostrom,
    e.author_contract
HAVING COUNT(*) >= 1;

-- Decentralized eco‑jury registry; jurors are validators opted into eco audits.
CREATE TABLE IF NOT EXISTS eco_jury_member (
    jury_member_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    validator_id         TEXT NOT NULL,      -- FK into validator registry in main spine
    region               TEXT NOT NULL,
    active               INTEGER NOT NULL CHECK (active IN (0,1)),
    stake_weight         REAL NOT NULL,      -- normalized stake fraction 0–1
    joined_utc           TEXT NOT NULL,
    UNIQUE(validator_id, region)
);

-- Random eco‑jury audits over evidence objects.
CREATE TABLE IF NOT EXISTS eco_jury_audit (
    audit_id             INTEGER PRIMARY KEY AUTOINCREMENT,
    evidence_id          INTEGER NOT NULL REFERENCES eco_evidence_aggregate(evidence_id)
                           ON DELETE CASCADE,
    region               TEXT NOT NULL,
    lane                 TEXT NOT NULL,
    audit_window_start   TEXT NOT NULL,
    audit_window_end     TEXT NOT NULL,
    -- who scheduled this (e.g. DRAND beacon index or scheduler contract id)
    scheduler_ref        TEXT NOT NULL,
    -- final verdict over this evidence
    verdict              TEXT NOT NULL CHECK (verdict IN (
                               'PENDING',
                               'CONFIRMED',
                               'FAKE_DATA',
                               'INSUFFICIENT_EVIDENCE'
                           )),
    slashing_applied     INTEGER NOT NULL DEFAULT 0 CHECK (slashing_applied IN (0,1)),
    created_utc          TEXT NOT NULL,
    updated_utc          TEXT NOT NULL
);

-- Eco‑jury votes; each vote is a non‑actuating record for governance review.
CREATE TABLE IF NOT EXISTS eco_jury_vote (
    jury_vote_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    audit_id             INTEGER NOT NULL REFERENCES eco_jury_audit(audit_id)
                           ON DELETE CASCADE,
    jury_member_id       INTEGER NOT NULL REFERENCES eco_jury_member(jury_member_id)
                           ON DELETE CASCADE,
    vote                 TEXT NOT NULL CHECK (vote IN (
                               'CONFIRMED',
                               'FAKE_DATA',
                               'ABSTAIN'
                           )),
    justification        TEXT NOT NULL,      -- human-readable / hash of evidence
    created_utc          TEXT NOT NULL,
    UNIQUE(audit_id, jury_member_id)
);

-- Slashing events for validators hosting fake eco data.
CREATE TABLE IF NOT EXISTS eco_validator_slash (
    slash_id             INTEGER PRIMARY KEY AUTOINCREMENT,
    validator_id         TEXT NOT NULL,
    evidence_id          INTEGER NOT NULL REFERENCES eco_evidence_aggregate(evidence_id)
                           ON DELETE CASCADE,
    audit_id             INTEGER NOT NULL REFERENCES eco_jury_audit(audit_id)
                           ON DELETE CASCADE,
    slash_reason         TEXT NOT NULL,      -- e.g. "FAKE_ECO_EVIDENCE"
    slash_fraction       REAL NOT NULL,      -- 0–1 fraction of stake
    applied_utc          TEXT NOT NULL,
    author_contract      TEXT NOT NULL,      -- governance contract that applied slash
    author_bostrom       TEXT NOT NULL
);

-- View: audits that concluded FAKE_DATA with majority vote; used to drive slashing logic.
CREATE VIEW IF NOT EXISTS v_eco_jury_fake_majority AS
WITH vote_counts AS (
    SELECT
        a.audit_id,
        SUM(CASE WHEN v.vote = 'CONFIRMED'   THEN 1 ELSE 0 END) AS confirmed_count,
        SUM(CASE WHEN v.vote = 'FAKE_DATA'   THEN 1 ELSE 0 END) AS fake_count,
        SUM(CASE WHEN v.vote = 'ABSTAIN'     THEN 1 ELSE 0 END) AS abstain_count,
        COUNT(*) AS total_votes
    FROM eco_jury_audit AS a
    JOIN eco_jury_vote  AS v
      ON v.audit_id = a.audit_id
    GROUP BY a.audit_id
)
SELECT
    a.audit_id,
    a.evidence_id,
    a.region,
    a.lane,
    a.audit_window_start,
    a.audit_window_end,
    a.verdict,
    vc.fake_count,
    vc.confirmed_count,
    vc.abstain_count,
    vc.total_votes
FROM eco_jury_audit AS a
JOIN vote_counts    AS vc
  ON vc.audit_id = a.audit_id
WHERE a.verdict = 'FAKE_DATA'
  AND vc.fake_count > vc.confirmed_count;

--------------------------------------------------------------------
-- 40. Sabotage via blacklisted content in research contributions
--    - Add blacklist_challenge_period and flag/quarantine workflow
--------------------------------------------------------------------

-- Research artifacts contributed by agents / hosts.
CREATE TABLE IF NOT EXISTS research_artifact (
    artifact_id              INTEGER PRIMARY KEY AUTOINCREMENT,
    region                   TEXT NOT NULL,
    lane                     TEXT NOT NULL,      -- RESEARCH, EXPPROD, PROD
    artifact_hash            TEXT NOT NULL,      -- hash over artifact content
    artifact_uri             TEXT NOT NULL,      -- pointer into repo/storage
    host_bostrom             TEXT NOT NULL,      -- host identity
    r_axis_initial           REAL NOT NULL,      -- initial R-axis value at submission
    blacklist_challenge_period_hours INTEGER NOT NULL,
    submitted_utc            TEXT NOT NULL,
    challenge_deadline_utc   TEXT NOT NULL,
    quarantined              INTEGER NOT NULL DEFAULT 0 CHECK (quarantined IN (0,1)),
    r_axis_frozen            INTEGER NOT NULL DEFAULT 1 CHECK (r_axis_frozen IN (0,1)),
    dao_case_id              TEXT,              -- linkage into DAO court system
    UNIQUE(artifact_hash, region)
);

-- Flags raised by validators during the challenge period.
CREATE TABLE IF NOT EXISTS research_artifact_flag (
    flag_id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    artifact_id              INTEGER NOT NULL REFERENCES research_artifact(artifact_id)
                               ON DELETE CASCADE,
    validator_id             TEXT NOT NULL,
    flag_reason              TEXT NOT NULL,   -- short code, e.g. "BLACKLIST_PATTERN"
    description              TEXT NOT NULL,   -- human-readable explanation
    created_utc              TEXT NOT NULL,
    resolved                 INTEGER NOT NULL DEFAULT 0 CHECK (resolved IN (0,1)),
    resolution               TEXT,            -- "UPHELD", "REJECTED"
    resolution_utc           TEXT
);

-- DAO court rulings on flagged artifacts.
CREATE TABLE IF NOT EXISTS research_artifact_dao_ruling (
    ruling_id                INTEGER PRIMARY KEY AUTOINCREMENT,
    artifact_id              INTEGER NOT NULL REFERENCES research_artifact(artifact_id)
                               ON DELETE CASCADE,
    dao_case_id              TEXT NOT NULL,
    ruling                   TEXT NOT NULL CHECK (ruling IN (
                               'MALICIOUS',
                               'BENIGN',
                               'INCONCLUSIVE'
                           )),
    sabotage_attempt_credit  INTEGER NOT NULL DEFAULT 0 CHECK (sabotage_attempt_credit IN (0,1)),
    host_penalized           INTEGER NOT NULL DEFAULT 0 CHECK (host_penalized IN (0,1)),
    flagger_slashed          INTEGER NOT NULL DEFAULT 0 CHECK (flagger_slashed IN (0,1)),
    created_utc              TEXT NOT NULL
);

-- Host sabotage attempt credits; reputation boost when falsely accused.
CREATE TABLE IF NOT EXISTS host_sabotage_credit (
    credit_id                INTEGER PRIMARY KEY AUTOINCREMENT,
    host_bostrom             TEXT NOT NULL,
    artifact_id              INTEGER NOT NULL REFERENCES research_artifact(artifact_id)
                               ON DELETE CASCADE,
    credit_reason            TEXT NOT NULL,   -- e.g. "FLAG_REJECTED"
    created_utc              TEXT NOT NULL
);

-- Validator slashing events specifically for bad flags (false accusations).
CREATE TABLE IF NOT EXISTS validator_flag_slash (
    slash_id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    validator_id             TEXT NOT NULL,
    artifact_id              INTEGER NOT NULL REFERENCES research_artifact(artifact_id)
                               ON DELETE CASCADE,
    slash_reason             TEXT NOT NULL,   -- e.g. "FRIVOLOUS_BLACKLIST_FLAG"
    slash_fraction           REAL NOT NULL,   -- 0–1
    applied_utc              TEXT NOT NULL,
    author_contract          TEXT NOT NULL,
    author_bostrom           TEXT NOT NULL
);

-- View: artifacts currently under blacklist challenge (challenge window open).
CREATE VIEW IF NOT EXISTS v_research_artifact_under_challenge AS
SELECT
    a.artifact_id,
    a.region,
    a.lane,
    a.artifact_hash,
    a.host_bostrom,
    a.blacklist_challenge_period_hours,
    a.submitted_utc,
    a.challenge_deadline_utc,
    a.quarantined,
    a.r_axis_frozen
FROM research_artifact AS a
WHERE a.challenge_deadline_utc > datetime('now');

-- View: artifacts quarantined due to flags but with unresolved DAO ruling.
CREATE VIEW IF NOT EXISTS v_research_artifact_quarantined AS
SELECT
    a.artifact_id,
    a.region,
    a.lane,
    a.artifact_hash,
    a.host_bostrom,
    a.submitted_utc,
    a.challenge_deadline_utc,
    a.quarantined,
    a.r_axis_frozen
FROM research_artifact AS a
WHERE a.quarantined = 1
  AND a.dao_case_id IS NULL;

--------------------------------------------------------------------
-- 41. BCI spoofing to bypass eco-intent attestation
--    - DRAND-based nonce injection for BCIChallengeSignature
--------------------------------------------------------------------

-- DRAND beacon snapshots replicated into the governance spine.
CREATE TABLE IF NOT EXISTS drand_beacon (
    beacon_round        INTEGER PRIMARY KEY,   -- DRAND round number
    beacon_hash         TEXT NOT NULL,         -- DRAND randomness hash
    randomness_hex      TEXT NOT NULL,         -- full randomness
    published_utc       TEXT NOT NULL
);

-- BCI challenge objects as seen by the bostrom-signer.
CREATE TABLE IF NOT EXISTS bci_challenge (
    challenge_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    tx_hash             TEXT NOT NULL,
    region              TEXT NOT NULL,
    lane                TEXT NOT NULL,
    host_bostrom        TEXT NOT NULL,
    eco_summary         TEXT NOT NULL,        -- e.g. "CO2 X kg, cost Y, eco Z"
    visual_cue          TEXT NOT NULL,        -- exact string flashed to host
    drand_round         INTEGER NOT NULL REFERENCES drand_beacon(beacon_round)
                           ON DELETE RESTRICT,
    drand_nonce_hex     TEXT NOT NULL,        -- nonce derived from randomness
    created_utc         TEXT NOT NULL,
    UNIQUE(tx_hash, host_bostrom)
);

-- BCI signatures bound to DRAND nonces.
CREATE TABLE IF NOT EXISTS bci_challenge_signature (
    signature_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    challenge_id        INTEGER NOT NULL REFERENCES bci_challenge(challenge_id)
                           ON DELETE CASCADE,
    host_bci_device_id  TEXT NOT NULL,         -- anonymised device identifier
    signature_hex       TEXT NOT NULL,         -- signature over (tx_hash || visual_cue || nonce)
    created_utc         TEXT NOT NULL
);

-- View: canonical envelope used for BCIChallengeSignature verification.
CREATE VIEW IF NOT EXISTS v_bci_challenge_envelope AS
SELECT
    c.challenge_id,
    c.tx_hash,
    c.region,
    c.lane,
    c.host_bostrom,
    c.visual_cue,
    c.drand_round,
    c.drand_nonce_hex,
    b.randomness_hex,
    c.created_utc
FROM bci_challenge AS c
JOIN drand_beacon  AS b
  ON b.beacon_round = c.drand_round;

--------------------------------------------------------------------
-- 42. Dilution of eco-wealth through over-minting
--    - Governance-frozen eco-credit circuit with supermajority + timelock
--------------------------------------------------------------------

-- Registry of eco-credit circuits and their verification keys.
CREATE TABLE IF NOT EXISTS ecocredit_circuit_registry (
    circuit_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    circuit_name        TEXT NOT NULL,        -- e.g. "EcoCreditMAR2026v1"
    version_tag         TEXT NOT NULL,        -- semantic version tag
    region              TEXT NOT NULL,
    vk_hash             TEXT NOT NULL,        -- hash of verification key
    params_hash         TEXT NOT NULL,        -- hash of circuit parameters
    frozen              INTEGER NOT NULL CHECK (frozen IN (0,1)),
    active              INTEGER NOT NULL CHECK (active IN (0,1)),
    governance_contract TEXT NOT NULL,        -- contract id governing upgrades
    created_utc         TEXT NOT NULL,
    updated_utc         TEXT NOT NULL,
    UNIQUE(circuit_name, version_tag, region)
);

-- Circuit upgrade proposals with dual supermajority + timelock.
CREATE TABLE IF NOT EXISTS ecocredit_circuit_upgrade_proposal (
    proposal_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    circuit_id          INTEGER NOT NULL REFERENCES ecocredit_circuit_registry(circuit_id)
                           ON DELETE CASCADE,
    new_vk_hash         TEXT NOT NULL,
    new_params_hash     TEXT NOT NULL,
    proposer_bostrom    TEXT NOT NULL,
    proposal_utc        TEXT NOT NULL,
    timelock_days       INTEGER NOT NULL DEFAULT 30,
    activation_earliest TEXT NOT NULL,        -- proposal_utc + timelock_days
    stake_supermajority_threshold REAL NOT NULL DEFAULT 0.6667,
    oracle_supermajority_threshold REAL NOT NULL DEFAULT 0.6667,
    stake_yes_weight    REAL NOT NULL DEFAULT 0.0,
    stake_no_weight     REAL NOT NULL DEFAULT 0.0,
    oracle_yes_weight   REAL NOT NULL DEFAULT 0.0,
    oracle_no_weight    REAL NOT NULL DEFAULT 0.0,
    finalized           INTEGER NOT NULL DEFAULT 0 CHECK (finalized IN (0,1)),
    approved            INTEGER NOT NULL DEFAULT 0 CHECK (approved IN (0,1))
);

-- Stake-weighted validator votes on circuit upgrades.
CREATE TABLE IF NOT EXISTS ecocredit_circuit_upgrade_stake_vote (
    stake_vote_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    proposal_id         INTEGER NOT NULL REFERENCES ecocredit_circuit_upgrade_proposal(proposal_id)
                           ON DELETE CASCADE,
    validator_id        TEXT NOT NULL,
    stake_fraction      REAL NOT NULL,     -- normalized stake 0–1
    vote                TEXT NOT NULL CHECK (vote IN ('YES','NO')),
    created_utc         TEXT NOT NULL,
    UNIQUE(proposal_id, validator_id)
);

-- Eco-oracle votes on circuit upgrades.
CREATE TABLE IF NOT EXISTS ecocredit_circuit_upgrade_oracle_vote (
    oracle_vote_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    proposal_id         INTEGER NOT NULL REFERENCES ecocredit_circuit_upgrade_proposal(proposal_id)
                           ON DELETE CASCADE,
    oracle_id           TEXT NOT NULL,
    oracle_weight       REAL NOT NULL,    -- normalized oracle weight 0–1
    vote                TEXT NOT NULL CHECK (vote IN ('YES','NO')),
    created_utc         TEXT NOT NULL,
    UNIQUE(proposal_id, oracle_id)
);

-- View: aggregated vote weights per proposal to check supermajority.
CREATE VIEW IF NOT EXISTS v_ecocredit_circuit_upgrade_votes AS
SELECT
    p.proposal_id,
    p.circuit_id,
    p.new_vk_hash,
    p.new_params_hash,
    p.proposal_utc,
    p.timelock_days,
    p.activation_earliest,
    COALESCE(SUM(CASE WHEN sv.vote = 'YES' THEN sv.stake_fraction ELSE 0 END), 0.0)
        AS stake_yes_weight,
    COALESCE(SUM(CASE WHEN sv.vote = 'NO'  THEN sv.stake_fraction ELSE 0 END), 0.0)
        AS stake_no_weight,
    COALESCE(SUM(CASE WHEN ov.vote = 'YES' THEN ov.oracle_weight ELSE 0 END), 0.0)
        AS oracle_yes_weight,
    COALESCE(SUM(CASE WHEN ov.vote = 'NO'  THEN ov.oracle_weight ELSE 0 END), 0.0)
        AS oracle_no_weight,
    p.stake_supermajority_threshold,
    p.oracle_supermajority_threshold,
    p.finalized,
    p.approved
FROM ecocredit_circuit_upgrade_proposal AS p
LEFT JOIN ecocredit_circuit_upgrade_stake_vote   AS sv
       ON sv.proposal_id = p.proposal_id
LEFT JOIN ecocredit_circuit_upgrade_oracle_vote  AS ov
       ON ov.proposal_id = p.proposal_id
GROUP BY
    p.proposal_id,
    p.circuit_id,
    p.new_vk_hash,
    p.new_params_hash,
    p.proposal_utc,
    p.timelock_days,
    p.activation_earliest,
    p.stake_supermajority_threshold,
    p.oracle_supermajority_threshold,
    p.finalized,
    p.approved;

--------------------------------------------------------------------
-- 2. Objection identifiers index
--    Guides agents to the schemas above for sabotage detection
--------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS objection_identifier_index (
    objection_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    label               TEXT NOT NULL,      -- e.g. "FakeEcoRestorationClaimsAI"
    question_number     INTEGER NOT NULL,   -- 39, 40, 41, 42
    sqlfile             TEXT NOT NULL,      -- db_objection_identifiers_sabotage.sql
    destination         TEXT NOT NULL,      -- Eco-Fort/db/db_objection_identifiers_sabotage.sql
    repo_target         TEXT NOT NULL,      -- github.com/mk-bluebird/eco_restoration_shard
    description         TEXT NOT NULL,      -- short explainer
    created_utc         TEXT NOT NULL
);

INSERT OR IGNORE INTO objection_identifier_index (
    label,
    question_number,
    sqlfile,
    destination,
    repo_target,
    description,
    created_utc
) VALUES
    (
        'FakeEcoRestorationClaimsAI',
        39,
        'db_objection_identifiers_sabotage.sql',
        'Eco-Fort/db/db_objection_identifiers_sabotage.sql',
        'github.com/mk-bluebird/eco_restoration_shard',
        'Hardware-secured eco evidence, eco-jury random audits, and validator slashing for fake eco reports.',
        datetime('now')
    ),
    (
        'BlacklistSabotageResearchArtifacts',
        40,
        'db_objection_identifiers_sabotage.sql',
        'Eco-Fort/db/db_objection_identifiers_sabotage.sql',
        'github.com/mk-bluebird/eco_restoration_shard',
        'Blacklist challenge period with quarantine, DAO court rulings, host sabotage credits, and flagger slashing.',
        datetime('now')
    ),
    (
        'BCISpoofingDRANDNonce',
        41,
        'db_objection_identifiers_sabotage.sql',
        'Eco-Fort/db/db_objection_identifiers_sabotage.sql',
        'github.com/mk-bluebird/eco_restoration_shard',
        'DRAND-based nonce injection into BCI challenges to prevent replay of neural patterns.',
        datetime('now')
    ),
    (
        'EcoWealthDilutionCircuitUpgrade',
        42,
        'db_objection_identifiers_sabotage.sql',
        'Eco-Fort/db/db_objection_identifiers_sabotage.sql',
        'github.com/mk-bluebird/eco_restoration_shard',
        'Frozen eco-credit circuits with dual supermajority and 30-day timelock for circuit upgrades.',
        datetime('now')
    );
