-- Surcharge breach blast-radius index for FOG routing / cyboquatic vaults.

BEGIN TRANSACTION;

CREATE TABLE IF NOT EXISTS blast_radius_index (
  id                   INTEGER PRIMARY KEY AUTOINCREMENT,

  -- Origin of breach
  vault_nodeid         TEXT    NOT NULL,
  breach_scenario_id   TEXT    NOT NULL,  -- e.g. "DESIGN_STORM_10YR", "PFAS_FLUSH"

  -- Downstream canal reach affected
  reach_id             TEXT    NOT NULL,

  -- Hydraulic distance and estimated travel time
  distance_m           REAL    NOT NULL,  -- along-thalweg distance
  delay_seconds        REAL    NOT NULL,  -- Saint-Venant-based travel time

  -- Severity metrics (dimensionless or scaled)
  peak_surcharge_m     REAL    NOT NULL,  -- peak head increase
  peak_r_surcharge     REAL    NOT NULL,  -- normalized rsurcharge ∈ [0,1]
  blast_intensity      REAL    NOT NULL,  -- e.g. distance-weighted B = ΔH·Q·rsurcharge

  -- Corridor / KER flags
  corridorpresent      INTEGER NOT NULL,  -- 1 if all mandatory corridors present
  ker_weighted_radius_m REAL   NOT NULL,  -- KER-weighted effective radius
  safestep             INTEGER NOT NULL,  -- 1 if Vt_{t+1} ≤ Vt_t along this path

  -- Provenance
  model_version        TEXT    NOT NULL,  -- Saint-Venant model / calibration ID
  evidencehex          TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_blast_radius_vault
  ON blast_radius_index (vault_nodeid, breach_scenario_id);

CREATE INDEX IF NOT EXISTS idx_blast_radius_reach
  ON blast_radius_index (reach_id);

COMMIT;
