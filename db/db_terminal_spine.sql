-- filename: db/db_terminal_spine.sql
-- destination: eco_restoration_shard/db/db_terminal_spine.sql
-- Purpose:
--   - Evidence-only record of DBSC, continuous endpoint auth, and AAL
--     for EcoNet-related terminals and sessions.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS terminal_device (
    device_id      TEXT PRIMARY KEY,        -- pseudo-id, no PHI
    platform       TEXT NOT NULL,           -- 'windows','macos','linux'
    tpm_present    INTEGER NOT NULL CHECK (tpm_present IN (0,1)),
    secure_enclave INTEGER NOT NULL CHECK (secure_enclave IN (0,1)),
    region_code    TEXT NOT NULL,           -- coarse region only
    notes          TEXT
);

CREATE TABLE IF NOT EXISTS terminal_session (
    session_id     TEXT PRIMARY KEY,
    device_id      TEXT NOT NULL REFERENCES terminal_device(device_id)
                   ON DELETE CASCADE,
    neuron_id      TEXT,                    -- optional Bostrom DID for owner
    origin         TEXT NOT NULL,           -- e.g. 'https://ecofort.app'
    dbsc_enabled   INTEGER NOT NULL CHECK (dbsc_enabled IN (0,1)),
    aal_level      TEXT NOT NULL CHECK (aal_level IN ('AAL1','AAL2','AAL3')),
    t_start_utc    TEXT NOT NULL,
    t_end_utc      TEXT,
    idle_timeout_s INTEGER,                 -- configured idle timeout
    reauth_interval_s INTEGER,              -- configured overall reauth max
    notes          TEXT
);

CREATE TABLE IF NOT EXISTS terminal_dbsc_event (
    event_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id     TEXT NOT NULL REFERENCES terminal_session(session_id)
                   ON DELETE CASCADE,
    event_type     TEXT NOT NULL CHECK (event_type IN ('REGISTER','REFRESH','FAIL')),
    algo           TEXT NOT NULL,           -- 'EC-P256','ML-DSA','HYBRID'
    nonce_len      INTEGER,
    latency_ms     REAL,
    created_utc    TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS terminal_cea_event (
    event_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    session_id     TEXT NOT NULL REFERENCES terminal_session(session_id)
                   ON DELETE CASCADE,
    engine         TEXT NOT NULL,           -- 'typingdna_3_5','custom'
    keystrokes     INTEGER NOT NULL,
    anomaly_score  REAL NOT NULL,           -- 0..1 anomaly
    action         TEXT NOT NULL CHECK (action IN ('ALLOW','STEP_UP','LOCK')),
    created_utc    TEXT NOT NULL
);

CREATE VIEW IF NOT EXISTS v_terminal_session_risk AS
SELECT
    s.session_id,
    s.device_id,
    s.aal_level,
    s.dbsc_enabled,
    MIN(d.event_type)              AS first_dbsc_event,
    MAX(d.event_type)              AS last_dbsc_event,
    COUNT(CASE WHEN d.event_type='FAIL' THEN 1 END) AS dbsc_fail_count,
    MAX(c.anomaly_score)          AS cea_max_anomaly,
    MAX(CASE WHEN c.action='LOCK' THEN 1 ELSE 0 END) AS locked_flag
FROM terminal_session s
LEFT JOIN terminal_dbsc_event d ON d.session_id = s.session_id
LEFT JOIN terminal_cea_event c ON c.session_id = s.session_id
GROUP BY s.session_id, s.device_id, s.aal_level, s.dbsc_enabled;
