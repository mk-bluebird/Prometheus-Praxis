-- filename: db/db_bostrom_provenance.sql
-- destination: eco_restoration_shard/db/db_bostrom_provenance.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- Bostrom address registry
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS bostrom_address (
    address_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    address_text  TEXT NOT NULL UNIQUE,  -- full Bostrom / ERC-20-compatible address
    label         TEXT NOT NULL,         -- PRIMARY, ALTERNATE, SAFE_ALT, ERC20
    description   TEXT NOT NULL,
    created_utc   TEXT NOT NULL,
    updated_utc   TEXT NOT NULL
);

----------------------------------------------------------------------
-- Restoration contracts, optionally linked to DefinitionRegistry slice
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS restoration_contract (
    contract_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    logical_name   TEXT NOT NULL,    -- e.g. restoration.ecoperjoule.policy.2026v1
    version_tag    TEXT NOT NULL,
    definition_id  INTEGER,          -- FK into definitionregistry_restoration
    description    TEXT NOT NULL,
    region         TEXT NOT NULL,
    status         TEXT NOT NULL,    -- FROZEN_ACTIVE, FROZEN_DEPRECATED, EXPERIMENTAL
    created_utc    TEXT NOT NULL,
    updated_utc    TEXT NOT NULL,
    UNIQUE (logical_name, version_tag, region),
    FOREIGN KEY (definition_id)
        REFERENCES definitionregistry_restoration(definition_id)
        ON DELETE SET NULL
);

----------------------------------------------------------------------
-- Binding between Bostrom addresses and restoration contracts
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS bostrom_contract_binding (
    binding_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    address_id    INTEGER NOT NULL,
    contract_id   INTEGER NOT NULL,
    role          TEXT NOT NULL,     -- AUTHOR, STEWARD, FUNDER, REVIEWER
    evidence_hex  TEXT NOT NULL,     -- hash or Merkle root bundle
    created_utc   TEXT NOT NULL,
    UNIQUE (address_id, contract_id, role),
    FOREIGN KEY (address_id)  REFERENCES bostrom_address(address_id)    ON DELETE CASCADE,
    FOREIGN KEY (contract_id) REFERENCES restoration_contract(contract_id) ON DELETE CASCADE
);

----------------------------------------------------------------------
-- Seed primary and alternate addresses for eco_restoration_shard
----------------------------------------------------------------------

INSERT OR IGNORE INTO bostrom_address (
    address_text,
    label,
    description,
    created_utc,
    updated_utc
) VALUES
(
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'PRIMARY',
    'Primary Bostrom address for eco-restoration governance and funding.',
    datetime('now'),
    datetime('now')
),
(
    'bostrom1ldgmtf20d6604a24ztr0jxht7xt7az4jhkmsrc',
    'ALTERNATE',
    'Alternate, secure, Google-linked Bostrom address requiring active RT monitoring.',
    datetime('now'),
    datetime('now')
),
(
    'zeta12x0up66pzyeretzyku8p4ccuxrjqtqpdc4y4x8',
    'SAFE_ALT',
    'Safe alternate address for eco-restoration shards.',
    datetime('now'),
    datetime('now')
),
(
    '0x519fC0eB4111323Cac44b70e1aE31c30e405802D',
    'ERC20',
    'ERC-20-compatible address for eco-restoration tokens and contracts.',
    datetime('now'),
    datetime('now')
);

----------------------------------------------------------------------
-- Seed core restoration contracts for this mono-repo
-- Adjust definition_id once definitionregistry_restoration rows exist.
----------------------------------------------------------------------

INSERT OR IGNORE INTO restoration_contract (
    logical_name,
    version_tag,
    definition_id,
    description,
    region,
    status,
    created_utc,
    updated_utc
) VALUES
(
    'restoration.governance.index.2026v1',
    '2026v1',
    NULL,
    'Governance index contract for restorationindex.sqlite3 in eco_restoration_shard.',
    'Phoenix-AZ',
    'FROZEN_ACTIVE',
    datetime('now'),
    datetime('now')
),
(
    'restoration.filewiring.2026v1',
    '2026v1',
    NULL,
    'File wiring and lane-band mapping contract for eco_restoration_shard.',
    'Phoenix-AZ',
    'FROZEN_ACTIVE',
    datetime('now'),
    datetime('now')
),
(
    'restoration.blastradius.phoenix.2026v1',
    '2026v1',
    NULL,
    'Blast-radius and restoration-radius grammar for Phoenix restoration nodes.',
    'Phoenix-AZ',
    'EXPERIMENTAL',
    datetime('now'),
    datetime('now')
),
(
    'energy.ecoperjoule.policy.phoenix.2026v1',
    '2026v1',
    NULL,
    'Eco-per-joule policy for Phoenix eco-restoration assets linked to this repo.',
    'Phoenix-AZ',
    'EXPERIMENTAL',
    datetime('now'),
    datetime('now')
);

----------------------------------------------------------------------
-- Bind primary Bostrom address as AUTHOR for core contracts
----------------------------------------------------------------------

INSERT OR IGNORE INTO bostrom_contract_binding (
    address_id,
    contract_id,
    role,
    evidence_hex,
    created_utc
)
SELECT
    a.address_id,
    c.contract_id,
    'AUTHOR',
    '0000000000000000000000000000000000000000000000000000000000000000',
    datetime('now')
FROM bostrom_address AS a
JOIN restoration_contract AS c
WHERE a.address_text = 'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7'
  AND c.logical_name IN (
      'restoration.governance.index.2026v1',
      'restoration.filewiring.2026v1',
      'restoration.blastradius.phoenix.2026v1',
      'energy.ecoperjoule.policy.phoenix.2026v1'
  );
