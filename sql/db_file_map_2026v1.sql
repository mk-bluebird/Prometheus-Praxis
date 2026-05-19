-- filename: db_file_map_2026v1.sql
-- destination: eco_restoration_shard/sql/db_file_map_2026v1.sql

PRAGMA foreign_keys = ON ;

----------------------------------------------------------------------
-- Minimal file-index table for Ecological-Order prewiring
----------------------------------------------------------------------

CREATE TABLE IF NOT EXISTS eco_file_index (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    filename      TEXT NOT NULL,
    destination   TEXT NOT NULL,
    repo_target   TEXT NOT NULL,
    contract_hint TEXT NOT NULL,
    UNIQUE (filename, destination, repo_target)
);

INSERT INTO eco_file_index (filename, destination, repo_target, contract_hint)
VALUES
    ('aln_grammar_core_2026v1.ebnf',
     'spec/aln/aln_grammar_core_2026v1.ebnf',
     'mk-bluebird/eco_restoration_shard',
     'ALNGrammarCore2026v1'
    ),
    ('aln_grammar_core_2026v1.bnf',
     'spec/aln/aln_grammar_core_2026v1.bnf',
     'mk-bluebird/eco_restoration_shard',
     'ALNGrammarCore2026v1'
    ),
    ('aln_telemetry_kernel_2026v1.ebnf',
     'spec/aln/aln_telemetry_kernel_2026v1.ebnf',
     'mk-bluebird/eco_restoration_shard',
     'ALNTelemetryKernel2026v1'
    ),
    ('db_grammar_index_2026v1.sql',
     'sql/db_grammar_index_2026v1.sql',
     'mk-bluebird/eco_restoration_shard',
     'ALNGrammarIndex2026v1'
    ),
    ('db_file_map_2026v1.sql',
     'sql/db_file_map_2026v1.sql',
     'mk-bluebird/eco_restoration_shard',
     'EcologicalOrderFileIndex2026v1'
    )
ON CONFLICT (filename, destination, repo_target) DO NOTHING;
