-- File: ecoshard/sqlite/ecosafety_index_v1.sql
-- This SQL file mirrors the Rust schema for environments where Rust is not available.
-- Destination: ecoshard/sqlite/ecosafety_index_v1.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS repo (
    repo_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL UNIQUE,
    url             TEXT NOT NULL,
    description     TEXT,
    created_utc     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    is_authoritative INTEGER NOT NULL DEFAULT 0,
    hex_provenance   TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS project (
    project_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    repo_id         INTEGER NOT NULL REFERENCES repo(repo_id) ON DELETE CASCADE,
    name            TEXT NOT NULL,
    description     TEXT,
    steward_did     TEXT NOT NULL,
    corridor_spec_id TEXT,
    UNIQUE (repo_id, name)
);

CREATE TABLE IF NOT EXISTS artifact (
    artifact_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    project_id      INTEGER NOT NULL REFERENCES project(project_id) ON DELETE CASCADE,
    path            TEXT NOT NULL,
    language        TEXT NOT NULL CHECK (language IN (
                        'rust','aln','lua','cpp','kotlin','sql','schema','doc'
                    )),
    kind            TEXT NOT NULL CHECK (kind IN (
                        'library','binary','config','schema','document'
                    )),
    blast_radius    TEXT NOT NULL CHECK (blast_radius IN (
                        'diagnostic_only','local_actuation','city_scale'
                    )),
    semantic_hash   TEXT NOT NULL,
    provenance_hex  TEXT NOT NULL,
    created_utc     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    UNIQUE (project_id, path)
);

CREATE TABLE IF NOT EXISTS artifact_ker (
    artifact_id     INTEGER PRIMARY KEY REFERENCES artifact(artifact_id) ON DELETE CASCADE,
    k_knowledge     REAL NOT NULL CHECK (k_knowledge >= 0.0 AND k_knowledge <= 1.0),
    e_eco_impact    REAL NOT NULL CHECK (e_eco_impact >= 0.0 AND e_eco_impact <= 1.0),
    r_risk_of_harm  REAL NOT NULL CHECK (r_risk_of_harm >= 0.0 AND r_risk_of_harm <= 1.0),
    ker_deployable  INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS artifact_planes (
    artifact_id     INTEGER PRIMARY KEY REFERENCES artifact(artifact_id) ON DELETE CASCADE,
    r_energy        REAL NOT NULL CHECK (r_energy >= 0.0 AND r_energy <= 1.0),
    r_hydraulic     REAL NOT NULL CHECK (r_hydraulic >= 0.0 AND r_hydraulic <= 1.0),
    r_biology       REAL NOT NULL CHECK (r_biology >= 0.0 AND r_biology <= 1.0),
    r_carbon        REAL NOT NULL CHECK (r_carbon >= 0.0 AND r_carbon <= 1.0),
    r_materials     REAL NOT NULL CHECK (r_materials >= 0.0 AND r_materials <= 1.0),
    v_residual      REAL NOT NULL,
    CHECK (v_residual >= 0.0)
);

CREATE TABLE IF NOT EXISTS blast_radius_band (
    band_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name            TEXT NOT NULL UNIQUE,
    description     TEXT,
    min_k           REAL NOT NULL CHECK (min_k >= 0.0 AND min_k <= 1.0),
    min_e           REAL NOT NULL CHECK (min_e >= 0.0 AND min_e <= 1.0),
    max_r           REAL NOT NULL CHECK (max_r >= 0.0 AND max_r <= 1.0),
    allows_actuation INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS artifact_band (
    artifact_id     INTEGER NOT NULL REFERENCES artifact(artifact_id) ON DELETE CASCADE,
    band_id         INTEGER NOT NULL REFERENCES blast_radius_band(band_id) ON DELETE CASCADE,
    PRIMARY KEY (artifact_id, band_id)
);

CREATE TABLE IF NOT EXISTS index_snapshot (
    snapshot_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    created_utc     TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ','now')),
    aln_index_spec  TEXT NOT NULL,
    index_quality   REAL NOT NULL CHECK (index_quality >= 0.0 AND index_quality <= 1.0)
);

CREATE INDEX IF NOT EXISTS idx_artifact_language
    ON artifact(language);

CREATE INDEX IF NOT EXISTS idx_artifact_blastradius
    ON artifact(blast_radius);

CREATE INDEX IF NOT EXISTS idx_artifact_ker_k
    ON artifact_ker(k_knowledge);

CREATE INDEX IF NOT EXISTS idx_artifact_ker_r
    ON artifact_ker(r_risk_of_harm);

CREATE INDEX IF NOT EXISTS idx_artifact_planes_v
    ON artifact_planes(v_residual);

INSERT OR IGNORE INTO blast_radius_band
    (name, description, min_k, min_e, max_r, allows_actuation)
VALUES
    (
        'diagnostic_research',
        'Function-only diagnostics; non-actuating frames and SQL schemas. Safe for broad replication.',
        0.80, 0.80, 0.20, 0
    ),
    (
        'local_actuation_pilot',
        'Pilot-grade local actuation; requires K>=0.90, E>=0.90, R<=0.13.',
        0.90, 0.90, 0.13, 1
    ),
    (
        'city_scale_ker_strict',
        'City-scale ecosafety machinery under Artemis-Cyboquatic governance; K>=0.94, E>=0.91, R<=0.12.',
        0.94, 0.91, 0.12, 1
    );
