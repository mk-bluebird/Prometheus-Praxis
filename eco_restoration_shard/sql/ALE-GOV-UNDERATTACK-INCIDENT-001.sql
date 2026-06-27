-- ALE-GOV-UNDERATTACK-INCIDENT-001.sql
-- Sovereign governance: UNDERATTACK incident table + triggers (SQLite).

CREATE TABLE IF NOT EXISTS incident_underattack (
    incidentid           TEXT PRIMARY KEY,
    victimidentityid     TEXT NOT NULL,
    victimidentitytype   TEXT NOT NULL CHECK (victimidentitytype = 'AugmentedCitizen'),
    state                TEXT NOT NULL CHECK (state IN ('UNDERATTACK','UNDERREVIEW','NORMAL')),
    beforeSvictim        REAL NOT NULL,
    afterSvictim         REAL NOT NULL,
    beforeSattacker      REAL,
    afterSattacker       REAL,
    attackeridentityid   TEXT,
    attackerclusterid    TEXT,
    evidencehex          TEXT NOT NULL,
    created_at_s         INTEGER NOT NULL,
    updated_at_s         INTEGER NOT NULL
);

-- ΔS_victim = 0 whenever state = UNDERATTACK.
CREATE TRIGGER IF NOT EXISTS trg_underattack_clamp_victimS
BEFORE UPDATE ON incident_underattack
FOR EACH ROW
BEGIN
    -- Freeze victim S when UNDERATTACK.
    IF OLD.state = 'UNDERATTACK' THEN
        -- Hard clamp: NEW.afterSvictim forced to OLD.beforeSvictim.
        SET NEW.afterSvictim = OLD.beforeSvictim;

        -- Optionally enforce monotone attacker penalty: afterSattacker <= beforeSattacker.
        IF NEW.afterSattacker IS NOT NULL AND OLD.beforeSattacker IS NOT NULL
           AND NEW.afterSattacker > OLD.beforeSattacker THEN
            SET NEW.afterSattacker = OLD.beforeSattacker;
        END IF;
    END IF;
END;

-- Block any attempt to reduce victim S during UNDERATTACK.
CREATE TRIGGER IF NOT EXISTS trg_underattack_block_victim_downgrade
BEFORE UPDATE ON incident_underattack
FOR EACH ROW
BEGIN
    IF OLD.state = 'UNDERATTACK'
       AND NEW.afterSvictim < OLD.beforeSvictim THEN
        SELECT RAISE(ABORT, 'UNDERATTACK: ΔS_victim < 0 forbidden');
    END IF;
END;
