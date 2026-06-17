-- filename: sql/chat/eco_chat_route_registry.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS eco_chat_route_registry (
  route_name              TEXT PRIMARY KEY,               -- e.g. 'chat-default', 'eco-risk-gov'
  description             TEXT NOT NULL,                  -- human-readable summary
  llm_provider_name       TEXT NOT NULL,                  -- e.g. 'qwen', 'perplexity'
  model_name              TEXT NOT NULL,                  -- e.g. 'Qwen/Qwen2.5-7B-Instruct'
  tools_csv               TEXT NOT NULL,                  -- comma-separated tool names
  retrieval_tool_name     TEXT,                           -- optional name for retrieval tool
  required_kfk_version    TEXT NOT NULL,                  -- e.g. 'KFK2026v1'
  residual_envelope_id    TEXT NOT NULL,                  -- e.g. 'RSEcoChatPhoenix2026v1'
  lane                    TEXT NOT NULL CHECK (
                             lane IN ('RESEARCH','EXPPROD','PROD','GOV')
                           ),
  non_actuating_only      INTEGER NOT NULL CHECK (
                             non_actuating_only IN (0,1)
                           ),
  active                  INTEGER NOT NULL CHECK (
                             active IN (0,1)
                           ),
  owner_did               TEXT NOT NULL,                  -- must match repo DID in validation
  created_utc             TEXT NOT NULL,                  -- ISO-8601
  updated_utc             TEXT NOT NULL                   -- ISO-8601
);

CREATE INDEX IF NOT EXISTS idx_eco_chat_route_active
  ON eco_chat_route_registry (active, lane);

CREATE INDEX IF NOT EXISTS idx_eco_chat_route_rse
  ON eco_chat_route_registry (residual_envelope_id);
