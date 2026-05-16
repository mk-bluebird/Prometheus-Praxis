-- filename: db_evidencehex_upgrade.sql
-- destination: ecorestorationshard/db/db_evidencehex_upgrade.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

-- 44. Objection:
-- Many evidencehex fields are currently placeholders (e.g., all zeros),
-- providing no real proof of origin or linkage to commits, artifacts,
-- or Merkle trees. This undermines the tamper-evidence goal of the
-- artifact and identity registries.

-- Immediate next step: add a staging field and a view to track placeholders,
-- then backfill with commit-linked hashes without breaking existing bindings.

ALTER TABLE restorationidentitybinding
ADD COLUMN evidencehex_staged TEXT;

CREATE VIEW IF NOT EXISTS vrestorationidentitybinding_placeholder_evidence AS
SELECT *
FROM restorationidentitybinding
WHERE evidencehex IS NULL
   OR evidencehex = '0x00000000000000000000000000000000';

-- Backfill strategy (procedural, not executed here):
-- 1. For each row in vrestorationidentitybinding_placeholder_evidence:
--      - Compute a content hash (e.g., hash of the canonical SQL/ALN file at the
--        referenced filepath and a specific git commit).
--      - Store it in evidencehex_staged.
-- 2. After verification, run:
--      UPDATE restorationidentitybinding
--      SET evidencehex = evidencehex_staged,
--          evidencehex_staged = NULL
--      WHERE evidencehex_staged IS NOT NULL;
--
-- This preserves existing bindings (no deletions) while upgrading them to
-- commit- or Merkle-linked evidence, and CI can enforce that no new rows
-- are inserted with placeholder evidence.
