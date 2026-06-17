-- FILE: ecorestorationshard/sqlecowealth/dbstewardecowealthvshard.sql
-- DESTINATION: ecorestorationshard/sqlecowealth/dbstewardecowealthvshard.sql
-- REPO-TARGET: github.com/mk-bluebird/eco_restoration_shard

PRAGMA foreign_keys = ON;

CREATE VIEW IF NOT EXISTS vshardstewardecowealth AS
SELECT
    region,
    planecontractid,
    lane AS rewardclass,
    COUNT(*) AS nworkloads,
    AVG(ecounitfinal) AS avgecogain,
    AVG(vtmaxwindow) AS avgecoefficiency,
    AVG(kmean) AS kband,
    AVG(emean) AS eband,
    AVG(rmean) AS rband
FROM StewardEcoWealthStatement
GROUP BY region, planecontractid, lane;
