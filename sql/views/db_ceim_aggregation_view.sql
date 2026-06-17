-- filename db_ceim_aggregation_view.sql
-- destination eco_restoration_shard/sql/views/db_ceim_aggregation_view.sql
PRAGMA foreign_keys = ON;

CREATE VIEW IF NOT EXISTS v_ceim_ecoimpact_by_region AS
SELECT
    region,
    contaminant,
    COUNT(*)               AS n_windows,
    SUM(kn)                AS kn_total,
    AVG(ecoimpactscore)    AS ecoimpact_avg,
    SUM(nanokarmabytes)    AS nanokarma_total
FROM ceim_xj_node_result
GROUP BY region, contaminant;
