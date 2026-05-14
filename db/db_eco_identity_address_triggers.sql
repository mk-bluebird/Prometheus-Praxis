-- filename db_eco_identity_address_triggers.sql
-- destination Eco-Fort/db/db_eco_identity_address_triggers.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 2. eco_identity_address primary-address uniqueness trigger
-------------------------------------------------------------------------------

-- Assumes eco_identity_address(person_id, address, address_type, is_primary)

CREATE TRIGGER IF NOT EXISTS trg_eco_identity_address_primary_ins
AFTER INSERT ON eco_identity_address
WHEN NEW.is_primary = 1
BEGIN
    UPDATE eco_identity_address
    SET is_primary = 0
    WHERE person_id = NEW.person_id
      AND address_id <> NEW.address_id;
END;

CREATE TRIGGER IF NOT EXISTS trg_eco_identity_address_primary_upd
AFTER UPDATE OF is_primary ON eco_identity_address
WHEN NEW.is_primary = 1
BEGIN
    UPDATE eco_identity_address
    SET is_primary = 0
    WHERE person_id = NEW.person_id
      AND address_id <> NEW.address_id;
END;
