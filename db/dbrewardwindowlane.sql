-- filename: db/dbrewardwindowlane.sql
-- destination: Eco-Fort/db/dbrewardwindowlane.sql

CREATE VIEW IF NOT EXISTS rewardwindowlane AS
SELECT
  s.stewarddid,
  s.regioncode,
  s.kercontext,
  -- medium-term window identifiers
  s.epochstartutc    AS window_start_utc,
  s.epochendutc      AS window_end_utc,

  -- raw KER metrics from steward statement
  s.kmetric          AS k_raw,
  s.emetric          AS e_raw,
  s.rmetric          AS r_raw,

  -- trust and governance factors
  s.dataqualityfactor,
  s.uncertaintyfactor,
  s.governancepenalty,

  -- adjusted K and E after trust & governance
  (s.kmetric * s.dataqualityfactor)              AS k_adj,
  (s.emetric * s.dataqualityfactor
              * s.uncertaintyfactor
              * s.governancepenalty)            AS e_adj,

  -- lane thresholds (joined from lanethresholds medium-term band)
  lt.k_min_medium,
  lt.e_min_medium,
  lt.r_max_medium,

  -- eligibility flags
  CASE
    WHEN (s.kmetric * s.dataqualityfactor) >= lt.k_min_medium
     AND (s.emetric * s.dataqualityfactor
                    * s.uncertaintyfactor
                    * s.governancepenalty) >= lt.e_min_medium
     AND s.rmetric <= lt.r_max_medium
    THEN 1 ELSE 0
  END AS reward_medium_eligible
FROM stewardecowealthstatement AS s
JOIN lanethresholds AS lt
  ON lt.lane = 'PROD'
 AND lt.window_kind = 'MEDIUM';
