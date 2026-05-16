-- filename: dq33_mt6883_neuroethic_breach_nodes.sql
-- destination: ecorestorationshard/db/dq33_mt6883_neuroethic_breach_nodes.sql
-- repo-target: github.com/mk-bluebird/eco_restoration_shard

-- 33. Phoenix nodes where neuroethic radius is breached, via mt6883registryid.

SELECT
    b.region,
    b.domain,
    b.scopeid,
    b.planeid,
    b.neuroethicradiushours AS blast_neuroethic_radius_hours,
    l.kernelid,
    l.lane,
    l.mt6883registryid,
    l.neuroethicradiushours AS lane_neuroethic_radius_hours,
    l.neuroethicok,
    l.kscore,
    l.escore,
    l.rscore,
    l.vtmax
FROM blastradiusindex AS b
JOIN lanestatusshard  AS l
  ON l.mt6883registryid = b.mt6883registryid
WHERE b.region = 'Phoenix-AZ'
  AND b.neuroethicradiushours IS NOT NULL
  AND (
        l.neuroethicok = 0
     OR b.neuroethicradiushours > l.neuroethicradiushours
      )
ORDER BY b.region, b.domain, b.scopeid, l.kernelid;
