-- filename: econet_mcp/db/ecosphere_reward_index_map.sql
-- destination: EcoNet/econet_mcp/db/ecosphere_reward_index_map.sql
-- purpose: index, map, and prewire this reward surface into the EcoNet constellation routing DB

PRAGMA foreign_keys = ON;

-- This table makes the reward schema discoverable for MCP / agent ecosystems
-- using the same DefinitionRegistry-style pattern already present in Eco-Fort.

CREATE TABLE IF NOT EXISTS ecosphere_definition_index (
    def_id          INTEGER PRIMARY KEY AUTOINCREMENT,
    logical_name    TEXT NOT NULL UNIQUE,   -- e.g. 'EcoSphere.RewardIndex.2026v1'
    repo_target     TEXT NOT NULL,          -- e.g. 'EcoNet', 'Eco-Fort'
    filepath        TEXT NOT NULL,          -- repo-relative path to .sql
    category        TEXT NOT NULL,          -- 'REWARD_SCHEMA', 'MCP_SURFACE'
    version_tag     TEXT NOT NULL,          -- e.g. '2026v1'
    frozen          INTEGER NOT NULL DEFAULT 1,
    CHECK (frozen IN (0,1))
);

INSERT OR IGNORE INTO ecosphere_definition_index (
    logical_name, repo_target, filepath, category, version_tag, frozen
) VALUES
    ('EcoSphere.RewardIndex.Schema.2026v1',
     'EcoNet',
     'econe t_mcp/db/ecosphere_reward_index.sql',
     'REWARD_SCHEMA',
     '2026v1',
     1),
    ('EcoSphere.RewardIndex.Map.2026v1',
     'EcoNet',
     'econe t_mcp/db/ecosphere_reward_index_map.sql',
     'MCP_SURFACE',
     '2026v1',
     1);
