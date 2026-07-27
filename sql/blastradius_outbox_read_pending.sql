-- filename: sql/blastradius_outbox_read_pending.sql
SELECT
    o.outbox_id,
    o.diagid,
    d.nodeid,
    d.fogregionid,
    d.timestamputc,
    d.rhydraulic,
    d.rbio,
    d.rtox,
    d.vtbefore,
    d.vtafter,
    d.deltavt,
    d.kfactor,
    d.efactor,
    d.rfactor,
    d.kerscore,
    d.lane,
    d.aln_hexstamp
FROM blastradius_outbox AS o
JOIN blastradiusdiag AS d
    ON d.diagid = o.diagid
WHERE o.status = 'PENDING'
ORDER BY o.outbox_id
LIMIT :limit;
