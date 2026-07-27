-- filename: sql/blastradius_outbox_mark_failed.sql
BEGIN IMMEDIATE;

UPDATE blastradius_outbox
SET status     = 'FAILED',
    last_error = :last_error
WHERE outbox_id = :outbox_id;

COMMIT;
