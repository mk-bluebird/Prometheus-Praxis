-- filename: db_gov_set_zk_circuit.sql
-- destination: ecorestorationshard/db/db_gov_set_zk_circuit.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS eco_zk_circuit_registry (
    circuit_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    logical_name       TEXT    NOT NULL,
    version_tag        TEXT    NOT NULL,
    vk_hash_hex        TEXT    NOT NULL,
    pk_hash_hex        TEXT    NOT NULL,
    on_chain_contract  TEXT    NOT NULL,
    status             TEXT    NOT NULL CHECK(status IN ('FROZEN_ACTIVE', 'FROZEN_DEPRECATED', 'EXPERIMENTAL')),
    created_utc        TEXT    NOT NULL,
    updated_utc        TEXT    NOT NULL,
    UNIQUE (logical_name, version_tag)
);

-- Example logical_name: 'eco.aggregate.delta_r.median.2026v1'
-- vk_hash_hex/pk_hash_hex store the hashes of verification/proving keys that MsgAggregateEcoEvidence refers to.
