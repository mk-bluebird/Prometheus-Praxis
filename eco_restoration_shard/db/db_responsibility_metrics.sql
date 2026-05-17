-- filename: eco_restoration_shard/db/db_responsibility_metrics.sql
PRAGMA foreign_keys = ON;

-- ResponsibilityAxis metrics for shards (healthcare / vampiric / MT6883 lanes).
CREATE TABLE IF NOT EXISTS responsibilitymetric (
    shardid INTEGER NOT NULL,
    -- Aggregate responsibility coordinate r_responsibility in [0,1].
    rresponsibility REAL NOT NULL CHECK (rresponsibility >= 0.0 AND rresponsibility <= 1.0),
    -- Optional breakdown fields for auditability.
    rpharma REAL CHECK (rpharma >= 0.0 AND rpharma <= 1.0),
    rtoxicity REAL CHECK (rtoxicity >= 0.0 AND rtoxicity <= 1.0),
    roverride REAL CHECK (roverride >= 0.0 AND roverride <= 1.0),
    PRIMARY KEY (shardid),
    FOREIGN KEY (shardid) REFERENCES shardinstance(shardid)
);

-- Portfolio diversity metric per shard window (or portfolio-scope shard).
CREATE TABLE IF NOT EXISTS portfoliodiversitymetric (
    shardid INTEGER NOT NULL,
    -- r_portfolio_diversity in [0,1], higher is riskier (more concentrated).
    rportfoliodiversity REAL NOT NULL CHECK (rportfoliodiversity >= 0.0 AND rportfoliodiversity <= 1.0),
    PRIMARY KEY (shardid),
    FOREIGN KEY (shardid) REFERENCES shardinstance(shardid)
);
