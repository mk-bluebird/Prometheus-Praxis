-- Filename: db/db_qpudatashard_water_quality.sql

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS qpudatashard_water_quality (
  id                INTEGER PRIMARY KEY AUTOINCREMENT,
  nodeid            TEXT    NOT NULL,
  windowstartts     TEXT    NOT NULL,  -- ISO-8601
  windowendts       TEXT    NOT NULL,

  -- Raw Phoenix water-quality measurements (typical units)
  bod_mg_per_l      REAL    NOT NULL,
  tss_mg_per_l      REAL    NOT NULL,
  n_mg_per_l        REAL    NOT NULL,
  p_mg_per_l        REAL    NOT NULL,
  cec_ug_per_l      REAL    NOT NULL,  -- aggregate CEC
  pfas_ng_per_l     REAL    NOT NULL,  -- PFAS sum

  -- Normalized risk coordinates r ∈ [0,1] per ecosafety grammar
  r_bod             REAL    NOT NULL,
  r_tss             REAL    NOT NULL,
  r_n               REAL    NOT NULL,
  r_p               REAL    NOT NULL,
  r_cec             REAL    NOT NULL,
  r_pfas            REAL    NOT NULL,

  -- Aggregate biology/contaminant risk (optional)
  r_bio             REAL    NOT NULL,

  -- Local Lyapunov residual and KER scores if computed
  vt                REAL    NOT NULL,
  kerk              REAL    NOT NULL,
  kere              REAL    NOT NULL,
  kerr              REAL    NOT NULL,

  -- Provenance: hex evidence stamp + signer DID binding
  evidencehex       TEXT    NOT NULL,
  signingdid        TEXT    NOT NULL,

  -- Ingestion metadata
  source_csv        TEXT    NOT NULL,
  ingested_at_utc   TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_qpudatashard_wq_node_window
  ON qpudatashard_water_quality (nodeid, windowendts);

COMMIT;
