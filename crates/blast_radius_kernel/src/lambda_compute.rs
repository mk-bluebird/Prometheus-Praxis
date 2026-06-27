// filename: lambda_compute.rs
// destination: ecorestoration_shard/blast_radius_kernel/src/lambda_compute.rs

use rusqlite::{params, Connection, Result};
use time::{OffsetDateTime, format_description::well_known::Rfc3339};

use crate::eco_weight::{compute_eco_weight_factor, load_eco_weight_for_segment};
use crate::model::{LambdaQuery, LambdaSummary};

fn now_utc_string() -> String {
    OffsetDateTime::now_utc()
        .format(&Rfc3339)
        .unwrap_or_else(|_| "1970-01-01T00:00:00Z".to_string())
}

fn load_substrate_kinetics(
    conn: &Connection,
    query: &LambdaQuery,
) -> Result<(String, f64)> {
    let mut stmt = conn.prepare(
        r#"
        SELECT substrate_kind, k_base_per_day
        FROM substrate_kinetics
        WHERE substrate_kind = (
            SELECT assetkind
            FROM cyboasset
            WHERE assetid = ?1
        )
        AND contaminant_code = ?2
        AND season_code = ?3
        "#,
    )?;

    stmt.query_row(
        params![query.segment_id, query.contaminant_code, query.season_code],
        |row| {
            let substrate_kind: String = row.get(0)?;
            let k_base_per_day: f64 = row.get(1)?;
            Ok((substrate_kind, k_base_per_day))
        },
    )
}

fn load_latest_velocity_snapshot(
    conn: &Connection,
    segment_id: i64,
    region_code: &str,
) -> Result<(f64, f64, i64, String, String)> {
    let mut stmt = conn.prepare(
        r#"
        SELECT v_mean_m_per_s,
               v_std_m_per_s,
               telemetry_span_s,
               t_start_utc,
               t_end_utc
        FROM segment_velocity_snapshot
        WHERE segment_id = ?1 AND region_code = ?2
        ORDER BY t_end_utc DESC
        LIMIT 1
        "#,
    )?;

    stmt.query_row(params![segment_id, region_code], |row| {
        let v_mean: f64 = row.get(0)?;
        let v_std: f64 = row.get(1)?;
        let span_s: i64 = row.get(2)?;
        let t_start: String = row.get(3)?;
        let t_end: String = row.get(4)?;
        Ok((v_mean, v_std, span_s, t_start, t_end))
    })
}

/// Compute lambda = k_eff / v_mean for a segment, apply eco weights, and
/// persist into blast_radius_lambda_cache. Returns the summary struct.
pub fn compute_lambda_for_segment(
    conn: &Connection,
    query: &LambdaQuery,
) -> Result<LambdaSummary> {
    let (substrate_kind, k_base_per_day) = load_substrate_kinetics(conn, query)?;
    let (v_mean, v_std, span_s, t_start, t_end) =
        load_latest_velocity_snapshot(conn, query.segment_id, &query.region_code)?;

    if v_mean <= 0.0 {
        return Err(rusqlite::Error::UserFunctionError(
            "non-positive velocity in lambda computation".into(),
        ));
    }

    let eco_cfg = load_eco_weight_for_segment(conn, query.segment_id, &query.region_code)?;
    let eco_weight = compute_eco_weight_factor(&eco_cfg);

    // Convert k_base_per_day to per-second, then to per-meter via v_mean.
    let k_eff_per_day = k_base_per_day;
    let k_eff_per_s = k_eff_per_day / 86400.0;

    let lambda_base_per_m = k_eff_per_s / v_mean;

    // Simple confidence bounds using v_std as a proxy.
    // Lower bound assumes v_mean + v_std, upper bound assumes v_mean - v_std (clipped).
    let v_high = (v_mean + v_std).max(1e-6);
    let v_low = (v_mean - v_std).max(1e-6);

    let lambda_min_per_m = (k_eff_per_s / v_high) * eco_weight;
    let lambda_max_per_m = (k_eff_per_s / v_low) * eco_weight;

    let lambda_eff_per_m = lambda_base_per_m * eco_weight;

    let created_utc = now_utc_string();

    conn.execute(
        r#"
        INSERT INTO blast_radius_lambda_cache (
            segment_id,
            region_code,
            contaminant_code,
            substrate_kind,
            k_eff_per_day,
            v_mean_m_per_s,
            eco_weight_applied,
            lambda_eff_per_m,
            lambda_eff_min_per_m,
            lambda_eff_max_per_m,
            telemetry_span_s,
            t_snapshot_start_utc,
            t_snapshot_end_utc,
            created_utc,
            updated_utc
        )
        VALUES (
            ?1, ?2, ?3, ?4,
            ?5, ?6, ?7,
            ?8, ?9, ?10,
            ?11, ?12, ?13,
            ?14, ?14
        )
        "#,
        params![
            query.segment_id,
            query.region_code,
            query.contaminant_code,
            substrate_kind,
            k_eff_per_day,
            v_mean,
            eco_weight,
            lambda_eff_per_m,
            lambda_min_per_m,
            lambda_max_per_m,
            span_s,
            t_start,
            t_end,
            created_utc
        ],
    )?;

    Ok(LambdaSummary {
        segment_id: query.segment_id,
        region_code: query.region_code.clone(),
        contaminant_code: query.contaminant_code.clone(),
        substrate_kind,
        k_eff_per_day,
        v_mean_m_per_s: v_mean,
        eco_weight_applied: eco_weight,
        lambda_eff_per_m,
        lambda_eff_min_per_m: lambda_min_per_m,
        lambda_eff_max_per_m: lambda_max_per_m,
        telemetry_span_s: span_s,
        t_snapshot_start_utc: t_start,
        t_snapshot_end_utc: t_end,
    })
}

pub fn list_latest_lambda_for_region(
    conn: &Connection,
    region_code: &str,
) -> Result<Vec<LambdaSummary>> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            segment_id,
            region_code,
            contaminant_code,
            substrate_kind,
            k_eff_per_day,
            v_mean_m_per_s,
            eco_weight_applied,
            lambda_eff_per_m,
            lambda_eff_min_per_m,
            lambda_eff_max_per_m,
            telemetry_span_s,
            t_snapshot_start_utc,
            t_snapshot_end_utc
        FROM blast_radius_lambda_cache
        WHERE region_code = ?1
        AND t_snapshot_end_utc = (
            SELECT MAX(t_snapshot_end_utc)
            FROM blast_radius_lambda_cache
            WHERE region_code = ?1
        )
        "#,
    )?;

    let iter = stmt.query_map(params![region_code], |row| {
        Ok(LambdaSummary {
            segment_id: row.get(0)?,
            region_code: row.get(1)?,
            contaminant_code: row.get(2)?,
            substrate_kind: row.get(3)?,
            k_eff_per_day: row.get(4)?,
            v_mean_m_per_s: row.get(5)?,
            eco_weight_applied: row.get(6)?,
            lambda_eff_per_m: row.get(7)?,
            lambda_eff_min_per_m: row.get(8)?,
            lambda_eff_max_per_m: row.get(9)?,
            telemetry_span_s: row.get(10)?,
            t_snapshot_start_utc: row.get(11)?,
            t_snapshot_end_utc: row.get(12)?,
        })
    })?;

    let mut out = Vec::new();
    for item in iter {
        out.push(item?);
    }
    Ok(out)
}
