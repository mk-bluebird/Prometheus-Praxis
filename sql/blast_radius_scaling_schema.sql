-- Filename: sql/blast_radius_scaling_schema.sql
-- Destination: Prometheus-Praxis/sql/blast_radius_scaling_schema.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS blast_radius_event (
    event_id            TEXT PRIMARY KEY,
    node_id             TEXT NOT NULL,
    rho_kgm3            REAL NOT NULL,
    g_ms2               REAL NOT NULL,
    surcharge_head_m    REAL NOT NULL,
    channel_width_m     REAL NOT NULL,
    channel_depth_m     REAL NOT NULL,
    channel_length_m    REAL NOT NULL,
    velocity_mps        REAL NOT NULL,
    fog_confinement     REAL NOT NULL,
    radius_overtop_m    REAL NOT NULL,
    radius_scour_m      REAL NOT NULL,
    e_star              REAL NOT NULL,
    r_star_overtop      REAL NOT NULL,
    r_star_scour        REAL NOT NULL,
    timestamputc        TEXT NOT NULL,
    evidence_hex        TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_blast_radius_event_node_time
    ON blast_radius_event (node_id, timestamputc);

CREATE TABLE IF NOT EXISTS blast_radius_master_curve_fit (
    fit_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    corridor_id    TEXT NOT NULL,
    a_coefficient  REAL NOT NULL,
    beta_exponent  REAL NOT NULL,
    sample_count   INTEGER NOT NULL,
    timestamputc   TEXT NOT NULL,
    evidence_hex   TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_blast_radius_master_curve_corridor
    ON blast_radius_master_curve_fit (corridor_id, timestamputc);
