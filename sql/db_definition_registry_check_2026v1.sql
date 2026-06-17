-- Filename: sql/db_definition_registry_check_2026v1.sql
-- Destination: sql/db_definition_registry_check_2026v1.sql

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS ecofileindex (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    filename TEXT NOT NULL,
    destination TEXT NOT NULL,
    repotarget TEXT NOT NULL,
    contracthint TEXT NOT NULL,
    UNIQUE (filename, destination, repotarget)
);

INSERT INTO ecofileindex (filename, destination, repotarget, contracthint)
VALUES
    ('db_governance_core_2026v1.sql',
     'sql/db_governance_core_2026v1.sql',
     'mk-bluebird/eco_restoration_shard',
     'econet.db.governance.core.2026v1')
ON CONFLICT (filename, destination, repotarget) DO NOTHING;
