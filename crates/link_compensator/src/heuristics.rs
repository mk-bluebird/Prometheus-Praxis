#![forbid(unsafe_code)]

use crate::{CompensatedDatum};
use chrono::{DateTime, Utc};

pub struct HeuristicOutput {
    pub fetched_status: String,
    pub archive_url: Option<String>,
    pub snapshot_ts_utc: Option<DateTime<Utc>>,
    pub energy_saved_kwh: Option<f64>,
    pub co2_offset_kg: Option<f64>,
    pub material_recyclability: Option<String>,
    pub data: Vec<CompensatedDatum>,
}

pub fn apply_heuristics(url: &str, source_kind: &str) -> HeuristicOutput {
    match source_kind {
        "CITY_GIS" => {
            let mut data = Vec::new();
            data.push(CompensatedDatum {
                key: "DATA_TYPE".to_string(),
                value: "City GIS datasets including canal alignments, stormwater, streets, parcels, facilities.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "FORMAT".to_string(),
                value: "ArcGIS Hub datasets; Shapefile, GeoJSON, KML, MapServer feature services.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "ACCESS_PATTERN".to_string(),
                value: "Search Phoenix Open Data Hub for 'canal', 'water', 'stormwater'; use ArcGIS REST endpoints for downloads.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "GOVERNANCE_HINT".to_string(),
                value: "Use canal centerlines and trails as corridors for cybo-aquatic machines; anchor no-go buffers at crossings and critical habitats.".to_string(),
            });

            HeuristicOutput {
                fetched_status: "heuristic".to_string(),
                archive_url: None,
                snapshot_ts_utc: Some(Utc::now()),
                energy_saved_kwh: Some(100.0),
                co2_offset_kg: Some(10.0),
                material_recyclability: Some("NEUTRAL".to_string()),
                data,
            }
        }
        "COUNTY_GIS" => {
            let mut data = Vec::new();
            data.push(CompensatedDatum {
                key: "DATA_TYPE".to_string(),
                value: "County-wide hydrology, floodplains, wetlands, parcels, contours, LiDAR.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "FORMAT".to_string(),
                value: "ArcGIS Hub feature layers, shapefiles, REST services.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "ACCESS_PATTERN".to_string(),
                value: "Use Maricopa GIS Open Data portal; query NHD flowlines, floodplain and wetlands layers.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "GOVERNANCE_HINT".to_string(),
                value: "Define sediment-rebalancing corridors along NHD lines; avoid high-risk floodplain tiles for heavy machinery.".to_string(),
            });

            HeuristicOutput {
                fetched_status: "heuristic".to_string(),
                archive_url: None,
                snapshot_ts_utc: Some(Utc::now()),
                energy_saved_kwh: Some(150.0),
                co2_offset_kg: Some(20.0),
                material_recyclability: Some("POSITIVE".to_string()),
                data,
            }
        }
        "STATE_WILDLIFE" => {
            let mut data = Vec::new();
            data.push(CompensatedDatum {
                key: "DATA_TYPE".to_string(),
                value: "Species distribution and habitat systems including desertscrub, grassland, forest/woodland, aquatic/riparian.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "FORMAT".to_string(),
                value: "Geodatabases, shapefiles, habitat profile documents.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "ACCESS_PATTERN".to_string(),
                value: "Use AGFD habitat profiles and Species & Habitat geodatabases; filter aquatic/riparian near Phoenix basins.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "GOVERNANCE_HINT".to_string(),
                value: "Treat spawning grounds and riparian SGCN as no-go or time-windowed corridors for cybo-aquatic operations.".to_string(),
            });

            HeuristicOutput {
                fetched_status: "heuristic".to_string(),
                archive_url: None,
                snapshot_ts_utc: Some(Utc::now()),
                energy_saved_kwh: Some(80.0),
                co2_offset_kg: Some(15.0),
                material_recyclability: Some("N/A".to_string()),
                data,
            }
        }
        "FED_CRIT_HABITAT" => {
            let mut data = Vec::new();
            data.push(CompensatedDatum {
                key: "DATA_TYPE".to_string(),
                value: "USFWS threatened & endangered species critical habitat polygons (proposed and final).".to_string(),
            });
            data.push(CompensatedDatum {
                key: "FORMAT".to_string(),
                value: "Shapefiles, ArcGIS feature layers, tabular reports.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "ACCESS_PATTERN".to_string(),
                value: "Use USFWS ECOS mapper and HQ ES Critical Habitat feature layer; filter by state=AZ.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "GOVERNANCE_HINT".to_string(),
                value: "Treat critical habitat as hard no-go tiles or tightly regulated zones in governance kernels.".to_string(),
            });

            HeuristicOutput {
                fetched_status: "heuristic".to_string(),
                archive_url: None,
                snapshot_ts_utc: Some(Utc::now()),
                energy_saved_kwh: Some(200.0),
                co2_offset_kg: Some(40.0),
                material_recyclability: Some("N/A".to_string()),
                data,
            }
        }
        "FED_WETLANDS" => {
            let mut data = Vec::new();
            data.push(CompensatedDatum {
                key: "DATA_TYPE".to_string(),
                value: "National Wetlands Inventory maps and geospatial data for wetlands and deepwater habitats.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "FORMAT".to_string(),
                value: "WMS services, KML, shapefiles, geospatial datasets.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "ACCESS_PATTERN".to_string(),
                value: "Use Wetlands Mapper and NWI geospatial dataset; intersect with Salt and Gila River corridors.".to_string(),
            });
            data.push(CompensatedDatum {
                key: "GOVERNANCE_HINT".to_string(),
                value: "Use wetland classes with sensor telemetry to compute risk-of-harm tiles for dredging or planting robots.".to_string(),
            });

            HeuristicOutput {
                fetched_status: "heuristic".to_string(),
                archive_url: None,
                snapshot_ts_utc: Some(Utc::now()),
                energy_saved_kwh: Some(180.0),
                co2_offset_kg: Some(35.0),
                material_recyclability: Some("N/A".to_string()),
                data,
            }
        }
        _ => {
            let mut data = Vec::new();
            data.push(CompensatedDatum {
                key: "DATA_TYPE".to_string(),
                value: "Unknown or generic web resource; minimal heuristics applied.".to_string(),
            });
            HeuristicOutput {
                fetched_status: "unknown".to_string(),
                archive_url: None,
                snapshot_ts_utc: Some(Utc::now()),
                energy_saved_kwh: None,
                co2_offset_kg: None,
                material_recyclability: Some("N/A".to_string()),
                data,
            }
        }
    }
}
