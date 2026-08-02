# From Calibration to Corridors: Using a Hexagonal Grid Model to Guide Cooling Infrastructure in Phoenix

## 1. Overview and Repository Context

This document explains how the Phoenix heat‑island model in `city_os/phoenix/heat_island` moves from
satellite calibration to corridor‑level cooling decisions. It ties together:

- Scientific context (Phoenix, hex grid, Landsat‑8).
- Implementation pipeline (Google Earth Engine → regression → Rust + ALN).
- Policy logic (rules of thumb for placing shade, cool materials, and water).

**Key paths in Prometheus‑Praxis**:

- GEE script:  
  `city_os/phoenix/heat_island/scripts/phoenix_hex_landsat_aggregation.js`
- Calibration tool:  
  `city_os/phoenix/heat_island/tools/fit_uhi_params.py`
- Calibration data outputs:  
  `city_os/phoenix/heat_island/data/calibration_params.json`  
  `city_os/phoenix/heat_island/data/hex_metrics.json`
- Rust crate (planning logic):  
  `city_os/phoenix/heat_island/` (crate `phoenix_heat_island`)
- ALN shard (semantic bindings and policy functions):  
  `city_os/phoenix/heat_island/hex_landsat_calibration.aln`

All components are designed to work together as an end‑to‑end pipeline for Aletheion’s planning functions.

---

## 2. Analytical Framework: Phoenix, Hex Grid, Landsat‑8

### 2.1. Phoenix as a UHI testbed

Phoenix, Arizona is a global benchmark for extreme urban heat:

- Semi‑arid climate, summer daily maximums regularly above 37.8 °C (100 °F).
- Documented large surface UHI magnitudes (urban–rural gaps up to ~5.5–7.8 °C), with growth over recent decades.
- Complex behavior including strong nighttime UHI and possible daytime “urban cool island” effects under highly reflective surfaces.

This makes Phoenix an ideal case for calibrating a robust heat‑island offset model that Aletheion can use more broadly.

### 2.2. Hexagonal grid as primary spatial substrate

The Phoenix heat‑island module uses a hexagonal grid as the primary analytical substrate:

- Each hex has six neighbors at equal distance, providing a consistent, direction‑free definition of local neighborhood.
- This is well‑suited to:
  - Aggregating continuous Land Surface Temperature (LST).
  - Detecting contiguous hot and cool corridors.
  - Running graph‑based planning algorithms.

In contrast:

- Square rasters can introduce directional artifacts due to edge/corner geometry.
- Census tracts are irregular, large units better for reporting but poor for fine‑scale thermal modeling.

Design choice:

- **Hex grid** is the authoritative space for analysis and routing.
- Tracts and other administrative units are used downstream for reporting and equity overlays.

The hex grid itself lives in a separate asset (e.g., `city_os/phoenix/hex_grid/v1`) and is imported into GEE and downstream tools.

### 2.3. Landsat‑8 as thermal data source

The pipeline uses Landsat‑8 Collection 2 Level‑2 science products:

- Dataset: `LANDSAT/LC08/C02/T1_L2` (surface reflectance + LST).[81]
- Resolution: 30 m, suitable for city‑scale UHI mapping.[81]
- Bands used:
  - `SR_B3` (Green), `SR_B4` (Red), `SR_B5` (NIR), `SR_B6` (SWIR1) for indices.
  - `ST_B10` (thermal infrared) for Land Surface Temperature.

Standard indices used for UHI analysis:

- NDVI (vegetation): green/health of vegetation.
- NDBI (built‑up/impervious): intensity of built surfaces.
- NDWI (water/moisture): presence of water and moist surfaces.[89][104][116]

These indices are computed per pixel, then aggregated to hex cells, forming the feature set for the heat‑island model.

---

## 3. Phase I: Calibration Pipeline and α/β/γ

Phase I establishes a defensible relationship between hex‑level UHI and land‑cover indices. It has three stages:

1. LST and index retrieval in GEE.
2. UHI derivation on the hex grid.
3. Parameter calibration via regression (α, β, γ).

### 3.1. LST and indices: GEE script

**Script:**  
`city_os/phoenix/heat_island/scripts/phoenix_hex_landsat_aggregation.js`

This script (run in the Earth Engine Code Editor):

1. Loads the Phoenix hex grid as a `FeatureCollection`.
2. Filters `LANDSAT/LC08/C02/T1_L2` by:
   - Phoenix AOI (union of hex geometries).
   - Date range (e.g., 2020‑06‑01 to 2020‑09‑30).
   - Cloud cover (<10%).
3. Applies QA‑based cloud masking.
4. Computes:
   - NDVI from SR_B5 and SR_B4.
   - NDBI from SR_B6 and SR_B5.
   - NDWI from SR_B3 and SR_B5.
   - LST in Kelvin from ST_B10 using documented scale/offset, then Celsius.[81][94]
5. Builds a median composite over the date window.
6. Aggregates composite bands onto the hex grid:
   - Mean, median, and standard deviation of LST, NDVI, NDBI, NDWI per hex.
7. Exports a hex‑level GeoJSON table to Google Drive:
   - Example name: `phoenix_hex_landsat_stats_2020_summer.geojson`.

This exported table is the input to the regression script.

### 3.2. UHI derivation and regression: Python tool

**Tool:**  
`city_os/phoenix/heat_island/tools/fit_uhi_params.py`

Usage example:

```bash
python tools/fit_uhi_params.py \
  --input-geojson data/phoenix_hex_landsat_stats_2020_summer.geojson \
  --output-calibration data/calibration_params.json \
  --output-hex-metrics data/hex_metrics.json \
  --season-label 2020-Summer
```

Pipeline inside `fit_uhi_params.py`:

1. **Ingest GeoJSON** using `geopandas`.[96][109]
2. **Select rural hexes** using NDBI/NDVI thresholds:
   - Rural mask: `NDBI <= rural_ndbi_max` and `NDVI` within `[rural_ndvi_min, rural_ndvi_max]`.
3. **Compute rural reference LST**:
   - `lst_c_median_rural` = median LST of rural hexes.
4. **Define UHI per hex**:
   - `UHI_h = mean_LST_C_h - lst_c_median_rural`.
5. **Fit Ridge regression** (L2‑regularized OLS):
   - Model:  
     `UHI_h = θ0 + θV * NDVI_h + θB * NDBI_h + θW * NDWI_h + ε_h`.
   - Uses `sklearn.linear_model.Ridge`.[95][108]
   - Splits data into train/validation sets for R² and RMSE estimation.
6. **Extract coefficients**:
   - `theta_v` (vegetation effect),
   - `theta_b` (built‑up effect),
   - `theta_w` (water effect).

### 3.3. α, β, γ: interpretation and storage

The fitted coefficients are mapped to the heat‑island parameters:

- **α (alpha)** = `theta_v` (°C per unit NDVI). Expected negative (more vegetation → cooler).[101][104][116]
- **β (beta)** = `theta_b` (°C per unit NDBI). Expected positive (more impervious → hotter).[104]
- **γ (gamma)** = `theta_w` (°C per unit NDWI). Expected negative (more water/moisture → cooler).[101][104]

These are stored as:

**`data/calibration_params.json`**

Example structure:

```json
{
  "city": "Phoenix-AZ",
  "dataset": "LANDSAT/LC08/C02_T1_L2",
  "season": "2020-Summer",
  "hex_grid_ref": "city_os/phoenix/hex_grid/v1",
  "regression_method": "ridge_UHI_vs_NDVI_NDBI_NDWI_alpha=1.0",
  "parameters": {
    "alpha": {
      "estimate": -3.2,
      "ci_low": -3.2,
      "ci_high": -3.2,
      "units": "degC_per_unit_NDVI"
    },
    "beta": {
      "estimate": 2.1,
      "ci_low": 2.1,
      "ci_high": 2.1,
      "units": "degC_per_unit_NDBI"
    },
    "gamma": {
      "estimate": -1.4,
      "ci_low": -1.4,
      "ci_high": -1.4,
      "units": "degC_per_unit_NDWI"
    }
  },
  "model_metrics": {
    "r2": 0.68,
    "rmse_degC": 1.7,
    "n_hex_train": 1200,
    "n_hex_valid": 300
  },
  "rural_reference": {
    "lst_c_median": 36.5,
    "selection_rule": "NDBI <= 0.0 AND NDVI between 0.1 and 0.3"
  }
}
```

(Values above are placeholders; actual values come from the fit.)

The range and sign expectations are informed by multi‑index UHI studies where NDVI and NDWI slopes are negative, NDBI is positive.[101][104][116]

### 3.4. Hex‑level metrics

**`data/hex_metrics.json`**

Example entries:

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

These metrics are inputs to the Rust module and ALN shard.

---

## 4. Rust + ALN: Turning Parameters into Planning Functions

### 4.1. Rust crate `phoenix_heat_island`

The Rust crate provides deterministic functions Aletheion can call.

Core types (simplified overview):

```rust
pub struct CalibrationParams {
    pub alpha: ParameterEstimate, // vegetation impact
    pub beta: ParameterEstimate,  // built-up materials impact
    pub gamma: ParameterEstimate, // water/moisture impact
}

pub struct ParameterEstimate {
    pub estimate: f64,
    pub ci_low: f64,
    pub ci_high: f64,
    pub units: &'static str,
}

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
```

Key functions:

```rust
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

These mirror the policy logic described in the prose:

- `tree_priority` emphasizes α (vegetation cooling), NDVI gap, UHI, and feasible tree planting.
- `roof_priority` emphasizes β and roof area fraction.
- `water_priority` emphasizes γ and hydrology feasibility.

### 4.2. ALN shard `hex_landsat_calibration.aln`

The ALN shard binds:

- α, β, γ and their provenance.
- Hex metrics from the data lake.
- Functional scoring logic for interventions.

Conceptual structure:

```aln
shard PhoenixHeatIslandCalibration {
  identity {
    city         = "Phoenix-AZ";
    dataset      = "Landsat-8-C2-L2-LST";
    season       = "2020-Summer";
    hex_grid_ref = "city_os/phoenix/hex_grid/v1";
  }

  parameters {
    alpha { estimate = -3.2; ci_low = -4.0; ci_high = -2.5; units = "degC_per_unit_NDVI"; }
    beta  { estimate =  2.1; ci_low =  1.5; ci_high =  2.8; units = "degC_per_unit_NDBI"; }
    gamma { estimate = -1.4; ci_low = -2.0; ci_high = -0.8; units = "degC_per_unit_NDWI"; }
  }

  functions {
    delta_T(hex_id, delta_ndvi, delta_ndbi, delta_ndwi) =
      alpha.estimate * delta_ndvi +
      beta.estimate  * delta_ndbi +
      gamma.estimate * delta_ndwi;

    tree_priority(hex_id) =
      UHI(hex_id) *
      abs(alpha.estimate) *
      (ndvi_target_max - NDVI(hex_id)) *
      feasible_tree_factor(hex_id);

    roof_priority(hex_id) =
      UHI(hex_id) *
      abs(beta.estimate) *
      roof_area_fraction(hex_id);

    water_priority(hex_id) =
      UHI(hex_id) *
      abs(gamma.estimate) *
      hydrology_feasibility(hex_id);
  }

  bindings {
    NDVI(hex_id)  -> DataLake.HexMetrics.NDVI[hex_id];
    NDBI(hex_id)  -> DataLake.HexMetrics.NDBI[hex_id];
    NDWI(hex_id)  -> DataLake.HexMetrics.NDWI[hex_id];
    UHI(hex_id)   -> DataLake.HexMetrics.UHI[hex_id];
    roof_area_fraction(hex_id) -> CityOS.BuildingLayer.roof_fraction[hex_id];
    feasible_tree_factor(hex_id) -> CityOS.StreetLayer.tree_feasibility[hex_id];
    hydrology_feasibility(hex_id) -> CityOS.HydrologyLayer.feasibility[hex_id];
  }
}
```

ALN ensures that the model’s semantics are explicit and that bindings to other city layers remain transparent and auditable.

---

## 5. Phase II: Corridor‑Level Application and Policy Rules

Once α, β, γ are calibrated and hex metrics computed, the system moves to application:

- Identify hot corridors and cool refuges.
- Simulate interventions.
- Derive rules of thumb for Aletheion.

### 5.1. Identifying hot corridors and cool refuges

Using `hex_metrics.json`:

- **Hot corridors**:
  - Hexes with high `uhi`, high `mean_ndbi`, low `mean_ndvi`, low `mean_ndwi`.
  - Connected via hex adjacency into chains (graph components).

- **Cool refuges**:
  - Hexes with low `uhi`, high `mean_ndvi` or `mean_ndwi`.
  - Serve as seeds for “cool corridors”.

The hex lattice lets Aletheion:

- Detect continuous heat chains along arterials, industrial zones, and parking complexes.
- Plan interventions that break or cool these chains rather than isolated spots.

### 5.2. Scenario simulations

Three primary scenarios:

1. **Targeted tree canopy expansion**  
   - Increase NDVI in selected hexes (e.g., from ~0.1 to 0.25).  
   - Use `delta_temperature` and `tree_priority` to estimate cooling per hex and per corridor.  
   - Focus on hexes with high UHI, low NDVI, high feasible_tree_factor and strong negative α.

2. **Cool roofs**  
   - Adjust effective NDBI downward via higher roof albedo in hexes with high roof_area_fraction and UHI.  
   - Use `roof_priority` and β to estimate benefits.  
   - Target industrial/commercial corridors with limited vegetation potential.

3. **Water features**  
   - Increase NDWI in hexes with high hydrology_feasibility and UHI.  
   - Use `water_priority` and γ to quantify cooling per unit water intervention.  
   - Balance cooling gains against water scarcity, prioritizing reuse and xeric design.

Each scenario generates updated UHI maps and corridor “cooling leverage” scores that feed directly into Aletheion’s planning engines.

### 5.3. Policy‑relevant rules of thumb

Examples of rules derived from the calibrated model and corridor analysis:

1. **Prioritize high‑impact locations**  
   - Allocate shade infrastructure to hexes where `tree_priority` is highest:
     - Large negative α (strong vegetation cooling).
     - High UHI.
     - Large NDVI gap (low current vegetation, high potential).

2. **Target socially vulnerable communities**  
   - Overlay `uhi` and priority scores with socio‑demographic vulnerability layers.
   - Prioritize interventions in hexes where thermal stress and vulnerability coincide.

3. **Connect cool refuges**  
   - Use adjacency to design continuous green/blue corridors.
   - Apply interventions to hex chains connecting parks, transit routes, and public facilities.

4. **Mandate cool materials in hot zones**  
   - Where `roof_priority` exceeds a threshold, enforce cool roof standards for new builds and incentivize retrofits.

5. **Optimize hydrology**  
   - Use γ and NDWI deltas to quantify cooling per unit reused water.
   - Favor xeric, high‑NDVI plantings and distributed water features over water‑intensive turf.

These rules are expressed as functions over hex metrics and α/β/γ, not just narrative guidance, and can be encoded in Aletheion’s policy modules.

---

## 6. Contributor Workflow Summary

For contributors working on the Phoenix heat‑island module:

1. **Run GEE script**  
   - Open `scripts/phoenix_hex_landsat_aggregation.js` in the Earth Engine Code Editor.  
   - Set `HEX_GRID_ASSET_ID` for the Phoenix hex grid asset.  
   - Run the script; download the resulting GeoJSON from Drive.

2. **Run calibration tool**  
   - Place the GeoJSON under `city_os/phoenix/heat_island/data/`.  
   - Run `tools/fit_uhi_params.py` as described above.  
   - Commit `calibration_params.json` and `hex_metrics.json` after review.

3. **Update Rust + ALN**  
   - Ensure the Rust crate loads the new JSONs and exposes priority functions.  
   - Update ALN shard if parameter estimates change meaningfully.

4. **Integrate with Aletheion**  
   - Use priority scores and corridor logic to drive planning endpoints (tree planting, cool materials, hydrology design).

This document is the canonical reference for how scientific calibration, hex‑level metrics, and Aletheion’s cooling policies fit together in the Phoenix UHI framework.
