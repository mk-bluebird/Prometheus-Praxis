-- filename db_eco_migration_views.sql
-- destination eco_restoration_shard/db/db_eco_migration_views.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. View for legacy-to-current migration and Bostrom addresses
-------------------------------------------------------------------------------

CREATE VIEW IF NOT EXISTS v_legacy_to_current_migration AS
WITH legacy_person AS (
    SELECT
        p.person_id,
        p.legacy_login,
        p.current_login
    FROM eco_identity_person p
),
address_group AS (
    SELECT
        a.person_id,
        GROUP_CONCAT(a.address, ',') AS bostrom_addresses
    FROM eco_identity_address a
    WHERE a.address_type = 'BOSTROM'
    GROUP BY a.person_id
)
SELECT
    m.migration_id,
    m.legacy_login        AS legacy_github_login,
    m.current_login       AS current_github_login,
    lp.person_id          AS person_id,
    ag.bostrom_addresses  AS bostrom_addresses,
    m.created_utc         AS migration_created_utc,
    m.revoked             AS migration_revoked
FROM eco_github_migration m
LEFT JOIN legacy_person lp
    ON lp.legacy_login = m.legacy_login
LEFT JOIN address_group ag
    ON ag.person_id = lp.person_id;
