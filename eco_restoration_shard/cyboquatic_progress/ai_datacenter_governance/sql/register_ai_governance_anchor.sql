-- filename: eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/sql/register_ai_governance_anchor.sql

INSERT OR IGNORE INTO phoenix_hex_anchor (
    logical_name,
    evidencehex,
    domain,
    subdomain,
    region_code,
    yyyymmdd,
    prior_anchor_id,
    signing_did,
    summary,
    default_relpath,
    created_utc,
    active
) VALUES (
    'PHX_AI_DC_GOV_FRAMEWORK_20260716',
    '0x20260716PHXAIDCGOVFRAMEWORKV1',
    'CYBOQUATIC',
    'AI_DC_GOVERNANCE',
    'PHX-CAZ-CEIM',
    '20260716',
    NULL,
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    'AI datacentre governance shard: ALN particle, 10‑axis mapping, KER derivation, daily progress schema.',
    'eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance',
    '2026-07-16T00:00:00Z',
    1
);

INSERT OR IGNORE INTO phoenix_hex_file (
    anchor_id,
    relpath,
    filename,
    file_type,
    file_hash_hex,
    scope,
    created_utc
) SELECT
    a.anchor_id,
    'eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/aln/AiDatacenterNode2026v1.aln',
    'AiDatacenterNode2026v1.aln',
    'ALN',
    '0xPHXHASHAIDCNODE2026V1',
    'PARTICLE',
    '2026-07-16T00:00:00Z'
FROM phoenix_hex_anchor a
WHERE a.logical_name = 'PHX_AI_DC_GOV_FRAMEWORK_20260716';

INSERT OR IGNORE INTO phoenix_hex_file (
    anchor_id,
    relpath,
    filename,
    file_type,
    file_hash_hex,
    scope,
    created_utc
) SELECT
    a.anchor_id,
    'eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/docs/10axis_to_risk_mapping.md',
    '10axis_to_risk_mapping.md',
    'DOC',
    '0xPHXHASHTENAXISMAP',
    'DOC',
    '2026-07-16T00:00:00Z'
FROM phoenix_hex_anchor a
WHERE a.logical_name = 'PHX_AI_DC_GOV_FRAMEWORK_20260716';

INSERT OR IGNORE INTO phoenix_hex_file (
    anchor_id,
    relpath,
    filename,
    file_type,
    file_hash_hex,
    scope,
    created_utc
) SELECT
    a.anchor_id,
    'eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/docs/ker_derivation.md',
    'ker_derivation.md',
    'DOC',
    '0xPHXHASHKERDERIV',
    'DOC',
    '2026-07-16T00:00:00Z'
FROM phoenix_hex_anchor a
WHERE a.logical_name = 'PHX_AI_DC_GOV_FRAMEWORK_20260716';

INSERT OR IGNORE INTO phoenix_hex_file (
    anchor_id,
    relpath,
    filename,
    file_type,
    file_hash_hex,
    scope,
    created_utc
) SELECT
    a.anchor_id,
    'eco_restoration_shard/cyboquatic_progress/ai_datacenter_governance/sql/daily_progress_ai_node.sql',
    'daily_progress_ai_node.sql',
    'SQL',
    '0xPHXHASHDAILYPROGRESSAI',
    'MIGRATION',
    '2026-07-16T00:00:00Z'
FROM phoenix_hex_anchor a
WHERE a.logical_name = 'PHX_AI_DC_GOV_FRAMEWORK_20260716';
