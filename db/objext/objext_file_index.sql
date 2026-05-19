-- filename: db/objext/objext_file_index.sql
-- destination: ecorestorationshard/db/objext/objext_file_index.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS objext_file_index (
    file_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    filename       TEXT NOT NULL,
    destination    TEXT NOT NULL,
    repo_target    TEXT NOT NULL,
    logical_role   TEXT NOT NULL,  -- e.g. OBJEXT-MAPPING, OBJEXT-DOC, OBJEXT-RUST-HOST
    profile        TEXT NOT NULL,  -- e.g. research, exam, code-audit
    version_tag    TEXT NOT NULL,  -- e.g. v1.0.0
    shard_id       TEXT NOT NULL,  -- e.g. OBJEXT-MAP-V1
    created_utc    TEXT NOT NULL,
    updated_utc    TEXT NOT NULL,
    UNIQUE(filename, destination)
);

INSERT OR IGNORE INTO objext_file_index (
    filename,                           destination,                              repo_target,
    logical_role,   profile,   version_tag, shard_id,
    created_utc,    updated_utc
) VALUES
    (
        '100-objext-template.v1.md',
        'docs/100-objext-template.v1.md',
        'github.com/mk-bluebird/eco_restoration_shard',
        'OBJEXT-DOC', 'research', 'v1.0.0', 'OBJEXT-MAP-V1',
        datetime('now'), datetime('now')
    ),
    (
        '100-objext.mapping.v1.aln',
        'schemas/objext/100-objext.mapping.v1.aln',
        'github.com/mk-bluebird/eco_restoration_shard',
        'OBJEXT-MAPPING', 'research', 'v1.0.0', 'OBJEXT-MAP-V1',
        datetime('now'), datetime('now')
    );
