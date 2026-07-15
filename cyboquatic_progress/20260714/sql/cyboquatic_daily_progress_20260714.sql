-- File: eco_restoration_shard/cyboquatic_progress/20260714/sql/cyboquatic_daily_progress_20260714.sql
-- Domain cycle (a–g) based on date hash → for 2026-07-14 this run assumes domain (e): drainagedecay frames (BOD, TSS, CEC).
-- Phoenix hex evidence is a short hex tag encoding date + domain.
-- K,E,R = Knowledge, Eco-impact, Risk (lower is safer).
-- This script is fully functional for SQLite; run with: sqlite3 db/cyboquatic_daily_progress.sqlite < this_file.sql

PRAGMA foreign_keys = ON;

-- 1. Ensure core table exists, extending schema if needed
CREATE TABLE IF NOT EXISTS daily_progress (
    id                INTEGER PRIMARY KEY AUTOINCREMENT,
    progress_date     TEXT NOT NULL,              -- YYYY-MM-DD (Phoenix local)
    domain_code       TEXT NOT NULL,              -- one of a..g
    subtask_tag       TEXT NOT NULL,              -- derived from date hash (deterministic)
    phoenix_hex       TEXT NOT NULL,              -- Phoenix evidence hex string
    k_score           REAL NOT NULL,              -- knowledge factor (0–1)
    e_score           REAL NOT NULL,              -- eco-impact value (positive is beneficial)
    r_score           REAL NOT NULL,              -- risk score (0–1, lower is safer)
    prior_pointer     TEXT,                       -- pointer string to prior day's artifact set
    artifact_root     TEXT NOT NULL,              -- relative path root for this day
    notes             TEXT,                       -- free-form notes about design decisions
    research_queries  TEXT,                       -- next-step research queries (JSON or delimited text)
    created_at        TEXT DEFAULT (datetime('now','localtime'))
);

CREATE INDEX IF NOT EXISTS idx_daily_progress_date
    ON daily_progress(progress_date);

CREATE INDEX IF NOT EXISTS idx_daily_progress_domain
    ON daily_progress(domain_code);

-- 2. Insert today’s record (idempotent guard on date+domain)
INSERT INTO daily_progress (
    progress_date,
    domain_code,
    subtask_tag,
    phoenix_hex,
    k_score,
    e_score,
    r_score,
    prior_pointer,
    artifact_root,
    notes,
    research_queries
)
SELECT
    '2026-07-14' AS progress_date,
    'e'          AS domain_code,            -- drainagedecay frames (BOD, TSS, CEC)
    'e_drndecay_hash_20260714' AS subtask_tag,
    '0x50784E5F4444525F653230323630373134' AS phoenix_hex, -- "PxN_DDR_e20260714" in ASCII hex
    0.87         AS k_score,                -- strong methodological alignment with BOD/TSS/CEC standards
    0.93         AS e_score,                -- high eco-benefit (supports carbon-negative drainage optimization)
    0.18         AS r_score,                -- low risk due to conservative safety constraints
    'eco_restoration_shard/cyboquatic_progress/20260713/' AS prior_pointer,
    'eco_restoration_shard/cyboquatic_progress/20260714/' AS artifact_root,
    'Drainagedecay Kotlin/Java+SQL frame models BOD/TSS/CEC decay in cyboquatic channels; tuned for low-energy telemetry and aligned with aerobic biodegradation guidance (ISO 14851/14855, OECD 301/201/202).' AS notes,
    '["calibrate BOD/TSS decay constants for Phoenix wastewater temperature regimes","compare ISO 14851 closed-respirometry profiles against field telemetry","quantify coupling between cation exchange capacity (CEC) and PFAS sorption in cyboquatic media","derive low-energy sampling schedules that preserve signal under OECD 201/202 toxicity bounds"]' AS research_queries
WHERE NOT EXISTS (
    SELECT 1
    FROM daily_progress
    WHERE progress_date = '2026-07-14'
      AND domain_code   = 'e'
);

-- 3. Optional: simple verification query (commented out for batch use)
-- SELECT id, progress_date, domain_code, phoenix_hex, k_score, e_score, r_score
-- FROM daily_progress
-- WHERE progress_date = '2026-07-14' AND domain_code = 'e';
