# Phoenix Heat Island Calibration (city_os/phoenix/heat_island)

## Purpose

This module calibrates a Phoenix-specific urban heat island (UHI) model using
Landsat-8 Level-2 land surface temperature (LST) and spectral indices, aggregated
onto the city-wide hexagonal grid. The calibrated parameters (α, β, γ) are used
by Aletheion to prioritize cooling interventions (trees, materials, hydrology)
at the hex level.

- **α**: marginal change in UHI (°C) per unit NDVI (vegetation).
- **β**: marginal change in UHI (°C) per unit NDBI (built-up / materials).
- **γ**: marginal change in UHI (°C) per unit NDWI (water / moisture).

## Dataflow Overview

1. **Google Earth Engine (GEE) aggregation**

   - Input:
     - Phoenix hex grid FeatureCollection (`users/<user>/phoenix_hex_grid`).
   - Process:
     - Load `LANDSAT/LC08/C02/T1_L2` (surface reflectance + LST).[81][91]
     - Filter for summer scenes with low cloud cover.
     - Apply QA-based cloud masking.
     - Compute NDVI, NDBI, NDWI, and LST_C for each pixel.[81][86][89]
     - Build a median composite over the calibration window.
     - Aggregate composite metrics to each hex (mean, median, stdDev).
   - Output:
     - `phoenix_hex_landsat_stats_YYYY_season.geojson` exported to Google Drive.

2. **Regression and calibration**

   - Input:
     - Hex-level GeoJSON from GEE.
     - Phoenix hex grid reference + city OS layers for roof/tree/hydrology feasibility.
   - Process:
     - Identify rural hexes (low NDBI, moderate NDVI).
     - Compute rural reference LST and UHI for each hex.
     - Fit regularized regression:

       ```text
       UHI_h = θ0 + θV * NDVI_h + θB * NDBI_h + θW * NDWI_h + ε_h
       ```

     - Derive α = θV, β = θB, γ = θW with confidence intervals.
     - Compute per-hex feasibility factors from city OS layers.
   - Output:
     - `data/calibration_params.json` – α, β, γ, model metrics, rural reference.
     - `data/hex_metrics.json` – per-hex UHI, NDVI, NDBI, NDWI, feasibility factors.

3. **ALN shard and Rust crate**

   - ALN:
     - `hex_landsat_calibration.aln` binds α, β, γ and scoring functions to the
       Phoenix hex lattice and data lake.
   - Rust:
     - Crate `phoenix_heat_island` provides deterministic functions to compute
       ΔT under intervention scenarios and priority scores for cooling investments.

## JSON Formats

### calibration_params.json

```json
{
  "city": "Phoenix-AZ",
  "dataset": "LANDSAT/LC08/C02_T1_L2",
  "season": "2020-Summer",
  "hex_grid_ref": "city_os/phoenix/hex_grid/v1",
  "regression_method": "ridge_UHI_vs_NDVI_NDBI_NDWI",
  "parameters": {
    "alpha": { "estimate": -3.2, "ci_low": -4.0, "ci_high": -2.5, "units": "degC_per_unit_NDVI" },
    "beta":  { "estimate":  2.1, "ci_low":  1.5, "ci_high":  2.8, "units": "degC_per_unit_NDBI" },
    "gamma": { "estimate": -1.4, "ci_low": -2.0, "ci_high": -0.8, "units": "degC_per_unit_NDWI" }
  },
  "model_metrics": {
    "r2": 0.68,
    "rmse_degC": 1.7,
    "n_hex_train": 1200,
    "n_hex_valid": 300
  },
  "rural_reference": {
    "lst_c_median": 36.5,
    "selection_rule": "NDBI < 0.0 AND NDVI between 0.1 and 0.3"
  }
}
```

### hex_metrics.json

```json
[
  {
    "hex_id": "PHX_000001",
    "mean_lst_c": 42.3,
    "median_lst_c": 42.0,
    "stddev_lst_c": 1.2,
    "mean_ndvi": 0.12,
    "median_ndvi": 0.10,
    "stddev_ndvi": 0.05,
    "mean_ndbi": 0.28,
    "median_ndbi": 0.27,
    "stddev_ndbi": 0.03,
    "mean_ndwi": -0.05,
    "median_ndwi": -0.04,
    "stddev_ndwi": 0.02,
    "uhi": 5.8,
    "roof_area_fraction": 0.55,
    "feasible_tree_factor": 0.70,
    "hydrology_feasibility": 0.40
  }
]
```

## Rust API (phoenix_heat_island crate)

The Rust crate exposes the following core types and functions:

```rust
/// Calibration parameters (α, β, γ).
pub struct CalibrationParams {
    pub alpha: ParameterEstimate,
    pub beta: ParameterEstimate,
    pub gamma: ParameterEstimate,
}

pub struct ParameterEstimate {
    pub estimate: f64,
    pub ci_low: f64,
    pub ci_high: f64,
    pub units: &'static str,
}

/// Hex-level metrics for UHI scoring.
pub struct HexMetrics {
    pub hex_id: String,
    pub uhi: f64,
    pub ndvi: f64,
    pub ndbi: f64,
    pub ndwi: f64,
    pub roof_area_fraction: f64,
    pub feasible_tree_factor: f64,
    pub hydrology_feasibility: f64,
}

/// Cooling offset under intervention deltas (ΔNDVI, ΔNDBI, ΔNDWI).
pub fn delta_temperature(
    params: &CalibrationParams,
    delta_ndvi: f64,
    delta_ndbi: f64,
    delta_ndwi: f64,
) -> f64;

/// Priority score for tree canopy interventions.
pub fn tree_priority(
    params: &CalibrationParams,
    metrics: &HexMetrics,
    ndvi_target_max: f64,
) -> f64;

/// Priority score for cool roof interventions.
pub fn roof_priority(params: &CalibrationParams, metrics: &HexMetrics) -> f64;

/// Priority score for water/hydrology interventions.
pub fn water_priority(params: &CalibrationParams, metrics: &HexMetrics) -> f64;
```

## Integration into Aletheion

Aletheion’s planning functions should:

1. Load `calibration_params.json` and `hex_metrics.json` for the current
   calibration season.
2. For each hex:
   - Compute tree, roof, and water priority scores using the Rust crate.
   - Select intervention mix according to available budget, equity rules,
     and infrastructure constraints.
3. Use the hex graph topology to:
   - Break up hot corridors.
   - Strengthen and connect cool refuges.
   - Place cooling infrastructure where α, β, γ indicate the greatest UHI
     reduction per unit investment.

This module is designed to be composable with other city_os components and to
be re-run whenever new Landsat data or updated city layers are available.
