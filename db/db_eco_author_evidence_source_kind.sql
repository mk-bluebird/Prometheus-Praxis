-- filename db_eco_author_evidence_source_kind.sql
-- destination Eco-Fort/db/db_eco_author_evidence_source_kind.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 31. Controlled vocabulary for eco_author_evidence.source_kind
-------------------------------------------------------------------------------

-- Lookup table for valid source kinds
CREATE TABLE IF NOT EXISTS eco_author_source_kind (
    source_kind TEXT PRIMARY KEY,
    description TEXT NOT NULL
);

INSERT OR IGNORE INTO eco_author_source_kind (source_kind, description) VALUES
    ('GIT_COMMIT', 'Evidence derived from a Git commit.'),
    ('GIT_TAG', 'Evidence derived from a Git tag or annotated release.'),
    ('GITHUB_PR', 'Evidence derived from a GitHub pull request.'),
    ('CI_RUN', 'Evidence derived from a continuous integration run.'),
    ('MANUAL_ENTRY', 'Manually entered evidence by a reviewer.'),
    ('ALN_SHARD', 'Evidence embedded in an ALN qpudatashard.'),
    ('DOC_ATTACHMENT', 'Evidence derived from attached documentation.'),
    ('LEGACY_IMPORT', 'Evidence imported from a legacy system.');

-- eco_author_evidence assumed schema (excerpt):
--   source_kind TEXT NOT NULL

-- Enforce controlled vocabulary via foreign key
ALTER TABLE eco_author_evidence
ADD COLUMN source_kind_fk TEXT;

UPDATE eco_author_evidence
SET source_kind_fk = source_kind;

ALTER TABLE eco_author_evidence
ADD CONSTRAINT fk_eco_author_source_kind
FOREIGN KEY (source_kind_fk)
REFERENCES eco_author_source_kind(source_kind);

-- Optionally, drop or ignore the old free-form column in future migrations.
