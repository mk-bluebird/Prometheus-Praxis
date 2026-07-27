PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Agent function catalog table for AI-visible functions
-------------------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS agentfunctioncatalog (
    functionid         TEXT PRIMARY KEY,
    summary            TEXT NOT NULL,
    inputschema        TEXT NOT NULL,    -- JSON Schema path or inline
    outputschema       TEXT NOT NULL,    -- JSON Schema path or inline
    backingview_or_bin TEXT NOT NULL,    -- SQL view name or binary/FFI symbol
    lanescope          TEXT NOT NULL,    -- comma-separated allowed lanes
    riskbands          TEXT NOT NULL,    -- comma-separated risk bands
    complexityscore    TEXT NOT NULL,    -- e.g. O(n), O(log n), O(1)
    aicapabilitylevel  TEXT NOT NULL CHECK (
        aicapabilitylevel IN ('NONE', 'DOC', 'READONLY_SPINE', 'PROTO_AGENT')
    ),
    versiontag         TEXT NOT NULL DEFAULT '2026v1',
    status             TEXT NOT NULL CHECK (status IN ('ACTIVE', 'DEPRECATED')),
    createdutc         TEXT NOT NULL DEFAULT (datetime('now')),
    updatedutc         TEXT NOT NULL DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_agentfunctioncatalog_status
    ON agentfunctioncatalog (status);

CREATE INDEX IF NOT EXISTS idx_agentfunctioncatalog_lanescope
    ON agentfunctioncatalog (lanescope);

-------------------------------------------------------------------------------
-- 2. Seed entries for core AI-visible catalog functions and views
-------------------------------------------------------------------------------

INSERT OR REPLACE INTO agentfunctioncatalog (
    functionid, summary, inputschema, outputschema, backingview_or_bin,
    lanescope, riskbands, complexityscore, aicapabilitylevel, versiontag, status
) VALUES (
    'FUNC_REPO_MANIFEST',
    'Retrieve repo manifest with safety, lanes, and contract information.',
    '{"type":"object","properties":{"reponame":{"type":"string"}},"required":["reponame"]}',
    '{"type":"object","properties":{"reponame":{"type":"string"},"roleband":{"type":"string"},"nonactuatingonly":{"type":"integer"}}}',
    'veconet_repo_manifest_agent',
    'RESEARCH,EXPPROD,PROD',
    'LOW_RISK',
    'O(1)',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

INSERT OR REPLACE INTO agentfunctioncatalog (
    functionid, summary, inputschema, outputschema, backingview_or_bin,
    lanescope, riskbands, complexityscore, aicapabilitylevel, versiontag, status
) VALUES (
    'FUNC_SAFE_CATALOG',
    'List all AI-visible non-actuating objects for a repository.',
    '{"type":"object","properties":{"reponame":{"type":"string"}},"required":["reponame"]}',
    '{"type":"array","items":{"type":"object","properties":{"objectid":{"type":"string"},"objectkind":{"type":"string"}}}}',
    'v_agent_safe_catalog',
    'RESEARCH,EXPPROD,PROD',
    'LOW_RISK',
    'O(n)',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

INSERT OR REPLACE INTO agentfunctioncatalog (
    functionid, summary, inputschema, outputschema, backingview_or_bin,
    lanescope, riskbands, complexityscore, aicapabilitylevel, versiontag, status
) VALUES (
    'FUNC_BLASTRADIUS_SUMMARY',
    'Get blast-radius summary per node or machine across planes.',
    '{"type":"object","properties":{"nodeid":{"type":"string"}},"required":["nodeid"]}',
    '{"type":"array","items":{"type":"object","properties":{"nodeid":{"type":"string"},"impacttype":{"type":"string"},"total_impact_score":{"type":"number"}}}}',
    'vmachine_blastradius',
    'RESEARCH,EXPPROD,PROD',
    'LOW_RISK,MODERATE_RISK',
    'O(n)',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

INSERT OR REPLACE INTO agentfunctioncatalog (
    functionid, summary, inputschema, outputschema, backingview_or_bin,
    lanescope, riskbands, complexityscore, aicapabilitylevel, versiontag, status
) VALUES (
    'FUNC_NODE_WINDOW',
    'Get workload window aggregates for a Cyboquatic node.',
    '{"type":"object","properties":{"nodeid":{"type":"string"}},"required":["nodeid"]}',
    '{"type":"array","items":{"type":"object","properties":{"nodeid":{"type":"string"},"channel":{"type":"string"},"mean_delta_vt":{"type":"number"}}}}',
    'vcyboworkloadnodewindow',
    'RESEARCH,EXPPROD',
    'LOW_RISK',
    'O(n)',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

INSERT OR REPLACE INTO agentfunctioncatalog (
    functionid, summary, inputschema, outputschema, backingview_or_bin,
    lanescope, riskbands, complexityscore, aicapabilitylevel, versiontag, status
) VALUES (
    'FUNC_PATTERN_LOOKUP',
    'Fetch parameterized SQL pattern by ID.',
    '{"type":"object","properties":{"patternid":{"type":"string"}},"required":["patternid"]}',
    '{"type":"object","properties":{"sqltext":{"type":"string"},"lanescope":{"type":"string"}}}',
    'agentsqlpattern',
    'RESEARCH,EXPPROD,PROD',
    'LOW_RISK',
    'O(1)',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

INSERT OR REPLACE INTO agentfunctioncatalog (
    functionid, summary, inputschema, outputschema, backingview_or_bin,
    lanescope, riskbands, complexityscore, aicapabilitylevel, versiontag, status
) VALUES (
    'FUNC_KER_TARGETS',
    'Query KER targets for a repository.',
    '{"type":"object","properties":{"reponame":{"type":"string"}},"required":["reponame"]}',
    '{"type":"object","properties":{"kertargetk":{"type":"number"},"kertargete":{"type":"number"},"kertargetr":{"type":"number"}}}',
    'econetrepoindex',
    'RESEARCH,EXPPROD,PROD',
    'LOW_RISK',
    'O(1)',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

INSERT OR REPLACE INTO agentfunctioncatalog (
    functionid, summary, inputschema, outputschema, backingview_or_bin,
    lanescope, riskbands, complexityscore, aicapabilitylevel, versiontag, status
) VALUES (
    'FUNC_SHREDDING_GOVERNANCE_SNAPSHOT',
    'KER-weighted blast-radius and lane verdict snapshot for a shredding machine, strictly non-actuating.',
    '{"$ref":"schemas/prometheus_praxis_get_shredding_snapshot_json.input.json"}',
    '{"$ref":"schemas/prometheus_praxis_get_shredding_snapshot_json.output.json"}',
    'prometheus_praxis_get_shredding_snapshot_json',
    'RESEARCH',
    'LOW_RISK',
    'O(1)',
    'READONLY_SPINE',
    '2026v1',
    'ACTIVE'
);

-------------------------------------------------------------------------------
-- 3. ALN catalog shard for governance-safe functions (tool grammar)
-------------------------------------------------------------------------------

alnversion 1.0.0
alnnamespace econet.agent.function.catalog.v1

particle econet.agent.function.entry.v1
  field functionid        str.v1
  field summary           str.v1
  field inputschema       str.v1
  field outputschema      str.v1
  field backingffi        str.v1
  field lanescope         enum.lane.band.v1
  field roleband          enum.role.band.v1
  field riskband          enum.risk.band.v1
  field blastradiusclass  enum.blastradius.class.v1
  field aicapabilitylevel enum.ai.capability.level.v1
  field versiontag        str.v1
  field status            enum.function.status.v1
end

enum lane.band.v1
  RESEARCH
  EXPPROD
  PROD
end

enum role.band.v1
  RESTORATIONMONO
  GOVERNANCE
  DIAGNOSTIC
end

enum risk.band.v1
  LOW
  MEDIUM
  HIGH
end

enum blastradius.class.v1
  NONACTUATING_DIAGNOSTIC
  GOVERNANCE_GUARD
end

enum ai.capability.level.v1
  DIAGNOSTICONLY
  MAYRANKKER
end

enum function.status.v1
  ACTIVE
  DEPRECATED
end

particle econet.agent.function.catalog.v1
  field entries list.econet.agent.function.entry.v1
end

object econet.agent.function.entry.v1
  functionid        "prometheus_praxis_get_shredding_snapshot_json.v1"
  summary           "KER-weighted blast-radius and lane verdict snapshot for a shredding machine, strictly non-actuating."
  inputschema       "schemas/prometheus_praxis_get_shredding_snapshot_json.input.json"
  outputschema      "schemas/prometheus_praxis_get_shredding_snapshot_json.output.json"
  backingffi        "prometheus_praxis_get_shredding_snapshot_json"
  lanescope         RESEARCH
  roleband          DIAGNOSTIC
  riskband          LOW
  blastradiusclass  NONACTUATING_DIAGNOSTIC
  aicapabilitylevel DIAGNOSTICONLY
  versiontag        "2026v1"
  status            ACTIVE
end

-------------------------------------------------------------------------------
-- 4. Register catalog shard in definitionregistry for discoverability
-------------------------------------------------------------------------------

INSERT OR IGNORE INTO definitionregistry (
    contractid,
    scope,
    logicalname,
    kind,
    repo,
    destinationpath,
    filename,
    language,
    versiontag,
    active,
    primaryplane,
    appliescope,
    summary,
    signingdid,
    issuedutc,
    updatedutc
) VALUES (
    'EcosafetyContinuity2026v1',
    'AICHAT',
    'agentfunctioncatalog',
    'ALN',
    'eco_restoration_shard',
    'aln',
    'econet.agent.function.catalog.v1.aln',
    'ALN',
    '2026v1',
    1,
    'GOVERNANCE',
    'AICHAT',
    'ALN shard defining grammar-level specification for AI-visible functions.',
    'bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7',
    datetime('now'),
    datetime('now')
);
