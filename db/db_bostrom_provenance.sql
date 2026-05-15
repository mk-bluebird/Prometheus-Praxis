-- filename: db_bostrom_provenance.sql
-- destination: eco_restoration_shard/db/db_bostrom_provenance.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS bostrom_address (
    addressid      INTEGER PRIMARY KEY AUTOINCREMENT,
    address_text   TEXT NOT NULL UNIQUE,  -- full Bostrom / ERC-20-compatible address
    label          TEXT NOT NULL,         -- 'PRIMARY', 'ALTERNATE', 'SAFE_ALT'
    description    TEXT NOT NULL,
    createdutc     TEXT NOT NULL,
    updatedutc     TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS restoration_contract (
    contractid     INTEGER PRIMARY KEY AUTOINCREMENT,
    logicalname    TEXT NOT NULL,     -- e.g. 'restoration.ecoperjoule.policy.2026v1'
    versiontag     TEXT NOT NULL,
    definitionid   INTEGER,           -- FK into definitionregistry_restoration
    description    TEXT NOT NULL,
    region         TEXT NOT NULL,
    status         TEXT NOT NULL,     -- FROZENACTIVE, EXPERIMENTAL
    createdutc     TEXT NOT NULL,
    updatedutc     TEXT NOT NULL,
    UNIQUE (logicalname, versiontag),
    FOREIGN KEY (definitionid) REFERENCES definitionregistry_restoration(definitionid)
        ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS bostrom_contract_binding (
    bindingid      INTEGER PRIMARY KEY AUTOINCREMENT,
    addressid      INTEGER NOT NULL,
    contractid     INTEGER NOT NULL,
    role           TEXT NOT NULL,     -- e.g. 'AUTHOR', 'STEWARD', 'FUNDER'
    evidencehex    TEXT NOT NULL,     -- merkle or hash bundle
    createdutc     TEXT NOT NULL,
    UNIQUE (addressid, contractid, role),
    FOREIGN KEY (addressid) REFERENCES bostrom_address(addressid) ON DELETE CASCADE,
    FOREIGN KEY (contractid) REFERENCES restoration_contract(contractid) ON DELETE CASCADE
);

INSERT OR IGNORE INTO bostrom_address (
    address_text,
    label,
    description,
    createdutc,
    updatedutc
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
    'SAFE_ALT',
    'ERC-20-compatible address for eco-restoration tokens and contracts.',
    datetime('now'),
    datetime('now')
);
