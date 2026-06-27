-- ALE-GOV-AUGCIT-KARMA-001.sql
-- AugmentedCitizen Karma floor and karmatolerance invariants.

CREATE TABLE IF NOT EXISTS augmented_citizen_karma (
    identityid            TEXT PRIMARY KEY,
    identitytype          TEXT NOT NULL CHECK (identitytype = 'AugmentedCitizen'),
    ProtectedStakeholder  INTEGER NOT NULL CHECK (ProtectedStakeholder IN (0,1)),
    DataSensitivity       TEXT NOT NULL CHECK (DataSensitivity IN ('HIGH','MEDIUM','LOW')),
    EvidenceMode          TEXT NOT NULL CHECK (EvidenceMode IN ('HASHONLY','FULL')),
    currentkarma          REAL NOT NULL,
    karmatolerancelevel   TEXT NOT NULL CHECK (karmatolerancelevel IN ('LOW','MEDIUM','HIGH')),
    provenharm            INTEGER NOT NULL CHECK (provenharm IN (0,1)),
    updated_reason        TEXT NOT NULL,
    updated_at_s          INTEGER NOT NULL
);

-- Hard floor: currentkarma >= 0.8 for AugmentedCitizen unless provenharm = TRUE.
CREATE TRIGGER IF NOT EXISTS trg_augcit_karma_floor
BEFORE UPDATE ON augmented_citizen_karma
FOR EACH ROW
BEGIN
    IF OLD.identitytype = 'AugmentedCitizen'
       AND NEW.provenharm = 0
       AND NEW.currentkarma < 0.8 THEN
        SELECT RAISE(ABORT, 'AugmentedCitizen Karma floor rollback forbidden');
    END IF;

    -- Karmatolerance cannot be downgraded without proven harm.
    IF OLD.identitytype = 'AugmentedCitizen'
       AND NEW.provenharm = 0
       AND (NEW.karmatolerancelevel = 'LOW' AND OLD.karmatolerancelevel IN ('MEDIUM','HIGH')
            OR NEW.karmatolerancelevel = 'MEDIUM' AND OLD.karmatolerancelevel = 'HIGH') THEN
        SELECT RAISE(ABORT, 'AugmentedCitizen karmatolerance downgrade forbidden');
    END IF;

    -- Data absence / missing evidence may not drive negative updates.
    IF OLD.identitytype = 'AugmentedCitizen'
       AND OLD.ProtectedStakeholder = 1
       AND OLD.DataSensitivity = 'HIGH'
       AND NEW.updated_reason IN ('DATAABSENCE','EVIDENCE_MISSING')
       AND NEW.currentkarma < OLD.currentkarma THEN
        SELECT RAISE(ABORT, 'Data absence cannot reduce AugmentedCitizen Karma');
    END IF;
END;
