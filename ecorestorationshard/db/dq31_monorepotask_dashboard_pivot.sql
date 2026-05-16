-- filename: dq31_monorepotask_dashboard_pivot.sql
-- destination: ecorestorationshard/db/dq31_monorepotask_dashboard_pivot.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

-- 31. Task counts by category and status for dashboard use.

SELECT
    category,
    status,
    COUNT(*) AS task_count
FROM monorepotask
GROUP BY category, status
ORDER BY category, status;
