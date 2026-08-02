// File: city_os/phoenix/heat_island/scripts/phoenix_hex_landsat_aggregation.js
//
// Purpose:
//   Aggregate Landsat-8 Level-2 land surface temperature (LST) and spectral
//   indices (NDVI, NDBI, NDWI) onto the Phoenix hexagonal grid for UHI
//   calibration. This script is intended to be run in the Google Earth Engine
//   (GEE) Code Editor by a contributor with GEE access.
//
// Context:
//   - Outputs a hex-level GeoJSON table used downstream by the regression
//     pipeline in Prometheus-Praxis/city_os/phoenix/heat_island.
//   - The exported GeoJSON is expected to be committed under
//     city_os/phoenix/heat_island/data/ after review.
//
// Usage (for contributors):
//   1. Upload the Phoenix hex grid as an EE asset (FeatureCollection) and
//      set HEX_GRID_ASSET_ID below.
//   2. Open this script in the GEE Code Editor.
//   3. Run the script to start an Export.table.toDrive task.
//   4. After the export completes, download the GeoJSON from Drive and place it
//      under city_os/phoenix/heat_island/data/ as described in the README.
//
// Notes:
//   - Dataset: LANDSAT/LC08/C02/T1_L2 (surface reflectance + LST).
//   - Indices: NDVI, NDBI, NDWI using SR bands with documented scale/offset.
//   - LST: ST_B10 → Kelvin → Celsius, per USGS Collection 2 docs.[81][94]

// 1. Configuration ----------------------------------------------------

// Replace with your actual hex grid asset ID in Earth Engine.
var HEX_GRID_ASSET_ID = 'users/<your_user>/phoenix_hex_grid';

// Date window for calibration (summer season).
var START_DATE = '2020-06-01';
var END_DATE   = '2020-09-30';

// Cloud cover threshold.
var MAX_CLOUD_COVER = 10;

// Export configuration.
var EXPORT_DESCRIPTION = 'phoenix_hex_landsat_stats_2020_summer';
var EXPORT_FOLDER      = 'phoenix_heat_island';  // Google Drive folder name.

// 2. Load hex grid and define AOI -------------------------------------

var hexGrid = ee.FeatureCollection(HEX_GRID_ASSET_ID);
var phoenixAOI = hexGrid.geometry();

// 3. Landsat-8 Level 2 collection ------------------------------------
// Dataset: LANDSAT/LC08/C02/T1_L2 (surface reflectance + LST).[81][91]

var landsatL2 = ee.ImageCollection('LANDSAT/LC08/C02/T1_L2')
  .filterBounds(phoenixAOI)
  .filterDate(START_DATE, END_DATE)
  .filter(ee.Filter.lt('CLOUD_COVER', MAX_CLOUD_COVER));

// Cloud mask using QA_PIXEL (simplified; tune as needed).
function maskL8L2(image) {
  var qa = image.select('QA_PIXEL');
  var cloudBit       = 1 << 3;
  var cloudShadowBit = 1 << 4;
  var mask = qa.bitwiseAnd(cloudBit).eq(0)
    .and(qa.bitwiseAnd(cloudShadowBit).eq(0));
  return image.updateMask(mask);
}

// Add NDVI, NDBI, NDWI and LST_C bands.
// NDVI, NDBI, NDWI formulas use surface reflectance bands with scale/offset.[81][86][89]
function addIndices(image) {
  // Apply scale/offset to SR bands as recommended for Collection 2.[81][91]
  var scale = 0.0000275;
  var offset = -0.2;

  var sr_b3 = image.select('SR_B3').multiply(scale).add(offset); // Green
  var sr_b4 = image.select('SR_B4').multiply(scale).add(offset); // Red
  var sr_b5 = image.select('SR_B5').multiply(scale).add(offset); // NIR
  var sr_b6 = image.select('SR_B6').multiply(scale).add(offset); // SWIR1

  var ndvi = sr_b5.subtract(sr_b4).divide(sr_b5.add(sr_b4)).rename('NDVI');[81][91]
  var ndbi = sr_b6.subtract(sr_b5).divide(sr_b6.add(sr_b5)).rename('NDBI');[86]
  var ndwi = sr_b3.subtract(sr_b5).divide(sr_b3.add(sr_b5)).rename('NDWI');[85][89]

  // Land Surface Temperature band ST_B10: scale/offset from metadata.[81][94]
  var lstK = image.select('ST_B10').multiply(0.00341802).add(149.0).rename('LST_K');
  var lstC = lstK.subtract(273.15).rename('LST_C');

  return image.addBands([ndvi, ndbi, ndwi, lstC]);
}

// Build processed image collection.
var processed = landsatL2
  .map(maskL8L2)
  .map(addIndices);

// Median composite over the date window.[72]
var composite = processed.median()
  .clip(phoenixAOI)
  .select(['LST_C', 'NDVI', 'NDBI', 'NDWI']);

// 4. Aggregate to hex grid --------------------------------------------

// Define a reducer that computes mean, median, and standard deviation.
var reducer = ee.Reducer.mean()
  .combine(ee.Reducer.median(), null, true)
  .combine(ee.Reducer.stdDev(), null, true);

// Reduce composite image over each hex feature.
var hexStats = composite.reduceRegions({
  collection: hexGrid,
  reducer: reducer,
  scale: 30  // Landsat resolution.[81]
});

// Optional: inspect one hex to verify fields in the Code Editor.
print('Hex stats sample', hexStats.first());

// 5. Export to Google Drive ------------------------------------------[83][87][90]

Export.table.toDrive({
  collection: hexStats,
  description: EXPORT_DESCRIPTION,
  folder: EXPORT_FOLDER,
  fileFormat: 'GeoJSON'
});
