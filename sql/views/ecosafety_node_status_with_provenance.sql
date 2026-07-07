-- View: EcosafetyNodeStatusWithProvenance
-- Exposes node ecosafety_status, cov_condition_number, vt_at_window_end,
-- lane, kerdeployable, and a biodiversity_warn flag derived from provenance.

CREATE VIEW IF NOT EXISTS EcosafetyNodeStatusWithProvenance AS
WITH
  -- Covariance provenance per envelope (kind = "covariance")
  CovProv AS (
    SELECT
      p.nodeid,
      p.region,
      p.window_start_utc,
      p.window_end_utc,
      json_extract(p.detail_json, '$.ecosafety_status') AS ecosafety_status_from_prov,
      json_extract(p.detail_json, '$.ecosafety_distance') AS ecosafety_distance_from_prov,
      json_extract(p.detail_json, '$.cov_condition_number') AS cov_condition_number_from_prov
    FROM ecosafety_provenance p
    WHERE json_extract(p.detail_json, '$.kind') = 'covariance'
  ),

  -- Biodiversity provenance per envelope (kind = "biodiversity")
  BiodivProv AS (
    SELECT
      p.nodeid,
      p.region,
      p.window_start_utc,
      p.window_end_utc,
      json_extract(p.detail_json, '$.warn') AS biodiversity_warn,
      json_extract(p.detail_json, '$.r_biodiv_mean') AS r_biodiv_mean_from_prov,
      json_extract(p.detail_json, '$.r_biodiv_threshold') AS r_biodiv_threshold_from_prov
    FROM ecosafety_provenance p
    WHERE json_extract(p.detail_json, '$.kind') = 'biodiversity'
  )

SELECT
  e.nodeid,
  e.region,
  e.medium,
  e.window_start_utc,
  e.window_end_utc,

  -- Primary ecosafety fields from envelope
  e.ecosafety_status AS ecosafety_status_env,
  e.cov_condition_number AS cov_condition_number_env,
  e.vt_at_window_end,
  e.lane,
  e.kerdeployable,

  -- Covariance details from provenance (if present)
  CovProv.ecosafety_status_from_prov,
  CovProv.ecosafety_distance_from_prov,
  CovProv.cov_condition_number_from_prov,

  -- Biodiversity flags and metrics from provenance
  BiodivProv.biodiversity_warn,
  BiodivProv.r_biodiv_mean_from_prov,
  BiodivProv.r_biodiv_threshold_from_prov

FROM ecosafety_envelope e
LEFT JOIN CovProv
  ON  CovProv.nodeid = e.nodeid
  AND CovProv.region = e.region
  AND CovProv.window_start_utc = e.window_start_utc
  AND CovProv.window_end_utc = e.window_end_utc
LEFT JOIN BiodivProv
  ON  BiodivProv.nodeid = e.nodeid
  AND BiodivProv.region = e.region
  AND BiodivProv.window_start_utc = e.window_start_utc
  AND BiodivProv.window_end_utc = e.window_end_utc;
