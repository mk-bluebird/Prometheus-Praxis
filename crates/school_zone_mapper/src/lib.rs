//! Library for mapping Texas/Arizona schools into the KER-ready
//! `TexasArizonaSchoolNanoCorridor2026v1.csv` shard format.
//!
//! Functions here are non‑actuating and purely diagnostic.

use csv::{ReaderBuilder, WriterBuilder};
use serde::Deserialize;
use std::collections::HashSet;
use std::io::{BufReader, Read, Write};

/// Minimal structure for one NCES school record.
#[derive(Debug, Deserialize)]
pub struct NcesSchool {
    #[serde(rename = "NCESSCH")]
    pub nces_id: String,
    #[serde(rename = "LEAID")]
    pub lea_id: String,
    #[serde(rename = "NAME")]
    pub name: String,
    #[serde(rename = "STREET")]
    pub street: Option<String>,
    #[serde(rename = "CITY")]
    pub city: Option<String>,
    #[serde(rename = "STATE")]
    pub state: String,
    #[serde(rename = "ZIP")]
    pub zip: Option<String>,
    #[serde(rename = "LAT1516")]
    pub lat: Option<f64>,
    #[serde(rename = "LON1516")]
    pub lon: Option<f64>,
    #[serde(rename = "SY_STATUS")]
    pub sy_status: Option<String>,
    #[serde(rename = "LEVEL")]
    pub level: Option<String>,
}

/// Assign screwworm zone based on county and state.
/// Uses USDA/TAHC outbreak data as of July 2026.
pub fn screwworm_zone(county: &str, state: &str) -> &str {
    if state == "TX" {
        let infested: HashSet<&str> = [
            "ZAVALA", "LA SALLE", "DIMMIT", "MAVERICK", "KINNEY", "UVALDE",
        ]
        .iter()
        .cloned()
        .collect();
        let watch: HashSet<&str> = ["KENDALL", "BEXAR", "WEBB", "MEDINA", "FRIO"]
            .iter()
            .cloned()
            .collect();
        if infested.contains(county) {
            "INFESTED"
        } else if watch.contains(county) {
            "WATCH"
        } else {
            "NONE"
        }
    } else if state == "AZ" {
        "WATCH" // all AZ counties currently watch due to proximity
    } else {
        "NONE"
    }
}

/// Map a ZIP code to a county name for known screwworm‑affected ZIPs.
pub fn zip_to_county(zip: &str, state: &str) -> String {
    if state == "TX" {
        match zip {
            "78839" => "ZAVALA".to_string(),
            "78014" => "LA SALLE".to_string(),
            "78827" => "DIMMIT".to_string(),
            "78852" => "MAVERICK".to_string(),
            "78834" => "KINNEY".to_string(),
            "78801" => "UVALDE".to_string(),
            "78063" => "MEDINA".to_string(),
            "78023" => "BEXAR".to_string(),
            "78022" => "KENDALL".to_string(),
            _ => "OTHER".to_string(),
        }
    } else {
        "OTHER".to_string()
    }
}

/// Construct a region_id from state and NCES school ID.
pub fn region_id(state: &str, nces_id: &str) -> String {
    format!("{}-{}-ZONE-01", state, nces_id)
}

/// The exact header columns for the shard CSV.
pub const SHARD_HEADER: [&str; 46] = [
    "region_id", "region_name", "state", "county", "district", "campus_id", "campus_name",
    "lat", "lon", "twindow_start", "twindow_end", "context_screwworm_zone",
    "context_livestock_density", "context_vec_index", "school_enrollment",
    "school_level", "has_ag_program", "water_source_type", "hvac_type",
    "parasite_indicator_type", "parasite_indicator_value", "parasite_indicator_unit",
    "nano_mode", "nano_parasite_coverage_frac", "nano_neuro_binding_score",
    "nano_material_half_life_days", "nano_carbon_kg_co2e_year", "nano_uncertainty_index",
    "r_parasite", "r_nano_benefit_gap", "r_neuro", "r_persistence", "r_carbon", "r_sigma",
    "vt", "ker_k", "ker_e", "ker_r", "kerdeployable",
    "corridor_neuro", "corridor_persistence", "corridor_carbon", "corridor_uncertainty",
    "evidence_hex", "signing_hex",
];

/// Generates a full shard CSV from an NCES school input reader.
///
/// `input` – any readable source of the NCES school CSV data.
/// `output` – any writable destination for the output shard.
/// `twindow_start` / `twindow_end` – ISO 8601 time window for this mapping.
pub fn map_schools_to_shard<R: Read, W: Write>(
    input: R,
    output: W,
    twindow_start: &str,
    twindow_end: &str,
) -> Result<(), Box<dyn std::error::Error>> {
    let mut rdr = ReaderBuilder::new()
        .has_headers(true)
        .flexible(true)
        .from_reader(BufReader::new(input));
    let mut wtr = WriterBuilder::new()
        .has_headers(true)
        .from_writer(output);

    wtr.write_record(&SHARD_HEADER)?;

    for result in rdr.deserialize::<NcesSchool>() {
        let rec = result?;
        if rec.state != "TX" && rec.state != "AZ" {
            continue;
        }
        if let Some(ref status) = rec.sy_status {
            if status != "1" && status != "2" && status != "3" {
                continue;
            }
        }

        let state = &rec.state;
        let county = match rec.zip.as_deref() {
            Some(z) => zip_to_county(z, state),
            None => "UNKNOWN".to_string(),
        };
        let zone = screwworm_zone(&county, state);
        let lat = rec.lat.unwrap_or(0.0);
        let lon = rec.lon.unwrap_or(0.0);
        let rid = region_id(state, &rec.nces_id);
        let region_name = format!("{} {}", rec.name, rec.city.as_deref().unwrap_or(""));

        wtr.write_record(&[
            rid,
            region_name,
            state.to_string(),
            county,
            rec.lea_id,
            rec.nces_id,
            rec.name,
            format!("{:.3}", lat),
            format!("{:.3}", lon),
            twindow_start.to_string(),
            twindow_end.to_string(),
            zone.to_string(),
            "UNKNOWN".to_string(),
            "0.0".to_string(),
            "0".to_string(),
            rec.level.unwrap_or_else(|| "U".to_string()),
            "false".to_string(),
            "UNKNOWN".to_string(),
            "UNKNOWN".to_string(),
            "NONE".to_string(),
            "0.0".to_string(),
            "N_A".to_string(),
            "NONE".to_string(),
            "0.0".to_string(),
            "0.0".to_string(),
            "0.0".to_string(),
            "0.0".to_string(),
            "1.0".to_string(),
            "0.0".to_string(),
            "1.0".to_string(),
            "0.0".to_string(),
            "0.0".to_string(),
            "0.0".to_string(),
            "1.0".to_string(),
            "1.0".to_string(),
            "0.0".to_string(),
            "0.0".to_string(),
            "0.0".to_string(),
            "false".to_string(),
            "NEURO_TIGHT".to_string(),
            "PERSIST_BIODEG_FAST".to_string(),
            "CARBON_HIGH".to_string(),
            "UNCERTAINTY_HIGH".to_string(),
            "0x0000000000000000".to_string(),
            "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7".to_string(),
        ])?;
    }

    wtr.flush()?;
    Ok(())
}
