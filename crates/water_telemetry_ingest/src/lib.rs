// Filename: crates/water_telemetry_ingest/src/lib.rs
// Destination: eco_restoration_shard/crates/water_telemetry_ingest/src/lib.rs
//
// Rust edition: 2024
// rust-version = "1.85"
// License: MIT OR Apache-2.0
//
// This crate ingests water.tank.telemetry.v1 particles and Phoenix water-quality CSVs,
// computes Lyapunov indices and normalized risk coordinates, and writes into SQLite
// surfaces used by the Phoenix ERM and eco-restoration stacks.

#![forbid(unsafe_code)]

use rusqlite::{params, Connection};
use serde::{Deserialize, Serialize};
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::time::SystemTime;

//
// 1. Tank telemetry: water.tank.telemetry.v1
//

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct WaterTankTelemetryV1 {
    pub node_id: String,
    pub timestamp_utc_s: i64,
    pub h1_m: f32,
    pub h2_m: f32,
    pub v_lyapunov: f32,
    pub u_cmd_norm: f32,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LyapunovHealthIndices {
    pub node_id: String,
    pub sample_count: usize,
    pub v_mean: f64,
    pub v_max: f64,
    pub frac_v_increasing: f64,
    pub max_delta_v_pos: f64,
    pub max_delta_v_neg: f64,
}

pub fn compute_lyapunov_health_indices(
    samples: &[WaterTankTelemetryV1],
) -> Option<LyapunovHealthIndices> {
    if samples.is_empty() {
        return None;
    }

    let node_id = samples[0].node_id.clone();
    let sample_count = samples.len();

    let mut sum_v = 0.0_f64;
    let mut v_max = f32::NEG_INFINITY;

    for s in samples {
        let v = s.v_lyapunov as f64;
        sum_v += v;
        if s.v_lyapunov > v_max {
            v_max = s.v_lyapunov;
        }
    }

    let v_mean = sum_v / (sample_count as f64);

    let mut increasing_count: usize = 0;
    let mut max_delta_v_pos: f64 = 0.0;
    let mut max_delta_v_neg: f64 = 0.0;

    for window in samples.windows(2) {
        let v0 = window[0].v_lyapunov as f64;
        let v1 = window[1].v_lyapunov as f64;
        let delta = v1 - v0;
        if delta > 0.0 {
            increasing_count += 1;
            if delta > max_delta_v_pos {
                max_delta_v_pos = delta;
            }
        } else {
            if delta < max_delta_v_neg {
                max_delta_v_neg = delta;
            }
        }
    }

    let frac_v_increasing = if sample_count > 1 {
        increasing_count as f64 / ((sample_count - 1) as f64)
    } else {
        0.0
    };

    Some(LyapunovHealthIndices {
        node_id,
        sample_count,
        v_mean,
        v_max: v_max as f64,
        frac_v_increasing,
        max_delta_v_pos,
        max_delta_v_neg,
    })
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct DistrictLyapunovSummary {
    pub district_id: String,
    pub node_count: usize,
    pub v_mean_mean: f64,
    pub v_max_max: f64,
    pub frac_v_increasing_mean: f64,
}

pub fn aggregate_district_summary(
    district_id: &str,
    node_indices: &[LyapunovHealthIndices],
) -> Option<DistrictLyapunovSummary> {
    if node_indices.is_empty() {
        return None;
    }

    let mut sum_v_mean = 0.0;
    let mut sum_frac_incr = 0.0;
    let mut v_max_max = f64::NEG_INFINITY;

    for idx in node_indices {
        sum_v_mean += idx.v_mean;
        sum_frac_incr += idx.frac_v_increasing;
        if idx.v_max > v_max_max {
            v_max_max = idx.v_max;
        }
    }

    let node_count = node_indices.len();
    let v_mean_mean = sum_v_mean / (node_count as f64);
    let frac_v_increasing_mean = sum_frac_incr / (node_count as f64);

    Some(DistrictLyapunovSummary {
        district_id: district_id.to_string(),
        node_count,
        v_mean_mean,
        v_max_max,
        frac_v_increasing_mean,
    })
}

//
// 2. Phoenix water-quality CSV → qpudatashard_water_quality
//

#[derive(Clone, Copy, Debug, Serialize, Deserialize)]
pub struct CorridorBands {
    pub safe_max: f64,
    pub hard_max: f64,
}

fn clamp01(x: f64) -> f64 {
    if !x.is_finite() {
        0.0
    } else if x <= 0.0 {
        0.0
    } else if x >= 1.0 {
        1.0
    } else {
        x
    }
}

pub fn fold_pollutant_to_r(x: f64, bands: CorridorBands) -> f64 {
    if x <= bands.safe_max {
        0.0
    } else if x >= bands.hard_max {
        1.0
    } else {
        let num = x - bands.safe_max;
        let den = bands.hard_max - bands.safe_max;
        clamp01(num / den)
    }
}

#[derive(Clone, Debug, Deserialize)]
pub struct PhoenixWaterRow {
    pub nodeid: String,
    pub windowstartts: String,
    pub windowendts: String,
    pub bod_mg_per_l: f64,
    pub tss_mg_per_l: f64,
    pub n_mg_per_l: f64,
    pub p_mg_per_l: f64,
    pub cec_ug_per_l: f64,
    pub pfas_ng_per_l: f64,
}

#[derive(Clone, Debug, Serialize)]
pub struct RiskVectorWater {
    pub r_bod: f64,
    pub r_tss: f64,
    pub r_n: f64,
    pub r_p: f64,
    pub r_cec: f64,
    pub r_pfas: f64,
    pub r_bio: f64,
}

pub fn compute_vt_water(rv: &RiskVectorWater) -> f64 {
    let sq = |x: f64| x * x;
    let w_bod = 1.0;
    let w_tss = 1.0;
    let w_n = 1.0;
    let w_p = 1.0;
    let w_cec = 1.0;
    let w_pfas = 1.0;
    w_bod * sq(rv.r_bod)
        + w_tss * sq(rv.r_tss)
        + w_n * sq(rv.r_n)
        + w_p * sq(rv.r_p)
        + w_cec * sq(rv.r_cec)
        + w_pfas * sq(rv.r_pfas)
}

pub fn compute_ker_from_vt(vt: f64) -> (f64, f64, f64) {
    let k = 0.93;
    let e = 0.90;
    let r = vt.max(0.0).min(1.0);
    (k, e, r)
}

pub fn compute_evidence_hex(nodeid: &str, windowendts: &str, vt: f64) -> String {
    let payload = format!("{}|{}|{:.6}", nodeid, windowendts, vt);
    let mut acc: u64 = 0;
    for b in payload.as_bytes() {
        acc = acc.wrapping_mul(109);
        acc = acc.wrapping_add(*b as u64);
    }
    format!("0x{:016x}", acc)
}

pub fn risk_from_row(
    row: &PhoenixWaterRow,
    bod_bands: CorridorBands,
    tss_bands: CorridorBands,
    n_bands: CorridorBands,
    p_bands: CorridorBands,
    cec_bands: CorridorBands,
    pfas_bands: CorridorBands,
) -> RiskVectorWater {
    let r_bod = fold_pollutant_to_r(row.bod_mg_per_l, bod_bands);
    let r_tss = fold_pollutant_to_r(row.tss_mg_per_l, tss_bands);
    let r_n = fold_pollutant_to_r(row.n_mg_per_l, n_bands);
    let r_p = fold_pollutant_to_r(row.p_mg_per_l, p_bands);
    let r_cec = fold_pollutant_to_r(row.cec_ug_per_l, cec_bands);
    let r_pfas = fold_pollutant_to_r(row.pfas_ng_per_l, pfas_bands);
    let r_bio = clamp01(
        r_bod
            .max(r_tss)
            .max(r_n)
            .max(r_p)
            .max(r_cec)
            .max(r_pfas),
    );
    RiskVectorWater {
        r_bod,
        r_tss,
        r_n,
        r_p,
        r_cec,
        r_pfas,
        r_bio,
    }
}

pub fn ingest_csv_to_sqlite(
    db_path: &str,
    csv_path: &str,
    signingdid: &str,
    bod_bands: CorridorBands,
    tss_bands: CorridorBands,
    n_bands: CorridorBands,
    p_bands: CorridorBands,
    cec_bands: CorridorBands,
    pfas_bands: CorridorBands,
) -> rusqlite::Result<()> {
    let conn = Connection::open(db_path)?;
    let file = File::open(csv_path)?;
    let reader = BufReader::new(file);
    let mut lines = reader.lines();

    if lines.next().is_none() {
        return Ok(());
    }

    let ingested_at_utc = humantime::format_rfc3339(SystemTime::now()).to_string();

    for line in lines.flatten() {
        let cols: Vec<&str> = line.split(',').collect();
        if cols.len() < 8 {
            continue;
        }

        let row = PhoenixWaterRow {
            nodeid: cols[0].to_string(),
            windowstartts: cols[1].to_string(),
            windowendts: cols[2].to_string(),
            bod_mg_per_l: cols[3].parse().unwrap_or(0.0),
            tss_mg_per_l: cols[4].parse().unwrap_or(0.0),
            n_mg_per_l: cols[5].parse().unwrap_or(0.0),
            p_mg_per_l: cols[6].parse().unwrap_or(0.0),
            cec_ug_per_l: cols[7].parse().unwrap_or(0.0),
            pfas_ng_per_l: cols
                .get(8)
                .and_then(|v| v.parse().ok())
                .unwrap_or(0.0),
        };

        let rv = risk_from_row(
            &row,
            bod_bands,
            tss_bands,
            n_bands,
            p_bands,
            cec_bands,
            pfas_bands,
        );
        let vt = compute_vt_water(&rv);
        let (kerk, kere, kerr) = compute_ker_from_vt(vt);
        let evidencehex = compute_evidence_hex(&row.nodeid, &row.windowendts, vt);

        conn.execute(
            "INSERT INTO qpudatashard_water_quality (
               nodeid, windowstartts, windowendts,
               bod_mg_per_l, tss_mg_per_l, n_mg_per_l, p_mg_per_l,
               cec_ug_per_l, pfas_ng_per_l,
               r_bod, r_tss, r_n, r_p, r_cec, r_pfas, r_bio,
               vt, kerk, kere, kerr,
               evidencehex, signingdid, source_csv, ingested_at_utc
             ) VALUES (
               ?1, ?2, ?3,
               ?4, ?5, ?6, ?7,
               ?8, ?9,
               ?10, ?11, ?12, ?13, ?14, ?15, ?16,
               ?17, ?18, ?19, ?20,
               ?21, ?22, ?23, ?24
             )",
            params![
                row.nodeid,
                row.windowstartts,
                row.windowendts,
                row.bod_mg_per_l,
                row.tss_mg_per_l,
                row.n_mg_per_l,
                row.p_mg_per_l,
                row.cec_ug_per_l,
                row.pfas_ng_per_l,
                rv.r_bod,
                rv.r_tss,
                rv.r_n,
                rv.r_p,
                rv.r_cec,
                rv.r_pfas,
                rv.r_bio,
                vt,
                kerk,
                kere,
                kerr,
                evidencehex,
                signingdid.to_string(),
                csv_path.to_string(),
                ingested_at_utc.clone(),
            ],
        )?;
    }

    Ok(())
}
