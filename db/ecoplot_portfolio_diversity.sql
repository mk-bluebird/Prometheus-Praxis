-- filename: db/ecoplot_portfolio_diversity.sql
-- destination: eco_restoration_shard/db/ecoplot_portfolio_diversity.sql

PRAGMA foreign_keys = ON;

-------------------------------------------------------------------------------
-- 1. Adjacency and blastradius encoding for portfolio diversity
-------------------------------------------------------------------------------

-- Plot-to-plot adjacency graph with weights in [0,1].
-- adjacency_weight can encode spatial proximity or ecological coupling.
CREATE TABLE IF NOT EXISTS ecoplotadjacency (
    plot_a_id        INTEGER NOT NULL,
    plot_b_id        INTEGER NOT NULL,
    adjacency_weight REAL    NOT NULL CHECK (adjacency_weight >= 0.0 AND adjacency_weight <= 1.0),
    PRIMARY KEY (plot_a_id, plot_b_id)
);

CREATE INDEX IF NOT EXISTS idx_ecoplotadjacency_a
    ON ecoplotadjacency (plot_a_id);

CREATE INDEX IF NOT EXISTS idx_ecoplotadjacency_b
    ON ecoplotadjacency (plot_b_id);

-- Optional: per-plot blastradius summary, used when normalising adjacency
-- into effective archetype reach.
CREATE TABLE IF NOT EXISTS ecoplotblastradius (
    plot_id          INTEGER PRIMARY KEY,
    radius_meters    REAL    NOT NULL CHECK (radius_meters >= 0.0),
    radius_hops      INTEGER NOT NULL CHECK (radius_hops >= 0),
    radius_time_h    REAL    NOT NULL CHECK (radius_time_h >= 0.0)
);

-------------------------------------------------------------------------------
-- 2. Plot metadata and archetype assignment (consumed by diversity views)
-------------------------------------------------------------------------------

-- Plots table must exist elsewhere; minimal projection shown here.
-- Each plot carries an archetype label and area used for weighting.
CREATE TABLE IF NOT EXISTS ecoplot (
    plot_id      INTEGER PRIMARY KEY,
    archetype    TEXT    NOT NULL,           -- e.g. HYDROBUFFER, BUGSLIFESUBSTRATE
    area_ha      REAL    NOT NULL CHECK (area_ha > 0.0),
    region       TEXT    NOT NULL,
    stewarddid   TEXT    NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_ecoplot_archetype
    ON ecoplot (archetype);

CREATE INDEX IF NOT EXISTS idx_ecoplot_region
    ON ecoplot (region);

-------------------------------------------------------------------------------
-- 3. Archetype share view for rportfolio_diversity
-------------------------------------------------------------------------------

-- v_archetype_weights yields normalised archetype weights per region and steward.
-- The Rust rportfolio_diversity kernel consumes the "weight" column as p_i.
CREATE VIEW IF NOT EXISTS v_archetype_weights AS
SELECT
    p.region              AS region,
    p.stewarddid          AS stewarddid,
    p.archetype           AS archetype,
    SUM(p.area_ha)        AS total_area_ha,
    SUM(p.area_ha)
        / SUM(SUM(p.area_ha)) OVER (PARTITION BY p.region, p.stewarddid)
                            AS weight
FROM ecoplot AS p
GROUP BY
    p.region,
    p.stewarddid,
    p.archetype;

-------------------------------------------------------------------------------
-- 4. Workspace Cargo manifest for kerresidual and ecowealth crates
-------------------------------------------------------------------------------
