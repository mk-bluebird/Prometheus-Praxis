-- filename: db_bostrom_contract_binding_upgrade.sql
-- destination: ecorestorationshard/db/db_bostrom_contract_binding_upgrade.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 42. Objection:
-- The current bostromcontractbinding schema has no way to revoke or supersede
-- bindings; stale AUTHOR/STEWARD roles remain indistinguishable from active ones,
-- leading to ambiguous authority for contracts.

-- Upgrade DDL: add deprecation and supersession while preserving referential integrity.

ALTER TABLE bostromcontractbinding
ADD COLUMN deprecated_utc TEXT;

ALTER TABLE bostromcontractbinding
ADD COLUMN superseded_by INTEGER;

-- superseded_by references another binding row; enforce via a separate constraint table
-- to avoid circular FK issues in ALTER TABLE for existing rows.

CREATE TABLE IF NOT EXISTS bostromcontractbinding_supersession (
    binding_id     INTEGER NOT NULL,
    superseded_by  INTEGER NOT NULL,
    PRIMARY KEY (binding_id, superseded_by),
    FOREIGN KEY (binding_id)
        REFERENCES bostromcontractbinding(bindingid)
        ON DELETE CASCADE,
    FOREIGN KEY (superseded_by)
        REFERENCES bostromcontractbinding(bindingid)
        ON DELETE CASCADE
);

-- View to show only active bindings (no deprecated_utc and not superseded).

CREATE VIEW IF NOT EXISTS vbostromcontractbinding_active AS
SELECT b.*
FROM bostromcontractbinding AS b
LEFT JOIN bostromcontractbinding_supersession AS s
  ON s.binding_id = b.bindingid
WHERE b.deprecated_utc IS NULL
  AND s.binding_id IS NULL;
