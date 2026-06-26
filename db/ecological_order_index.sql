-- filename: ecological_order_index.sql
-- repo: mk-bluebird/eco_restoration_shard
-- destination_hint: PATH_TO_BE_CHOSEN_IN_REPO_ROOT

CREATE TABLE IF NOT EXISTS ecological_order_index (
    id INTEGER PRIMARY KEY,
    filename TEXT NOT NULL,
    repo TEXT NOT NULL CHECK (
        repo IN (
            'mk-bluebird/Virta-Sys',
            'mk-bluebird/Data_Lake',
            'mk-bluebird/EcoNet',
            'mk-bluebird/Eco-Fort',
            'mk-bluebird/ALN-Blockchain',
            'mk-bluebird/eco_restoration_shard',
            'mk-bluebird/Augmented-Citizen',
            'mk-bluebird/Cyboquatics'
        )
    ),
    destination_hint TEXT NOT NULL,
    primary_role TEXT NOT NULL,
    language TEXT NOT NULL CHECK (
        language IN (
            'ALN','Rust','C++','Kotlin','Java','Lua','SQL','CSV','JSON','NDJSON','Mermaid'
        )
    ),
    brain_identity_relevance INTEGER NOT NULL,
    eco_impact_focus TEXT NOT NULL,
    UNIQUE(filename, repo)
);
