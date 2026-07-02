// filename:crates/econet_governance_spine/src/eco_wealth_portfolio.rs
// Rust 2024, rust-version = "1.85", MIT OR Apache-2.0
#![forbid(unsafe_code)]

use std::os::raw::c_char;

use rusqlite::{params, Connection};
use serde::Serialize;

use crate::{GovernanceSpine, EcoWealthStatement, SpineError, ShardIndex};
use crate::schema::ExpectedSchema;

/// Aggregated eco-wealth portfolio per shard.
#[derive(Debug, Serialize)]
pub struct EcoWealthPortfolioSummary {
    pub shardid: String,
    pub steward_count: u64,
    pub total_wealthscore: f64,
    pub mean_keffective: f64,
    pub mean_eeffective: f64,
    pub mean_reffective: f64,
}

/// Internal helper: compute eco-wealth portfolio summary for a shard.
fn query_ecowealth_portfolio(
    conn: &Connection,
    shardid: &str,
) -> Result<EcoWealthPortfolioSummary, SpineError> {
    if shardid.is_empty() {
        return Err(SpineError::InvalidArgument(
            "Shard ID cannot be empty".to_string(),
        ));
    }

    let mut stmt = conn.prepare(
        r#"
        SELECT stewarddid, shardid, wealthscore, keffective, eeffective, reffective
        FROM vstewardecowealthlatest
        WHERE shardid = ?1
        "#,
    )?;

    let mut count: u64 = 0;
    let mut sum_wealth: f64 = 0.0;
    let mut sum_k: f64 = 0.0;
    let mut sum_e: f64 = 0.0;
    let mut sum_r: f64 = 0.0;

    let mut shard_id_opt: Option<String> = None;

    let rows = stmt.query_map(params![shardid], |row| {
        Ok(EcoWealthStatement {
            stewarddid: row.get(0)?,
            shardid: row.get(1)?,
            wealthscore: row.get(2)?,
            keffective: row.get(3)?,
            eeffective: row.get(4)?,
            reffective: row.get(5)?,
        })
    })?;

    for row_res in rows {
        let ew = row_res?;
        shard_id_opt.get_or_insert_with(|| ew.shardid.clone());
        count += 1;
        sum_wealth += ew.wealthscore;
        sum_k += ew.keffective;
        sum_e += ew.eeffective;
        sum_r += ew.reffective;
    }

    let shard_final = shard_id_opt.unwrap_or_else(|| shardid.to_string());

    if count == 0 {
        // No steward rows for this shard; treat portfolio as empty with zero scores.
        return Ok(EcoWealthPortfolioSummary {
            shardid: shard_final,
            steward_count: 0,
            total_wealthscore: 0.0,
            mean_keffective: 0.0,
            mean_eeffective: 0.0,
            mean_reffective: 0.0,
        });
    }

    let mean_k = sum_k / (count as f64);
    let mean_e = sum_e / (count as f64);
    let mean_r = sum_r / (count as f64);

    Ok(EcoWealthPortfolioSummary {
        shardid: shard_final,
        steward_count: count,
        total_wealthscore: sum_wealth,
        mean_keffective: mean_k,
        mean_eeffective: mean_e,
        mean_reffective: mean_r,
    })
}

// FFI macro is defined in crateseconet-governance-spine/src/lib.rs as `implffiquery!`,
// generating extern "C" functions returning `*mut c_char` JSON pointers.

implffiquery! {
    #[no_mangle]
    pub extern "C" fn econet_ecowealth_portfolio_for_shard(
        handle: *mut ShardIndex,
        shardid: *const c_char,
    ) -> *mut c_char {
        if handle.is_null() {
            return Err(
                SpineError::InvalidArgument(
                    "Invalid null ShardIndex handle provided to econet_ecowealth_portfolio_for_shard"
                        .to_string(),
                )
                .to_string(),
            );
        }

        // Convert C string to &str using shared helper.
        let shardid_str = crate::cstrtostr(shardid)
            .map_err(|e| e.to_string())?;

        if shardid_str.is_empty() {
            return Err(
                SpineError::InvalidArgument("Shard ID cannot be empty".to_string()).to_string(),
            );
        }

        // Access the shared connection from the opaque handle.
        let shard_index = unsafe { &*handle };
        let conn = &shard_index.conn;

        // Compute aggregated eco-wealth portfolio summary.
        let summary = query_ecowealth_portfolio(conn, shardid_str)?;
        Ok(summary)
    }
}
