-- filename: sql/shards/db_econet_centralaz_tools.sql
-- destination: eco_restoration_shard/sql/shards/db_econet_centralaz_tools.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS gila_ecoli_buffer_plan_2026 (
    nodeid          TEXT NOT NULL,
    region          TEXT NOT NULL,
    parameter       TEXT NOT NULL,
    buffer_width_m  REAL NOT NULL,
    cin             REAL NOT NULL,
    cout_base       REAL NOT NULL,
    cout_buffer     REAL NOT NULL,
    flow            REAL NOT NULL,
    windowstart     TEXT NOT NULL,
    windowend       TEXT NOT NULL,
    cref            REAL NOT NULL,
    hazardweight    REAL NOT NULL,
    kn_base         REAL NOT NULL,
    kn_buffer       REAL NOT NULL,
    ecoimpact_gain  REAL NOT NULL
);

CREATE TABLE IF NOT EXISTS lake_pleasant_pfbs_early_warning_2026 (
    nodeid          TEXT NOT NULL,
    region          TEXT NOT NULL,
    parameter       TEXT NOT NULL,
    cin             REAL NOT NULL,
    cout            REAL NOT NULL,
    flow            REAL NOT NULL,
    windowstart     TEXT NOT NULL,
    windowend       TEXT NOT NULL,
    cref            REAL NOT NULL,
    hazardweight    REAL NOT NULL,
    kn              REAL NOT NULL,
    ecoimpactscore  REAL NOT NULL,
    safe_band_flag  INTEGER NOT NULL CHECK (safe_band_flag IN (0,1)),
    advice_text     TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS salinity_offset_plan_2026 (
    nodeid            TEXT NOT NULL,
    region            TEXT NOT NULL,
    parameter         TEXT NOT NULL,
    cin               REAL NOT NULL,
    cout              REAL NOT NULL,
    flow              REAL NOT NULL,
    windowstart       TEXT NOT NULL,
    windowend         TEXT NOT NULL,
    delta_mass_tons   REAL NOT NULL,
    target_share_tons REAL NOT NULL,
    karma_per_ton     REAL NOT NULL,
    karma_allocated   REAL NOT NULL
);

CREATE TABLE IF NOT EXISTS econet_identity_2026 (
    identityid       TEXT PRIMARY KEY,
    github_org       TEXT NOT NULL,
    bostrom_did      TEXT NOT NULL,
    karma_current    REAL NOT NULL,
    tolerance_level  REAL NOT NULL
);

CREATE TABLE IF NOT EXISTS ceimxj_karma_window_2026 (
    nodeid           TEXT NOT NULL,
    stakeholderid    TEXT NOT NULL,
    contaminant      TEXT NOT NULL,
    cin              REAL NOT NULL,
    cout             REAL NOT NULL,
    flow             REAL NOT NULL,
    windowstart      TEXT NOT NULL,
    windowend        TEXT NOT NULL,
    cref             REAL NOT NULL,
    hazardweight     REAL NOT NULL,
    kn               REAL NOT NULL,
    ecoimpactscore   REAL NOT NULL,
    mintcap_tokens   REAL NOT NULL,
    burndue_tokens   REAL NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_ceimxj_karma_window_node
    ON ceimxj_karma_window_2026(nodeid, contaminant, windowend);
