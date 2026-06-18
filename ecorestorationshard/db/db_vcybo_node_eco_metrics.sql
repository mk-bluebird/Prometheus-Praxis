-- filename: db_vcybo_node_eco_metrics.sql
-- destination: ecorestorationshard/db/db_vcybo_node_eco_metrics.sql
-- Purpose:
-- - Provide the vcybo_node_eco_metrics view required by econet_get_cybo_node_eco_metrics.
-- - Join Cyboquatic workload windows with blast-radius summary and node metadata.

PRAGMA foreign_keys = ON;

CREATE TABLE IF NOT EXISTS cybonode_metadata (
    nodeid         TEXT PRIMARY KEY,
    displayname    TEXT NOT NULL,
    region         TEXT NOT NULL,
    medium         TEXT NOT NULL,
    noderole       TEXT NOT NULL,
    machineryclass TEXT NOT NULL
);

CREATE VIEW IF NOT EXISTS vcybo_node_eco_metrics AS
SELECT
    n.nodeid,
    n.displayname,
    n.region,
    n.medium,
    n.noderole,
    n.machineryclass,
    w.windowstartutc,
    w.windowendutc,
    w.totalrequestsj,
    w.totalsurplusj,
    w.acceptfraction,
    w.meanvtbefore,
    w.meanvtafter,
    w.meandeltavt,
    w.meanrcarbon,
    w.meanrbiodiv,
    b.impacttype,
    b.impactscoresum,
    b.vtsensitivitymean,
    b.linkcount
FROM cybonode_metadata n
LEFT JOIN vcyboworkloadwindow w
    ON w.nodeid = n.nodeid
LEFT JOIN vshardblastradius b
    ON b.nodeid = n.nodeid;
