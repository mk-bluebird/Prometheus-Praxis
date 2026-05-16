-- filename: dbpromptshard.sql
-- destination: ecorestorationshard/db/dbpromptshard.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 1. Prompt shard table schema
--    Makes sovereign prompts first-class governance artifacts.

CREATE TABLE IF NOT EXISTS prompt_shard (
    promptid        INTEGER PRIMARY KEY AUTOINCREMENT,

    logicalname     TEXT    NOT NULL,   -- e.g. prompt.sovereign.governance.phoenix.2026v1
    versiontag      TEXT    NOT NULL,   -- e.g. 2026v1
    region          TEXT    NOT NULL,   -- e.g. Phoenix-AZ
    scope           TEXT    NOT NULL,   -- REGION, CONSTELLATION, NODE

    repofile_id     INTEGER NOT NULL,   -- back-reference into repofile
    ecoscope        TEXT    NOT NULL,   -- e.g. RESTORATION_CORE, AGENT_PROMPT
    lane_band       TEXT    NOT NULL,   -- RESEARCH, EXPPROD, PROD, GOV
    consumer_kind   TEXT    NOT NULL,   -- AI_CHAT, AGENT_API, HUMAN, CI

    bostromaddress  TEXT    NOT NULL,   -- author identity (full address text)
    contractid      TEXT    NOT NULL,   -- bound governance contract/logical contract id

    prompt_text     TEXT    NOT NULL,   -- full natural-language prompt
    equation_text   TEXT,               -- e.g. "eta = Δkarma / (Δmass + 1)"
    freedom_stance  TEXT,               -- description of neurorights / anti-coercion stance

    createdutc      TEXT    NOT NULL,
    updatedutc      TEXT    NOT NULL,

    UNIQUE (logicalname, versiontag),
    FOREIGN KEY (repofile_id)
        REFERENCES repofile(fileid)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_prompt_shard_logical
    ON prompt_shard (logicalname, region, scope);

CREATE INDEX IF NOT EXISTS idx_prompt_shard_consumer
    ON prompt_shard (consumer_kind, lane_band);
