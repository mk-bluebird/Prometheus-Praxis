-- filename db/dblanecowealtheducation.sql

CREATE VIEW IF NOT EXISTS vstearducowealth AS
WITH base AS (
    SELECT
        s.stewardid,
        s.region,
        kw.kshort,  kw.eshort,  kw.rshort,
        kw.kmedium, kw.emedium, kw.rmedium,
        kw.klong,   kw.elong,   kw.rlong,
        -- physical delta E over long window
        (kw.elong - kw.eshort) AS delta_e_phys,
        -- education multiplier from StewardKnowledgeState
        sks.knowledgemultiplier AS mk_education
    FROM steward_window_ker AS kw
    JOIN stewardknowledgestate AS sks
      ON sks.stewarddid = kw.stewardid
    JOIN steward_region_map AS s
      ON s.stewardid = kw.stewardid
)
SELECT
    stewardid,
    region,
    kshort, eshort, rshort,
    kmedium, emedium, rmedium,
    klong,   elong,   rlong,
    delta_e_phys,
    mk_education
FROM base;
