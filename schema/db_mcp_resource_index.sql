-- file: schema/db_mcp_resource_index.sql

PRAGMA foreign_keys = ON;

----------------------------------------------------------------------
--  mcp_repo: root of the constellation-wide knowledge graph
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS mcp_repo (
    repoid              INTEGER PRIMARY KEY AUTOINCREMENT,
    reponame            TEXT    NOT NULL UNIQUE,
    githubslug          TEXT    NOT NULL,
    roleband            TEXT    NOT NULL CHECK (roleband IN (
                            'SPINE','RESEARCH','ENGINE','MATERIAL','GOV','APP'
                        )),
    primaryplane        TEXT    NOT NULL, -- e.g. 'CARBON','BIODIVERSITY','WATER'
    lanedefault         TEXT    NOT NULL CHECK (lanedefault IN (
                            'RESEARCH','EXPPROD','PROD'
                        )),
    nonactuatingonly    INTEGER NOT NULL DEFAULT 1 CHECK (nonactuatingonly IN (0,1)),
    didowner            TEXT    NOT NULL,
    description         TEXT    NOT NULL,
    createdutc          TEXT    NOT NULL DEFAULT (
                            strftime('%Y-%m-%dT%H:%M:%SZ','now')
                        ),
    updatedutc          TEXT    NOT NULL DEFAULT (
                            strftime('%Y-%m-%dT%H:%M:%SZ','now')
                        )
);

CREATE INDEX IF NOT EXISTS idx_mcp_repo_roleband
    ON mcp_repo(roleband);

CREATE INDEX IF NOT EXISTS idx_mcp_repo_lanedefault
    ON mcp_repo(lanedefault);

----------------------------------------------------------------------
--  mcp_file: index of MCP-addressable files per repo
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS mcp_file (
    fileid          INTEGER PRIMARY KEY AUTOINCREMENT,
    repoid          INTEGER NOT NULL REFERENCES mcp_repo(repoid)
                            ON DELETE CASCADE,
    relpath         TEXT    NOT NULL,      -- e.g. 'src/ker/spine/'
    filename        TEXT    NOT NULL,      -- e.g. 'ker_spine.cxx'
    filekind        TEXT    NOT NULL CHECK (filekind IN (
                        'SQL','RUST','ALN','CSV','DOC','LUA',
                        'KOTLIN','ANDROID','JSON','OTHER'
                    )),
    filerole        TEXT    NOT NULL CHECK (filerole IN (
                        'SCHEMA','INDEX','MCP_TOOL','CONFIG',
                        'DOC','VIEW','DATASET'
                    )),
    planebands      TEXT    NOT NULL,      -- e.g. 'CARBON;WATER'
    logicalname     TEXT    NULL,
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    maintainerdid   TEXT    NULL,
    createdutc      TEXT    NOT NULL DEFAULT (
                        strftime('%Y-%m-%dT%H:%M:%SZ','now')
                    ),
    updatedutc      TEXT    NOT NULL DEFAULT (
                        strftime('%Y-%m-%dT%H:%M:%SZ','now')
                    ),
    UNIQUE (repoid, relpath, filename)
);

CREATE INDEX IF NOT EXISTS idx_mcp_file_repoid
    ON mcp_file(repoid);

CREATE INDEX IF NOT EXISTS idx_mcp_file_active
    ON mcp_file(active);

----------------------------------------------------------------------
--  mcp_tool: logical tool registry with governance metadata
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS mcp_tool (
    toolid          INTEGER PRIMARY KEY AUTOINCREMENT,
    repoid          INTEGER NOT NULL REFERENCES mcp_repo(repoid)
                            ON DELETE CASCADE,
    fileid          INTEGER NOT NULL REFERENCES mcp_file(fileid)
                            ON DELETE CASCADE,
    toolname        TEXT    NOT NULL UNIQUE,
    summary         TEXT    NOT NULL,
    toolkind        TEXT    NOT NULL CHECK (toolkind IN (
                        'COMMAND','FILE','HTTP','SQL_QUERY','RUST_FN'
                    )),
    resourcemode    TEXT    NOT NULL CHECK (resourcemode IN (
                        'READONLY','MUTATING'
                    )),
    lanedefault     TEXT    NOT NULL CHECK (lanedefault IN (
                        'RESEARCH','EXPPROD','PROD'
                    )),
    planebands      TEXT    NOT NULL,
    citizen_ready   INTEGER NOT NULL DEFAULT 0 CHECK (citizen_ready IN (0,1)),
    neuroflag       INTEGER NOT NULL DEFAULT 0 CHECK (neuroflag IN (0,1)),
    input_schema    TEXT    NULL,
    output_schema   TEXT    NULL,
    stewarddid      TEXT    NULL,
    ker_hint        TEXT    NULL,
    active          INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1)),
    createdutc      TEXT    NOT NULL DEFAULT (
                        strftime('%Y-%m-%dT%H:%M:%SZ','now')
                    ),
    updatedutc      TEXT    NOT NULL DEFAULT (
                        strftime('%Y-%m-%dT%H:%M:%SZ','now')
                    )
);

CREATE INDEX IF NOT EXISTS idx_mcp_tool_repoid
    ON mcp_tool(repoid);

CREATE INDEX IF NOT EXISTS idx_mcp_tool_resourcemode
    ON mcp_tool(resourcemode);

CREATE INDEX IF NOT EXISTS idx_mcp_tool_lanedefault
    ON mcp_tool(lanedefault);

CREATE INDEX IF NOT EXISTS idx_mcp_tool_neuroflag
    ON mcp_tool(neuroflag);

----------------------------------------------------------------------
--  mcp_endpoint: concrete implementation bindings per tool
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS mcp_endpoint (
    endpointid      INTEGER PRIMARY KEY AUTOINCREMENT,
    toolid          INTEGER NOT NULL REFERENCES mcp_tool(toolid)
                            ON DELETE CASCADE,
    endpoint_type   TEXT    NOT NULL CHECK (endpoint_type IN (
                        'CLI','SQL','RUST_FN','LUA','KOTLIN','ANDROID_VIEW'
                    )),
    identifier      TEXT    NOT NULL,  -- e.g. 'eco_ker_spine_eval'
    sql_view_name   TEXT    NULL,      -- populated when endpoint_type='SQL'
    rust_fn_path    TEXT    NULL,      -- 'crate::module::fn_name'
    script_entry    TEXT    NULL,      -- for CLI / scripting entry points
    is_default      INTEGER NOT NULL DEFAULT 1 CHECK (is_default IN (0,1)),
    createdutc      TEXT    NOT NULL DEFAULT (
                        strftime('%Y-%m-%dT%H:%M:%SZ','now')
                    ),
    updatedutc      TEXT    NOT NULL DEFAULT (
                        strftime('%Y-%m-%dT%H:%M:%SZ','now')
                    ),
    UNIQUE (toolid, endpoint_type, identifier)
);

CREATE INDEX IF NOT EXISTS idx_mcp_endpoint_toolid
    ON mcp_endpoint(toolid);

CREATE INDEX IF NOT EXISTS idx_mcp_endpoint_is_default
    ON mcp_endpoint(is_default);

----------------------------------------------------------------------
--  neuro_capability: one-to-one overlay on mcp_tool for neuro-tools
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS neuro_capability (
    neurocapid          INTEGER PRIMARY KEY AUTOINCREMENT,
    toolid              INTEGER NOT NULL UNIQUE
                            REFERENCES mcp_tool(toolid) ON DELETE CASCADE,
    capability_type     TEXT    NOT NULL CHECK (capability_type IN (
                            'COGNITIVE_GUARD',
                            'EDU_PROMPT',
                            'BIOFEEDBACK_ANALYSIS',
                            'NEURO_METRIC_REPORT'
                        )),
    roh_ceiling         REAL    NOT NULL CHECK (roh_ceiling BETWEEN 0.0 AND 1.0),
    ker_neuro_hint      TEXT    NOT NULL,
    continuityanchorid  TEXT    NULL,   -- FK into continuity_anchor when present
    fearpain_policycode TEXT    NULL,
    healthcare_lane     INTEGER NOT NULL DEFAULT 0 CHECK (healthcare_lane IN (0,1)),
    diagnostic_only     INTEGER NOT NULL DEFAULT 1 CHECK (diagnostic_only IN (0,1)),
    cohort_scope        TEXT    NULL,
    createdutc          TEXT    NOT NULL DEFAULT (
                            strftime('%Y-%m-%dT%H:%M:%SZ','now')
                        ),
    updatedutc          TEXT    NOT NULL DEFAULT (
                            strftime('%Y-%m-%dT%H:%M:%SZ','now')
                        )
);

----------------------------------------------------------------------
--  governance_verdict: append-only governance chain for lane/status
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS governance_verdict (
    verdictid               INTEGER PRIMARY KEY AUTOINCREMENT,
    action_type             TEXT NOT NULL CHECK (action_type IN (
                                'LANE_PROMOTION',
                                'SHARD_DEPLOYMENT',
                                'TOOL_VERDICT'
                            )),
    target_object_id        TEXT NOT NULL,  -- e.g. toolid, shard id, lane id
    target_object_type      TEXT NOT NULL,  -- e.g. 'MCP_TOOL','QPUDATASHARD'
    authorized_by_did       TEXT NOT NULL,
    evidence_hex            TEXT NOT NULL,  -- hex of evidence bundle hash
    continuityanchorid      TEXT NOT NULL,  -- contract binding the decision
    definition_registry_ref TEXT NOT NULL,  -- e.g. filename or ALN id
    approvedutc             TEXT NOT NULL DEFAULT (
                                strftime('%Y-%m-%dT%H:%M:%SZ','now')
                            ),
    signature               TEXT NULL       -- DID signature of the record
);

CREATE INDEX IF NOT EXISTS idx_gov_verdict_target
    ON governance_verdict(target_object_type, target_object_id);

CREATE INDEX IF NOT EXISTS idx_gov_verdict_action_type
    ON governance_verdict(action_type);

----------------------------------------------------------------------
--  Views for MCP discovery and neuro governance
----------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_mcp_tools AS
SELECT
    t.toolid,
    t.toolname,
    t.summary,
    t.toolkind,
    t.resourcemode,
    t.lanedefault,
    t.planebands,
    t.citizen_ready,
    t.neuroflag,
    t.input_schema,
    t.output_schema,
    t.stewarddid,
    t.ker_hint,
    t.active        AS tool_active,
    r.repoid,
    r.reponame,
    r.githubslug,
    r.roleband,
    r.primaryplane,
    r.lanedefault   AS repo_lanedefault,
    r.nonactuatingonly,
    r.didowner,
    f.fileid,
    f.relpath,
    f.filename,
    f.filekind,
    f.filerole,
    f.planebands    AS file_planebands,
    f.logicalname,
    f.active        AS file_active
FROM mcp_tool t
JOIN mcp_repo r  ON t.repoid = r.repoid
JOIN mcp_file f  ON t.fileid = f.fileid
WHERE t.active = 1
  AND f.active = 1;

CREATE VIEW IF NOT EXISTS v_neuro_capabilities AS
SELECT
    v.toolid,
    v.toolname,
    v.summary,
    v.toolkind,
    v.resourcemode,
    v.lanedefault      AS toollane,
    v.planebands       AS tool_planebands,
    v.citizen_ready,
    v.neuroflag,
    n.capability_type,
    n.roh_ceiling,
    n.ker_neuro_hint,
    n.continuityanchorid,
    n.fearpain_policycode,
    n.healthcare_lane,
    n.diagnostic_only,
    n.cohort_scope,
    v.repoid,
    v.reponame,
    v.githubslug,
    v.roleband,
    v.primaryplane,
    v.repo_lanedefault,
    v.nonactuatingonly,
    v.didowner
FROM v_mcp_tools v
JOIN neuro_capability n
  ON n.toolid = v.toolid
WHERE v.neuroflag = 1;

----------------------------------------------------------------------
--  Trigger helpers for updatedutc
----------------------------------------------------------------------

CREATE TRIGGER IF NOT EXISTS trg_mcp_repo_updated
AFTER UPDATE ON mcp_repo
FOR EACH ROW
BEGIN
    UPDATE mcp_repo
       SET updatedutc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
     WHERE repoid = NEW.repoid;
END;

CREATE TRIGGER IF NOT EXISTS trg_mcp_file_updated
AFTER UPDATE ON mcp_file
FOR EACH ROW
BEGIN
    UPDATE mcp_file
       SET updatedutc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
     WHERE fileid = NEW.fileid;
END;

CREATE TRIGGER IF NOT EXISTS trg_mcp_tool_updated
AFTER UPDATE ON mcp_tool
FOR EACH ROW
BEGIN
    UPDATE mcp_tool
       SET updatedutc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
     WHERE toolid = NEW.toolid;
END;

CREATE TRIGGER IF NOT EXISTS trg_mcp_endpoint_updated
AFTER UPDATE ON mcp_endpoint
FOR EACH ROW
BEGIN
    UPDATE mcp_endpoint
       SET updatedutc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
     WHERE endpointid = NEW.endpointid;
END;

CREATE TRIGGER IF NOT EXISTS trg_neuro_capability_updated
AFTER UPDATE ON neuro_capability
FOR EACH ROW
BEGIN
    UPDATE neuro_capability
       SET updatedutc = strftime('%Y-%m-%dT%H:%M:%SZ','now')
     WHERE neurocapid = NEW.neurocapid;
END;
