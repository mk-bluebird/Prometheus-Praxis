-- filename: db_global_blacklist_dao.sql
-- destination: ecorestorationshard/db/db_global_blacklist_dao.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS blacklist_member (
    member_address   TEXT PRIMARY KEY,
    staked_boot      TEXT NOT NULL,
    joined_utc       TEXT NOT NULL,
    active           INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1))
);

CREATE TABLE IF NOT EXISTS blacklist_artifact_proposal (
    proposal_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    cid              TEXT NOT NULL,
    reason           TEXT NOT NULL,
    proposer_address TEXT NOT NULL,
    proposed_utc     TEXT NOT NULL,
    status           TEXT NOT NULL CHECK(status IN ('PENDING','APPROVED','REJECTED')),
    merkle_leaf_hash TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS blacklist_merkle_root (
    root_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    merkle_root_hex  TEXT NOT NULL,
    forest_version   INTEGER NOT NULL,
    anchored_height  INTEGER NOT NULL,
    anchored_tx_hash TEXT NOT NULL,
    created_utc      TEXT NOT NULL,
    UNIQUE(merkle_root_hex, forest_version)
);
