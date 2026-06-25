PRAGMA foreign_keys = ON;

-- Core table for HealthcareRiskPlane2026v1 coordinates
CREATE TABLE IF NOT EXISTS healthcare_risk_plane_2026v1 (
  coordinate_id TEXT PRIMARY KEY,
  identityid TEXT NOT NULL,
  soicref TEXT NOT NULL,
  mt6883_course_id TEXT,
  region_id TEXT NOT NULL,
  birthsign_id TEXT NOT NULL,
  plane_version TEXT NOT NULL CHECK (plane_version = 'HEALTHCARE_RISK_PLANE_2026V1'),

  r_healthcare REAL NOT NULL CHECK (r_healthcare >= 0.0 AND r_healthcare <= 1.0),
  r_nanoswarm REAL NOT NULL CHECK (r_nanoswarm >= 0.0 AND r_nanoswarm <= 1.0),
  r_surgery REAL NOT NULL CHECK (r_surgery >= 0.0 AND r_surgery <= 1.0),
  r_thermal_overload REAL NOT NULL CHECK (r_thermal_overload >= 0.0 AND r_thermal_overload <= 1.0),
  r_near_death REAL NOT NULL CHECK (r_near_death >= 0.0 AND r_near_death <= 1.0),

  roh_norm REAL NOT NULL CHECK (roh_norm >= 0.0 AND roh_norm <= 1.0),
  roh_ceiling_norm REAL NOT NULL CHECK (roh_ceiling_norm >= 0.0 AND roh_ceiling_norm <= 1.0),

  near_death_corridor_flag INTEGER NOT NULL CHECK (near_death_corridor_flag IN (0,1)),

  psych_continuity_pressure REAL NOT NULL CHECK (psych_continuity_pressure >= 0.0 AND psych_continuity_pressure <= 1.0),
  continuity_floor_norm REAL NOT NULL CHECK (continuity_floor_norm >= 0.0 AND continuity_floor_norm <= 1.0),
  continuity_margin_norm REAL NOT NULL CHECK (continuity_margin_norm >= 0.0 AND continuity_margin_norm <= 1.0),

  ker_eco_impact REAL NOT NULL CHECK (ker_eco_impact >= -1.0 AND ker_eco_impact <= 1.0),
  ker_risk_of_harm REAL NOT NULL CHECK (ker_risk_of_harm >= 0.0 AND ker_risk_of_harm <= 1.0),
  ker_knowledge_yield REAL NOT NULL CHECK (ker_knowledge_yield >= 0.0 AND ker_knowledge_yield <= 1.0),
  ker_composite REAL NOT NULL CHECK (ker_composite >= -1.0 AND ker_composite <= 1.0),

  reward_ker_sum REAL NOT NULL CHECK (reward_ker_sum >= 0.0),
  reward_corridor_kind TEXT NOT NULL CHECK (reward_corridor_kind = 'REWARD_CORRIDOR'),

  rohanchorhex TEXT NOT NULL,
  continuity_proofhex TEXT,
  nonrollback_provenance_anchor_id TEXT,

  vtresidual REAL NOT NULL CHECK (vtresidual >= 0.0 AND vtresidual <= 1.0),
  topologyrisk REAL NOT NULL CHECK (topologyrisk >= 0.0 AND topologyrisk <= 1.0),

  created_utc TEXT NOT NULL,
  updated_utc TEXT NOT NULL,
  active INTEGER NOT NULL CHECK (active IN (0,1))
);

-- Indexes to support Virta-Sys and MT6883 lookups
CREATE INDEX IF NOT EXISTS idx_hrp2026_identity_plane
  ON healthcare_risk_plane_2026v1 (identityid, plane_version);

CREATE INDEX IF NOT EXISTS idx_hrp2026_mt6883
  ON healthcare_risk_plane_2026v1 (mt6883_course_id);

CREATE INDEX IF NOT EXISTS idx_hrp2026_region_birthsign
  ON healthcare_risk_plane_2026v1 (region_id, birthsign_id);

-- Optional foreign key into a shared NonRollbackProvenanceAnchor table
-- (table must exist in the same DB or attached DB)
CREATE TABLE IF NOT EXISTS nonrollback_provenance_anchor_2026v1 (
  anchor_id TEXT PRIMARY KEY,
  schemahex TEXT NOT NULL,
  owner_did TEXT NOT NULL,
  signinghex TEXT NOT NULL,
  created_utc TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_nrpa2026_owner
  ON nonrollback_provenance_anchor_2026v1 (owner_did);

-- Enforce that nonrollback_provenance_anchor_id, if present, refers to NRPA table
CREATE TRIGGER IF NOT EXISTS trg_hrp2026_nrpa_fk
BEFORE INSERT ON healthcare_risk_plane_2026v1
WHEN NEW.nonrollback_provenance_anchor_id IS NOT NULL
BEGIN
  SELECT CASE
    WHEN NOT EXISTS (
      SELECT 1 FROM nonrollback_provenance_anchor_2026v1
      WHERE anchor_id = NEW.nonrollback_provenance_anchor_id
    )
    THEN RAISE(ABORT,'nonrollback_provenance_anchor_id missing in NRPA table')
  END;
END;

-- Global RoH ceiling of 0.30 when not in near-death corridor
CREATE TRIGGER IF NOT EXISTS trg_hrp2026_global_roh_ceiling
BEFORE INSERT ON healthcare_risk_plane_2026v1
BEGIN
  SELECT CASE
    WHEN NEW.near_death_corridor_flag = 0
         AND (NEW.roh_norm > 0.30 OR NEW.ker_risk_of_harm > 0.30)
    THEN RAISE(ABORT,'RoH ceiling exceeded outside near-death corridor')
  END;
END;

CREATE TRIGGER IF NOT EXISTS trg_hrp2026_global_roh_ceiling_update
BEFORE UPDATE ON healthcare_risk_plane_2026v1
BEGIN
  SELECT CASE
    WHEN NEW.near_death_corridor_flag = 0
         AND (NEW.roh_norm > 0.30 OR NEW.ker_risk_of_harm > 0.30)
    THEN RAISE(ABORT,'RoH ceiling exceeded outside near-death corridor')
  END;
END;

-- Enforce roh_norm as max of coordinates (up to DB-level check)
CREATE TRIGGER IF NOT EXISTS trg_hrp2026_roh_norm_max
BEFORE INSERT ON healthcare_risk_plane_2026v1
BEGIN
  SELECT CASE
    WHEN NEW.roh_norm < MAX(
           NEW.r_healthcare,
           NEW.r_nanoswarm,
           NEW.r_surgery,
           NEW.r_thermal_overload,
           NEW.r_near_death
         )
    THEN RAISE(ABORT,'roh_norm must be >= max component risk')
  END;
END;

CREATE TRIGGER IF NOT EXISTS trg_hrp2026_roh_norm_max_update
BEFORE UPDATE ON healthcare_risk_plane_2026v1
BEGIN
  SELECT CASE
    WHEN NEW.roh_norm < MAX(
           NEW.r_healthcare,
           NEW.r_nanoswarm,
           NEW.r_surgery,
           NEW.r_thermal_overload,
           NEW.r_near_death
         )
    THEN RAISE(ABORT,'roh_norm must be >= max component risk')
  END;
END;

-- Continuity proof required for near-death corridor rows
CREATE TRIGGER IF NOT EXISTS trg_hrp2026_continuity_near_death
BEFORE INSERT ON healthcare_risk_plane_2026v1
WHEN NEW.near_death_corridor_flag = 1
BEGIN
  SELECT CASE
    WHEN NEW.continuity_proofhex IS NULL
         OR NEW.continuity_proofhex NOT GLOB '0x[0-9a-f][0-9a-f]*'
         OR LENGTH(NEW.continuity_proofhex) != 66
    THEN RAISE(ABORT,'continuity_proofhex required and must be 256-bit hex for near-death corridor')
  END;
END;

CREATE TRIGGER IF NOT EXISTS trg_hrp2026_continuity_near_death_update
BEFORE UPDATE ON healthcare_risk_plane_2026v1
WHEN NEW.near_death_corridor_flag = 1
BEGIN
  SELECT CASE
    WHEN NEW.continuity_proofhex IS NULL
         OR NEW.continuity_proofhex NOT GLOB '0x[0-9a-f][0-9a-f]*'
         OR LENGTH(NEW.continuity_proofhex) != 66
    THEN RAISE(ABORT,'continuity_proofhex required and must be 256-bit hex for near-death corridor')
  END;
END;

-- reward_ker_sum monotone non-decreasing at DB level
CREATE TABLE IF NOT EXISTS healthcare_risk_plane_2026v1_log (
  log_id INTEGER PRIMARY KEY AUTOINCREMENT,
  coordinate_id TEXT NOT NULL,
  identityid TEXT NOT NULL,
  plane_version TEXT NOT NULL,
  reward_ker_sum REAL NOT NULL,
  created_utc TEXT NOT NULL
);

CREATE TRIGGER IF NOT EXISTS trg_hrp2026_log_insert
AFTER INSERT ON healthcare_risk_plane_2026v1
BEGIN
  INSERT INTO healthcare_risk_plane_2026v1_log (coordinate_id, identityid, plane_version, reward_ker_sum, created_utc)
  VALUES (NEW.coordinate_id, NEW.identityid, NEW.plane_version, NEW.reward_ker_sum, NEW.created_utc);
END;

CREATE TRIGGER IF NOT EXISTS trg_hrp2026_reward_monotone
BEFORE UPDATE ON healthcare_risk_plane_2026v1
BEGIN
  SELECT CASE
    WHEN NEW.reward_ker_sum < (
      SELECT MAX(l.reward_ker_sum)
      FROM healthcare_risk_plane_2026v1_log l
      WHERE l.identityid = NEW.identityid
        AND l.plane_version = NEW.plane_version
    )
    THEN RAISE(ABORT,'reward_ker_sum must be non-decreasing (use dual-review compensating events instead)')
  END;
END;

-- Hash-only pattern checks for rohanchorhex and continuity_proofhex
CREATE TRIGGER IF NOT EXISTS trg_hrp2026_hex_patterns
BEFORE INSERT ON healthcare_risk_plane_2026v1
BEGIN
  SELECT CASE
    WHEN NEW.rohanchorhex NOT GLOB '0x[0-9a-f][0-9a-f]*'
         OR LENGTH(NEW.rohanchorhex) != 66
    THEN RAISE(ABORT,'bad rohanchorhex')
  END;
END;

CREATE TRIGGER IF NOT EXISTS trg_hrp2026_hex_patterns_update
BEFORE UPDATE ON healthcare_risk_plane_2026v1
BEGIN
  SELECT CASE
    WHEN NEW.rohanchorhex NOT GLOB '0x[0-9a-f][0-9a-f]*'
         OR LENGTH(NEW.rohanchorhex) != 66
    THEN RAISE(ABORT,'bad rohanchorhex')
  END;
END;
