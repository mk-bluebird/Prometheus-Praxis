-- ALE-GOV-AUGCIT-DISQUALIFICATION-REVIEW-001.sql
-- AugmentedCitizenDisqualificationReview shard with dual-human Merkle root.

CREATE TABLE IF NOT EXISTS AugmentedCitizenDisqualificationReview (
    caseid                  TEXT PRIMARY KEY,
    identityid              TEXT NOT NULL,
    identitytype            TEXT NOT NULL CHECK (identitytype = 'AugmentedCitizen'),
    datasensitivity         TEXT NOT NULL CHECK (datasensitivity IN ('HIGH','MEDIUM','LOW')),
    allegedharmnodeid       TEXT NOT NULL,
    K_before                REAL NOT NULL,
    K_after                 REAL NOT NULL,
    ecoimpact_before        REAL NOT NULL,
    ecoimpact_after         REAL NOT NULL,
    reviewer1did_hashhex    TEXT NOT NULL,
    reviewer2did_hashhex    TEXT NOT NULL,
    reviewer1sighex         TEXT NOT NULL,
    reviewer2sighex         TEXT NOT NULL,
    ceim_window_hashhex     TEXT NOT NULL,
    finalverdict            TEXT NOT NULL CHECK (finalverdict IN ('DISQUALIFY','RETAIN')),
    final_sighex            TEXT NOT NULL,
    created_at_s            INTEGER NOT NULL
);

-- Invariant: DISQUALIFY requires dual reviewers + CEIM harm window.
CREATE TRIGGER IF NOT EXISTS trg_augcit_disqualify_invariants
BEFORE INSERT ON AugmentedCitizenDisqualificationReview
FOR EACH ROW
BEGIN
    -- Require dual reviewer signatures and CEIM window hash.
    IF NEW.finalverdict = 'DISQUALIFY' THEN
        IF NEW.reviewer1sighex IS NULL OR NEW.reviewer2sighex IS NULL
           OR NEW.ceim_window_hashhex IS NULL THEN
            SELECT RAISE(ABORT, 'DISQUALIFY requires dual signatures + CEIM window hash');
        END IF;
    END IF;

    -- K_after may not drop below 0.8 unless DISQUALIFY with proven harm.
    IF NEW.K_after < 0.8 AND NEW.finalverdict != 'DISQUALIFY' THEN
        SELECT RAISE(ABORT, 'Karma floor downgrade without DISQUALIFY forbidden');
    END IF;
END;
