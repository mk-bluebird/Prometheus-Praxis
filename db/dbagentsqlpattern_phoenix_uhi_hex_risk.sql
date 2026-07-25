-- filename: db/dbagentsqlpattern_phoenix_uhi_hex_risk.sql
-- destination: https://github.com/mk-bluebird/Prometheus-Praxis/db/dbagentsqlpattern_phoenix_uhi_hex_risk.sql
-- license: MIT OR Apache-2.0

PRAGMA foreign_keys = ON;

BEGIN TRANSACTION;

-- Register a standard SQL pattern for listing UHI hex risk shards for Phoenix.
-- This pattern gives AI-chat tools a ready-made query so they never invent SQL.

INSERT OR IGNORE INTO agentsqlpattern (
    pattern_id,
    repo_slug,
    description,
    sql_text,
    lane_scope,
    risk_ceiling_note
) VALUES (
    'phoenix_uhi_hex_risk_list',
    'mk-bluebird/Prometheus-Praxis',
    'List Phoenix UHI hex risk shards with triad coordinates and composite thermal risk, ordered by r_thermal descending.',
    '
    SELECT
        hex_id,
        r_t,
        r_c,
        r_a,
        r_thermal,
        r_hyd,
        r_energy,
        r_biodiv,
        r_ai
    FROM
        ecoshard_phoenix_uhi_hex_risk
    WHERE
        region = ''Phoenix-AZ''
    ORDER BY
        r_thermal DESC;
    ',
    'RESEARCH',
    'Thermal risk only; non-offsettable corridors for biodiversity/carbon/neurorights are enforced at shard emission time.'
);

COMMIT;
