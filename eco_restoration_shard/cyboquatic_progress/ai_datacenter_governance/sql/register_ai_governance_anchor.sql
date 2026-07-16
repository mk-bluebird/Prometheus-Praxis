-- filename: eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/sql/register_ai_governance_anchor.sql
-- purpose : Inserts the Phoenix hex anchor for the AI datacenter governance framework
--           (Object 1‑5), plus file and particle bindings into the global registry.

-- Ensure you are connected to Eco-Fort/db/phoenix_hex_registry.sqlite

INSERT OR IGNORE INTO phoenix_hex_anchor (
    logical_name,
    evidence_hex,
    domain,
    subdomain,
    region_code,
    planes,
    yyyymmdd,
    prior_anchor_id,
    signing_did,
    summary,
    description,
    file_class,
    default_relpath,
    created_utc,
    active
) VALUES (
    'PHX_AI_DC_GOV_FRAMEWORK_20260716',
    '0x20260716PHXAIDCGOVFRAMEWORKV1',
    'CYBOQUATIC',              -- AI data centers fall under cyboquatic machinery
    'AI_DC_GOVERNANCE',
    'PHX-CAZ-CEIM',
    'ENERGY,CARBON,MATERIALS,TOPOLOGY,GOV,DATA',
    '20260716',
    NULL,                      -- first of its kind
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'Phoenix AI Datacenter Governance Framework – template ALN particle, 10‑axis mapping, KER derivation, and daily progress SQLite shard.',
    'Foundational set of artifacts (Objects 1‑5) that integrates AI data centres as Cyboquatic nodes under the Lyapunov/KER grammar, with primary constraints and secondary guidance metrics.',
    'MIXED',
    'eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance',
    '2026-07-16T00:00:00Z',
    1
);

-- File bindings (one per artifact)
INSERT OR IGNORE INTO phoenix_hex_file (anchor_id, relpath, filename, file_type, file_hash_hex, scope, created_utc)
SELECT
    a.anchor_id,
    'eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/aln/AiDatacenterNode2026v1.aln',
    'AiDatacenterNode2026v1.aln',
    'ALN',
    '0xPHXHASHAIDCNODE2026V1',   -- replace with actual hash
    'PARTICLE',
    '2026-07-16T00:00:00Z'
FROM phoenix_hex_anchor a WHERE a.logical_name = 'PHX_AI_DC_GOV_FRAMEWORK_20260716';

INSERT OR IGNORE INTO phoenix_hex_file (anchor_id, relpath, filename, file_type, file_hash_hex, scope, created_utc)
SELECT
    a.anchor_id,
    'eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/docs/10axis_to_risk_mapping.md',
    '10axis_to_risk_mapping.md',
    'DOC',
    '0xPHXHASHTENAXISMAP',
    'DOC',
    '2026-07-16T00:00:00Z'
FROM phoenix_hex_anchor a WHERE a.logical_name = 'PHX_AI_DC_GOV_FRAMEWORK_20260716';

INSERT OR IGNORE INTO phoenix_hex_file (anchor_id, relpath, filename, file_type, file_hash_hex, scope, created_utc)
SELECT
    a.anchor_id,
    'eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/docs/ker_derivation.md',
    'ker_derivation.md',
    'DOC',
    '0xPHXHASHKERDERIVATION',
    'DOC',
    '2026-07-16T00:00:00Z'
FROM phoenix_hex_anchor a WHERE a.logical_name = 'PHX_AI_DC_GOV_FRAMEWORK_20260716';

INSERT OR IGNORE INTO phoenix_hex_file (anchor_id, relpath, filename, file_type, file_hash_hex, scope, created_utc)
SELECT
    a.anchor_id,
    'eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/sql/daily_progress_ai_node.sql',
    'daily_progress_ai_node.sql',
    'SQL',
    '0xPHXHASHDAILYPROGRESSAI',
    'MIGRATION',
    '2026-07-16T00:00:00Z'
FROM phoenix_hex_anchor a WHERE a.logical_name = 'PHX_AI_DC_GOV_FRAMEWORK_20260716';

-- Particle binding: link the ALN particle to this anchor
INSERT OR IGNORE INTO phoenix_hex_particle_binding (
    anchor_id,
    particle_name,
    particle_relpath,
    particle_role,
    evidence_table,
    evidence_column,
    notes,
    created_utc
)
SELECT
    a.anchor_id,
    'AiDatacenterNode2026v1',
    'eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/aln/AiDatacenterNode2026v1.aln',
    'GOVERNANCE',
    'daily_progress_ai_node',
    'evidence_hex',
    'Template ALN particle for AI Cyboquatic nodes; defines all planes, corridor bands, and KER engine.',
    '2026-07-16T00:00:00Z'
FROM phoenix_hex_anchor a WHERE a.logical_name = 'PHX_AI_DC_GOV_FRAMEWORK_20260716';
