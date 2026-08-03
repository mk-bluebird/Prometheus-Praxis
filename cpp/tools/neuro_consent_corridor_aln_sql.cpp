// File: cpp/tools/neuro_consent_corridor_aln_sql.cpp
#include <iostream>

namespace eco {

const char* ALN_NEURO_CONSENT_CORRIDOR = R"ALN(
entity NeuroConsentCorridor {
  fields {
    consent_id        : Text;
    module_id         : Text;
    subject_id        : Text;   // neuro subject or cohort identifier
    consent_level     : Integer; // 0=NONE,1=BASIC,2=ADVANCED,3=NEURO_ADJACENT
    consent_state     : Text;    // 'PENDING','ACTIVE','REVOKED'
    consent_scope     : Text;    // textual description of allowed operations
    ts_granted        : Time?;
    ts_revoked        : Time?;
  }

  invariant consent_level_range {
    consent_level >= 0 && consent_level <= 3;
  }

  invariant state_machine_valid {
    // PENDING -> ACTIVE or REVOKED; ACTIVE -> REVOKED; REVOKED is terminal.
    consent_state in {'PENDING','ACTIVE','REVOKED'};
  }

  invariant timestamps_consistent {
    (consent_state == 'ACTIVE'  -> ts_granted != null && ts_revoked == null);
    (consent_state == 'REVOKED' -> ts_granted != null && ts_revoked != null && ts_revoked >= ts_granted);
    (consent_state == 'PENDING' -> ts_granted == null && ts_revoked == null);
  }

  invariant neuro_adjacent_requires_active {
    consent_level = 3 -> consent_state = 'ACTIVE';
  }
}

entity SynapseEndpoint {
  fields {
    synapse_id     : Text;
    source_module  : Text;
    target_module  : Text;
    lane           : Text;    // 'RESEARCH','EXPPROD','PROD', etc.
    neuro_flagged  : Bool;    // true if synapse carries neuro-adjacent data
    consent_id     : Text?;   // linked to NeuroConsentCorridor when neuro_flagged
  }

  invariant neuro_synapse_requires_consent {
    neuro_flagged -> consent_id != null;
  }
}
)ALN";

const char* SQL_NEURO_CONSENT_SCHEMA = R"SQL(
-- SQL schema for NeuroConsentCorridor and synapse_endpoint consent fields.

CREATE TABLE IF NOT EXISTS neuro_consent_corridor (
  consent_id      TEXT PRIMARY KEY,
  module_id       TEXT NOT NULL,
  subject_id      TEXT NOT NULL,
  consent_level   INTEGER NOT NULL,   -- 0..3
  consent_state   TEXT NOT NULL,      -- 'PENDING','ACTIVE','REVOKED'
  consent_scope   TEXT NOT NULL,
  ts_granted      TEXT,               -- ISO timestamp
  ts_revoked      TEXT                -- ISO timestamp
);

CREATE TABLE IF NOT EXISTS synapse_endpoint (
  synapse_id     TEXT PRIMARY KEY,
  source_module  TEXT NOT NULL,
  target_module  TEXT NOT NULL,
  lane           TEXT NOT NULL,
  neuro_flagged  INTEGER NOT NULL DEFAULT 0, -- 0=false,1=true
  consent_id     TEXT                         -- nullable, required when neuro_flagged=1
);
)SQL";

const char* SQL_NEURO_CONSENT_TRIGGERS = R"SQL(
-- Triggers enforcing NeuroConsentCorridor state machine and
-- preventing neuro-data leakage at synapse_endpoint.

-- BEFORE INSERT/UPDATE on neuro_consent_corridor: enforce state machine and timestamps.
CREATE TRIGGER IF NOT EXISTS tr_neuro_consent_before_ins
BEFORE INSERT ON neuro_consent_corridor
FOR EACH ROW
BEGIN
  CASE
    WHEN NEW.consent_level < 0 OR NEW.consent_level > 3 THEN
      RAISE(ABORT, 'NeuroConsentCorridor: consent_level must be in [0,3]');
    WHEN NEW.consent_state NOT IN ('PENDING','ACTIVE','REVOKED') THEN
      RAISE(ABORT, 'NeuroConsentCorridor: invalid consent_state');
  END;

  CASE
    WHEN NEW.consent_state = 'ACTIVE' AND NEW.ts_granted IS NULL THEN
      RAISE(ABORT, 'NeuroConsentCorridor: ACTIVE requires ts_granted');
    WHEN NEW.consent_state = 'ACTIVE' AND NEW.ts_revoked IS NOT NULL THEN
      RAISE(ABORT, 'NeuroConsentCorridor: ACTIVE must have ts_revoked NULL');
    WHEN NEW.consent_state = 'REVOKED' AND (NEW.ts_granted IS NULL OR NEW.ts_revoked IS NULL) THEN
      RAISE(ABORT, 'NeuroConsentCorridor: REVOKED requires ts_granted and ts_revoked');
    WHEN NEW.consent_state = 'REVOKED' AND NEW.ts_revoked < NEW.ts_granted THEN
      RAISE(ABORT, 'NeuroConsentCorridor: ts_revoked must be >= ts_granted');
    WHEN NEW.consent_state = 'PENDING' AND (NEW.ts_granted IS NOT NULL OR NEW.ts_revoked IS NOT NULL) THEN
      RAISE(ABORT, 'NeuroConsentCorridor: PENDING must have ts_granted and ts_revoked NULL');
  END;

  CASE
    WHEN NEW.consent_level = 3 AND NEW.consent_state <> 'ACTIVE' THEN
      RAISE(ABORT, 'NeuroConsentCorridor: neuro-adjacent (level 3) requires ACTIVE state');
  END;
END;

CREATE TRIGGER IF NOT EXISTS tr_neuro_consent_before_upd
BEFORE UPDATE ON neuro_consent_corridor
FOR EACH ROW
BEGIN
  CASE
    WHEN NEW.consent_level < 0 OR NEW.consent_level > 3 THEN
      RAISE(ABORT, 'NeuroConsentCorridor: consent_level must be in [0,3]');
    WHEN NEW.consent_state NOT IN ('PENDING','ACTIVE','REVOKED') THEN
      RAISE(ABORT, 'NeuroConsentCorridor: invalid consent_state');
  END;

  CASE
    WHEN NEW.consent_state = 'ACTIVE' AND NEW.ts_granted IS NULL THEN
      RAISE(ABORT, 'NeuroConsentCorridor: ACTIVE requires ts_granted');
    WHEN NEW.consent_state = 'ACTIVE' AND NEW.ts_revoked IS NOT NULL THEN
      RAISE(ABORT, 'NeuroConsentCorridor: ACTIVE must have ts_revoked NULL');
    WHEN NEW.consent_state = 'REVOKED' AND (NEW.ts_granted IS NULL OR NEW.ts_revoked IS NULL) THEN
      RAISE(ABORT, 'NeuroConsentCorridor: REVOKED requires ts_granted and ts_revoked');
    WHEN NEW.consent_state = 'REVOKED' AND NEW.ts_revoked < NEW.ts_granted THEN
      RAISE(ABORT, 'NeuroConsentCorridor: ts_revoked must be >= ts_granted');
    WHEN NEW.consent_state = 'PENDING' AND (NEW.ts_granted IS NOT NULL OR NEW.ts_revoked IS NOT NULL) THEN
      RAISE(ABORT, 'NeuroConsentCorridor: PENDING must have ts_granted and ts_revoked NULL');
  END;

  CASE
    WHEN NEW.consent_level = 3 AND NEW.consent_state <> 'ACTIVE' THEN
      RAISE(ABORT, 'NeuroConsentCorridor: neuro-adjacent (level 3) requires ACTIVE state');
  END;
END;

-- BEFORE INSERT/UPDATE on synapse_endpoint: enforce neuro consent corridor.
CREATE TRIGGER IF NOT EXISTS tr_synapse_endpoint_neuro_before_ins
BEFORE INSERT ON synapse_endpoint
FOR EACH ROW
BEGIN
  CASE
    WHEN NEW.neuro_flagged = 1 AND NEW.consent_id IS NULL THEN
      RAISE(ABORT, 'NeuroConsentCorridor: neuro-flagged synapse requires consent_id');
  END;

  -- Check consent corridor state for linked consent_id.
  CASE
    WHEN NEW.neuro_flagged = 1 AND NOT EXISTS (
      SELECT 1 FROM neuro_consent_corridor c
      WHERE c.consent_id = NEW.consent_id
        AND c.module_id = NEW.source_module
        AND c.consent_state = 'ACTIVE'
        AND c.consent_level = 3
    ) THEN
      RAISE(ABORT, 'NeuroConsentCorridor: no ACTIVE level-3 consent for neuro-flagged synapse');
  END;
END;

CREATE TRIGGER IF NOT EXISTS tr_synapse_endpoint_neuro_before_upd
BEFORE UPDATE ON synapse_endpoint
FOR EACH ROW
BEGIN
  CASE
    WHEN NEW.neuro_flagged = 1 AND NEW.consent_id IS NULL THEN
      RAISE(ABORT, 'NeuroConsentCorridor: neuro-flagged synapse requires consent_id');
  END;

  CASE
    WHEN NEW.neuro_flagged = 1 AND NOT EXISTS (
      SELECT 1 FROM neuro_consent_corridor c
      WHERE c.consent_id = NEW.consent_id
        AND c.module_id = NEW.source_module
        AND c.consent_state = 'ACTIVE'
        AND c.consent_level = 3
    ) THEN
      RAISE(ABORT, 'NeuroConsentCorridor: no ACTIVE level-3 consent for neuro-flagged synapse');
  END;
END;
)SQL";

void print_neuro_consent_specs() {
    std::cout << "=== ALN v2: NeuroConsentCorridor + SynapseEndpoint ===\n\n";
    std::cout << ALN_NEURO_CONSENT_CORRIDOR << "\n\n";

    std::cout << "=== SQL Schema: NeuroConsentCorridor & synapse_endpoint ===\n\n";
    std::cout << SQL_NEURO_CONSENT_SCHEMA << "\n\n";

    std::cout << "=== SQL Triggers: NeuroConsentCorridor & synapse_endpoint ===\n\n";
    std::cout << SQL_NEURO_CONSENT_TRIGGERS << "\n";
}

} // namespace eco

int main() {
    eco::print_neuro_consent_specs();
    return 0;
}
