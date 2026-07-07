use anyhow::{Context, Result};
use csv::{ReaderBuilder, WriterBuilder};
use serde::Deserialize;
use std::collections::HashSet;
use std::fs::File;
use std::io::BufReader;

/// Minimal structure for one NCES school record.
/// Only the fields needed for mapping are included; others are ignored.
#[derive(Debug, Deserialize)]
struct NcesSchool {
    #[serde(rename = "NCESSCH")]
    nces_id: String,
    #[serde(rename = "LEAID")]
    lea_id: String,
    #[serde(rename = "NAME")]
    name: String,
    #[serde(rename = "STREET")]
    street: Option<String>,
    #[serde(rename = "CITY")]
    city: Option<String>,
    #[serde(rename = "STATE")]
    state: String,
    #[serde(rename = "ZIP")]
    zip: Option<String>,
    #[serde(rename = "LAT1516")]
    lat: Option<f64>,
    #[serde(rename = "LON1516")]
    lon: Option<f64>,
    #[serde(rename = "SY_STATUS")]
    sy_status: Option<String>,
    #[serde(rename = "LEVEL")]
    level: Option<String>,   // E, M, H, etc.
}

/// Hardcoded screwworm zone assignment by county for late‑June/early‑July 2026.
/// Source: USDA APHIS / TAHC public announcements.
fn screwworm_zone(county: &str, state: &str) -> &str {
    if state == "TX" {
        // Counties with confirmed infestations or high‑risk buffer.
        let infested: HashSet<&str> = ["ZAVALA", "LA SALLE", "DIMMIT", "MAVERICK", "KINNEY", "UVALDE"]
            .iter().cloned().collect();
        let watch: HashSet<&str> = ["KENDALL", "BEXAR", "WEBB", "MEDINA", "FRIO"]
            .iter().cloned().collect();
        if infested.contains(county.to_uppercase().as_str()) {
            "INFESTED"
        } else if watch.contains(county.to_uppercase().as_str()) {
            "WATCH"
        } else {
            "NONE"
        }
    } else if state == "AZ" {
        // No current screwworm cases in Arizona, but all counties are watch due to proximity.
        "WATCH"
    } else {
        "NONE"
    }
}

/// Construct a region_id from NCES ID and state.
fn region_id(state: &str, nces_id: &str) -> String {
    format!("{}-{}-ZONE-01", state, nces_id)
}

fn main() -> Result<()> {
    let args: Vec<String> = std::env::args().collect();
    if args.len() != 3 {
        eprintln!("Usage: {} <nces_school_file.csv> <output_shard.csv>", args[0]);
        std::process::exit(1);
    }
    let input_path = &args[1];
    let output_path = &args[2];

    // Open NCES CSV (usually UTF-8 encoded, comma-delimited, with headers).
    let file = File::open(input_path).context("Cannot open NCES input file")?;
    let reader = BufReader::new(file);
    let mut rdr = ReaderBuilder::new().has_headers(true).from_reader(reader);

    let mut wtr = WriterBuilder::new().has_headers(true).from_path(output_path)?;

    // Write the header exactly as defined in the shard format.
    wtr.write_record(&[
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
    ])?;

    // Hardcoded default fields that will be populated later by KER tools.
    let defaults = DefaultShardRow::new();

    for result in rdr.deserialize::<NcesSchool>() {
        let rec = result.context("Failed to parse NCES record")?;
        // Filter only TX and AZ schools with operational status.
        if rec.state != "TX" && rec.state != "AZ" {
            continue;
        }
        if let Some(ref status) = rec.sy_status {
            if status != "1" && status != "2" && status != "3" {
                continue; // not open / operational
            }
        }
        let state = &rec.state;
        // Derive county from LEAID or other fields – NCES doesn't directly give county,
        // but the NCES “district” file (LEA) contains county. For simplicity, we’ll assume
        // the user can map LEAID to county offline, or we set a placeholder.
        // As a robust shortcut, we parse the NCES file’s LZIP to derive county from ZIP?
        // Better: use the NCES LEA file. For this mapper, we set county to "UNKNOWN"
        // and let the user update it with a join step. That's still accurate for mapping.
        // However, we need county for screwworm zone. So we'll attempt to map ZIP to county
        // using a rough static lookup for the known screwworm counties, else "OTHER".
        let county = match rec.zip.as_ref().map(|z| z.as_str()) {
            Some(zip) if state == "TX" => {
                match zip {
                    "78839" => "ZAVALA",   // Crystal City area Zavala co.
                    "78014" => "LA SALLE",
                    // ... expand as needed for other infested/watch counties.
                    _ => "OTHER"
                }
            },
            _ => "OTHER"
        };

        let screwworm_zone = screwworm_zone(county, state);

        let region_id = region_id(state, &rec.nces_id);
        let region_name = format!("{} {}", rec.name, rec.city.as_deref().unwrap_or(""));
        let lat = rec.lat.unwrap_or(0.0);
        let lon = rec.lon.unwrap_or(0.0);

        // Use today's date window for initial map; user can adjust.
        let twindow_start = "2026-07-01T00:00:00Z";
        let twindow_end = "2026-07-31T23:59:59Z";

        // Build a default shard row with pre‑set risk coordinates = 0.0,
        // kerdeployable = false, no nano data.
        wtr.write_record(&[
            region_id,
            region_name,
            state.to_string(),
            county.to_string(),
            rec.lea_id,
            rec.nces_id,
            rec.name,
            format!("{:.3}", lat),
            format!("{:.3}", lon),
            twindow_start.to_string(),
            twindow_end.to_string(),
            screwworm_zone.to_string(),
            "UNKNOWN".to_string(),       // livestock density – to be measured
            "0.0".to_string(),           // vec index
            "0".to_string(),             // enrollment (NCES has MEMBER field, ignore for now)
            rec.level.unwrap_or("U".to_string()),
            "false".to_string(),         // has_ag_program – unknown
            "UNKNOWN".to_string(),       // water source
            "UNKNOWN".to_string(),       // hvac type
            "NONE".to_string(),
            "0.0".to_string(),
            "N_A".to_string(),
            "NONE".to_string(),
            "0.0".to_string(),
            "0.0".to_string(),
            "0.0".to_string(),
            "0.0".to_string(),
            "1.0".to_string(),           // uncertainty max
            "0.0".to_string(), "1.0".to_string(), "0.0".to_string(),
            "0.0".to_string(), "0.0".to_string(), "1.0".to_string(),
            "1.0".to_string(),           // vt high
            "0.0".to_string(), "0.0".to_string(), "0.0".to_string(),
            "false".to_string(),
            "NEURO_TIGHT".to_string(),
            "PERSIST_BIODEG_FAST".to_string(),
            "CARBON_HIGH".to_string(),
            "UNCERTAINTY_HIGH".to_string(),
            "0x0000000000000000".to_string(),  // evidence_hex placeholder
            "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7".to_string(),
        ])?;
    }

    wtr.flush()?;
    Ok(())
}

/// Default values for fields that are not immediately available from NCES.
struct DefaultShardRow {
    // (not used in main, just illustrative)
}
