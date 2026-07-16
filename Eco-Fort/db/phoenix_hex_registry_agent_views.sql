-- filename: Eco-Fort/db/phoenix_hex_registry_agent_views.sql
-- destination: Eco-Fort/db/phoenix_hex_registry_agent_views.sql
-- repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
--
-- Purpose:
--   Agent- and AI-chat-friendly views for convenient hex discovery.[file:36]

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
-- 1. v_phx_hex_daily_by_domain
--    Daily anchors filtered by domain/subdomain.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_phx_hex_daily_by_domain AS
SELECT
    anchor_id,
    logical_name,
    evidence_hex,
    domain,
    subdomain,
    region_code,
    planes,
    yyyymmdd,
    prior_anchor_id,
    summary,
    default_relpath
FROM phoenix_hex_anchor
WHERE yyyymmdd IS NOT NULL;

----------------------------------------------------------------------
-- 2. v_phx_hex_forward_chain
--    Forward-only chain per domain/subdomain sorted by date and ID.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_phx_hex_forward_chain AS
SELECT
    anchor_id,
    logical_name,
    evidence_hex,
    domain,
    subdomain,
    region_code,
    planes,
    yyyymmdd,
    prior_anchor_id,
    created_utc
FROM phoenix_hex_anchor
ORDER BY domain, subdomain, yyyymmdd, anchor_id;

----------------------------------------------------------------------
-- 3. v_phx_hex_files_for_agent
--    Minimal projection for AI-chat to locate files for an anchor.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_phx_hex_files_for_agent AS
SELECT
    a.logical_name,
    a.evidence_hex,
    a.domain,
    a.subdomain,
    a.planes,
    a.yyyymmdd,
    f.relpath,
    f.filename,
    f.file_type,
    f.scope
FROM phoenix_hex_anchor AS a
JOIN phoenix_hex_file AS f
  ON f.anchor_id = a.anchor_id;

----------------------------------------------------------------------
-- 4. v_phx_hex_particles_for_agent
--    Minimal projection for AI-chat to locate ALN particles.
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_phx_hex_particles_for_agent AS
SELECT
    a.logical_name,
    a.evidence_hex,
    a.domain,
    a.subdomain,
    a.planes,
    a.yyyymmdd,
    p.particle_name,
    p.particle_relpath,
    p.particle_role,
    p.evidence_table,
    p.evidence_column
FROM phoenix_hex_anchor AS a
JOIN phoenix_hex_particle_binding AS p
  ON p.anchor_id = a.anchor_id;
