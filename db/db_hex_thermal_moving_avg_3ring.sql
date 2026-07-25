-- filename: db/db_hex_thermal_moving_avg_3ring.sql

SELECT
    center.hex_id AS center_hex,
    AVG(neigh.r_thermal) AS avg_r_thermal_3ring
FROM ecoshard_phoenix_uhi_hex_risk AS center
JOIN ecoshard_phoenix_uhi_hex_risk AS neigh
  ON h3_distance(center.hex_id, neigh.hex_id) <= 3
GROUP BY center.hex_id;
