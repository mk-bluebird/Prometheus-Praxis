-- File: sql/hex_restoration_commitment.sql
PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS hex_restoration_commitment (
    commitment_id INTEGER PRIMARY KEY AUTOINCREMENT,
    h3_index TEXT NOT NULL,
    did TEXT NOT NULL,
    interventions TEXT NOT NULL,
    target_ker_e REAL NOT NULL,
    target_lst_drop_k REAL NOT NULL,
    start_date_utc TEXT NOT NULL,  -- YYYY-MM-DD
    end_date_utc TEXT NOT NULL,    -- YYYY-MM-DD
    FOREIGN KEY(h3_index) REFERENCES hex_thermal_recovery(h3_index),
    FOREIGN KEY(did) REFERENCES governance_particle(did),
    CHECK (target_ker_e <= 0.0)
);

CREATE INDEX IF NOT EXISTS idx_hex_commitment_h3_dates
    ON hex_restoration_commitment(h3_index, start_date_utc, end_date_utc);

-- Progress reporting view: join commitments with hex_thermal_recovery to compute
-- average afternoon LST reduction relative to a baseline (e.g., pre-commitment mean).

-- Assume a baseline table or view hex_lst_baseline(h3_index, baseline_afternoon_lst_k).
CREATE TABLE IF NOT EXISTS hex_lst_baseline (
    h3_index TEXT PRIMARY KEY,
    baseline_afternoon_lst_k REAL NOT NULL
);

CREATE VIEW IF NOT EXISTS hex_restoration_progress AS
SELECT
    hr.commitment_id,
    hr.h3_index,
    hr.did,
    hr.interventions,
    hr.target_ker_e,
    hr.target_lst_drop_k,
    hr.start_date_utc,
    hr.end_date_utc,
    b.baseline_afternoon_lst_k,
    AVG(b.baseline_afternoon_lst_k - h.afternoon_lst_k) AS avg_lst_drop_k,
    CASE
        WHEN AVG(b.baseline_afternoon_lst_k - h.afternoon_lst_k) >= hr.target_lst_drop_k
        THEN 'ON_TARGET'
        ELSE 'BELOW_TARGET'
    END AS progress_status
FROM hex_restoration_commitment hr
JOIN hex_thermal_recovery h
  ON h.h3_index = hr.h3_index
JOIN hex_lst_baseline b
  ON b.h3_index = hr.h3_index
WHERE h.date_utc >= hr.start_date_utc
  AND h.date_utc <= hr.end_date_utc
GROUP BY hr.commitment_id, hr.h3_index, hr.did, hr.interventions,
         hr.target_ker_e, hr.target_lst_drop_k,
         hr.start_date_utc, hr.end_date_utc, b.baseline_afternoon_lst_k;
