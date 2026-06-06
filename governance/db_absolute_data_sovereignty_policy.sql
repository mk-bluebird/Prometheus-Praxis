-- filename: governance/db_absolute_data_sovereignty_policy.sql
-- destination: eco_restoration_shard/governance/db_absolute_data_sovereignty_policy.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS absolute_data_sovereignty_policy (
    policyid TEXT PRIMARY KEY,
    version TEXT NOT NULL,
    ownerdid TEXT NOT NULL,
    hostdid TEXT NOT NULL,
    protected_extensions TEXT NOT NULL,
    binding_event_kinds TEXT NOT NULL,
    requires_contribution INTEGER NOT NULL CHECK (requires_contribution IN (0, 1)),
    contribution_target TEXT NOT NULL,
    countermeasure_flags TEXT NOT NULL,
    quantum_safe_enforcement INTEGER NOT NULL CHECK (quantum_safe_enforcement IN (0, 1)),
    director_authority_only_access INTEGER NOT NULL CHECK (director_authority_only_access IN (0, 1)),
    network_sandbox_enforced INTEGER NOT NULL CHECK (network_sandbox_enforced IN (0, 1)),
    rollback_on_any_violation INTEGER NOT NULL CHECK (rollback_on_any_violation IN (0, 1)),
    audit_integrity_verification INTEGER NOT NULL CHECK (audit_integrity_verification IN (0, 1)),
    immutable_storage INTEGER NOT NULL CHECK (immutable_storage IN (0, 1)),
    createdutc TEXT NOT NULL,
    updatedutc TEXT NOT NULL,
    evidencehex TEXT NOT NULL,
    signinghex TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_absolute_data_sovereignty_policy_owner
    ON absolute_data_sovereignty_policy (ownerdid, hostdid);

CREATE VIEW IF NOT EXISTS v_active_sovereignty_policy AS
SELECT p.*
FROM absolute_data_sovereignty_policy p
JOIN (
    SELECT ownerdid,
           hostdid,
           MAX(createdutc) AS createdutc
    FROM absolute_data_sovereignty_policy
    GROUP BY ownerdid, hostdid
) latest
ON p.ownerdid = latest.ownerdid
AND p.hostdid = latest.hostdid
AND p.createdutc = latest.createdutc
WHERE p.immutable_storage = 1;
