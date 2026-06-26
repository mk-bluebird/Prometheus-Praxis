-- filename: db/ppx_minimal_continuity_neurorights.sql
-- repo: eco_restoration_shard/db/ppx_minimal_continuity_neurorights.sql

PRAGMA foreign_keys = ON;

-- 1. Brain DIDs (neutral)
CREATE TABLE IF NOT EXISTS ppx_brain_did (
  did        TEXT PRIMARY KEY,
  method     TEXT NOT NULL,
  controller TEXT NOT NULL
);

-- 2. Psych state references
CREATE TABLE IF NOT EXISTS ppx_psych_state_ref (
  id          INTEGER PRIMARY KEY AUTOINCREMENT,
  shard_id    TEXT NOT NULL,
  version_tag TEXT NOT NULL
);

-- 3. Similarity metric registry
CREATE TABLE IF NOT EXISTS ppx_similarity_metric (
  id          TEXT PRIMARY KEY,
  description TEXT NOT NULL
);

-- 4. Psychological continuity evidence (no verdicts)
CREATE TABLE IF NOT EXISTS ppx_psych_continuity_evidence (
  evidence_id     INTEGER PRIMARY KEY AUTOINCREMENT,
  subject_did     TEXT NOT NULL,
  from_state_id   INTEGER NOT NULL,
  to_state_id     INTEGER NOT NULL,
  metric_id       TEXT NOT NULL,
  score           REAL NOT NULL,
  measured_at_utc TEXT NOT NULL,
  notes           TEXT,
  CHECK(score >= 0.0 AND score <= 1.0),
  FOREIGN KEY(subject_did)   REFERENCES ppx_brain_did(did)        ON DELETE CASCADE,
  FOREIGN KEY(from_state_id) REFERENCES ppx_psych_state_ref(id)   ON DELETE CASCADE,
  FOREIGN KEY(to_state_id)   REFERENCES ppx_psych_state_ref(id)   ON DELETE CASCADE,
  FOREIGN KEY(metric_id)     REFERENCES ppx_similarity_metric(id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_ppx_psych_cont_subject_time
  ON ppx_psych_continuity_evidence(subject_did, measured_at_utc);

-- 5. Neurorights corridor specs (evidence only)
CREATE TABLE IF NOT EXISTS ppx_neuroright_corridor_spec (
  id                    TEXT PRIMARY KEY,
  context_tag           TEXT NOT NULL,
  description           TEXT NOT NULL,
  right_name            TEXT NOT NULL,  -- COGNITIVE_LIBERTY, MENTAL_PRIVACY, etc.
  min_protection_level  REAL NOT NULL,
  max_risk_tolerance    REAL NOT NULL,
  CHECK(min_protection_level >= 0.0 AND min_protection_level <= 1.0),
  CHECK(max_risk_tolerance >= 0.0 AND max_risk_tolerance <= 1.0)
);

CREATE INDEX IF NOT EXISTS idx_ppx_neuroright_context
  ON ppx_neuroright_corridor_spec(context_tag, right_name);

-- 6. Systems & well-being components (system-level only)
CREATE TABLE IF NOT EXISTS ppx_system (
  system_id   TEXT PRIMARY KEY,
  description TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS ppx_system_wellbeing_component (
  id             INTEGER PRIMARY KEY AUTOINCREMENT,
  system_id      TEXT NOT NULL,
  context_tag    TEXT NOT NULL,
  component_name TEXT NOT NULL,   -- HEALTH, AUTONOMY, SAFETY, ENVIRONMENT, EQUITY, SOCIAL_COHESION
  value          REAL NOT NULL,
  description    TEXT NOT NULL,
  assessed_at_utc TEXT NOT NULL,
  notes          TEXT,
  CHECK(value >= 0.0 AND value <= 1.0),
  FOREIGN KEY(system_id) REFERENCES ppx_system(system_id) ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_ppx_system_wellbeing
  ON ppx_system_wellbeing_component(system_id, context_tag, component_name);

-- 7. User continuity preferences (metadata only)
CREATE TABLE IF NOT EXISTS ppx_user_continuity_preference (
  subject_did         TEXT PRIMARY KEY,
  preferred_metric_id TEXT NOT NULL,
  preferred_min_score REAL NOT NULL,
  note                TEXT,
  CHECK(preferred_min_score >= 0.0 AND preferred_min_score <= 1.0),
  FOREIGN KEY(subject_did)         REFERENCES ppx_brain_did(did)        ON DELETE CASCADE,
  FOREIGN KEY(preferred_metric_id) REFERENCES ppx_similarity_metric(id) ON DELETE CASCADE
);

-- 8. Sovereignty guarantees (hard invariants for tooling)
CREATE TABLE IF NOT EXISTS ppx_sovereignty_guarantee (
  id   TEXT PRIMARY KEY,
  text TEXT NOT NULL
);

INSERT OR IGNORE INTO ppx_sovereignty_guarantee (id, text) VALUES
  ('PPX-NO-IDENTITY-CLASSIFICATION',
   'No table, column, or enum in this namespace may classify identity as legitimate/illegitimate.'),
  ('PPX-NO-RIGHTS-DOWNGRADE-BY-METRIC',
   'No metric in this namespace may directly downgrade personhood, neurorights, or access to healthcare/cybernetics.');
