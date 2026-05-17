-- filename: db/dbplaneweightscore7.sql
-- destination: Eco-Fort/db/dbplaneweightscore7.sql

WITH core AS (
  SELECT
    pw.contractid,
    MAX(CASE WHEN UPPER(pw.planecode) = 'CARBON'       THEN pw.weight END) AS w_carbon,
    MAX(CASE WHEN UPPER(pw.planecode) = 'BIODIVERSITY' THEN pw.weight END) AS w_biodiversity,
    MAX(CASE WHEN UPPER(pw.planecode) = 'ENERGY'       THEN pw.weight END) AS w_energy,
    MAX(CASE WHEN UPPER(pw.planecode) = 'MATERIALS'    THEN pw.weight END) AS w_materials,
    MAX(CASE WHEN UPPER(pw.planecode) = 'HYDROLOGY'    THEN pw.weight END) AS w_hydrology,
    MAX(CASE WHEN UPPER(pw.planecode) = 'DATAQUALITY'  THEN pw.weight END) AS w_dataquality,
    MAX(CASE WHEN UPPER(pw.planecode) = 'TOPOLOGY'     THEN pw.weight END) AS w_topology
  FROM planeweightsplane AS pw
  GROUP BY pw.contractid
)
SELECT
  contractid,
  w_carbon,
  w_biodiversity,
  w_energy,
  w_materials,
  w_hydrology,
  w_dataquality,
  w_topology
FROM core;
