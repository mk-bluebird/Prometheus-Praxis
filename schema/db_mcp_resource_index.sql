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
