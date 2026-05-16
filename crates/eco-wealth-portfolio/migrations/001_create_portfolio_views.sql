-- crates/eco-wealth-portfolio/migrations/001_create_portfolio_views.sql

CREATE MATERIALIZED VIEW eco_wealth_portfolio_region AS
SELECT
    nfa.region_id,
    SUM(nfa.tree_biomass_kg) AS total_tree_biomass,
    AVG(nfa.shade_canopy_cover_pct) AS avg_canopy,
    SUM(nfa.pollinator_habitat_score) AS total_pollinator_score,
    AVG(nfa.thermal_comfort_index) AS avg_thermal_comfort,
    jsonb_object_agg(nfa.asset_type, nfa.value) AS other_assets
FROM non_financial_assets AS nfa
GROUP BY nfa.region_id;

CREATE MATERIALIZED VIEW eco_wealth_portfolio_by_steward AS
SELECT
    ls.steward_did,
    nfa.region_id,
    SUM(nfa.tree_biomass_kg) AS total_tree_biomass,
    AVG(nfa.shade_canopy_cover_pct) AS avg_canopy,
    SUM(nfa.pollinator_habitat_score) AS total_pollinator_score,
    AVG(nfa.thermal_comfort_index) AS avg_thermal_comfort,
    jsonb_object_agg(nfa.asset_type, nfa.value) AS other_assets
FROM non_financial_assets AS nfa
JOIN land_stewardship AS ls
  ON nfa.parcel_id = ls.parcel_id
GROUP BY ls.steward_did, nfa.region_id;
