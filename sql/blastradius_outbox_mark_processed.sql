-- filename: sql/blastradius_outbox_mark_processed.sql
BEGIN IMMEDIATE;

UPDATE blastradiusdiag
SET aln_hexstamp = :aln_hexstamp
WHERE diagid = :diagid
  AND aln_hexstamp IS NULL;

UPDATE blastradius_outbox
SET status        = 'PROCESSED',
    processed_utc = strftime('%Y-%m-%dT%H:%M:%SZ', 'now'),
    last_error    = NULL
WHERE outbox_id = :outbox_id;

COMMIT;
