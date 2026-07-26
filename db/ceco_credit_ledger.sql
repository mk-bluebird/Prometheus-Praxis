-- filename: db/ceco_credit_ledger.sql
CREATE TABLE IF NOT EXISTS cecocredits (
    creditid UUID PRIMARY KEY,
    recordid UUID NOT NULL REFERENCES ecoperjoulelogs(recordid),
    ceco DOUBLE PRECISION NOT NULL,
    mintedat TIMESTAMPTZ NOT NULL DEFAULT now(),
    shardid UUID NOT NULL,
    did TEXT NOT NULL,        -- signing DID
    hexanchor TEXT NOT NULL   -- hex stamp of phoenix.uhi.hex.risk.v1 shard
);

CREATE INDEX IF NOT EXISTS idx_ceco_record ON cecocredits(recordid);
