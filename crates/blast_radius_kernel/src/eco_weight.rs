// filename: eco_weight.rs
// destination: ecorestoration_shard/blast_radius_kernel/src/eco_weight.rs

use rusqlite::{params, Connection, Result};

pub struct EcoWeightConfig {
    pub non_offsettable_carbon: bool,
    pub non_offsettable_bio: bool,
    pub restoration_bonus: f64,
    pub eco_weight_min: f64,
    pub eco_weight_max: f64,
}

pub fn load_eco_weight_for_segment(
    conn: &Connection,
    segment_id: i64,
    region_code: &str,
) -> Result<EcoWeightConfig> {
    let mut stmt = conn.prepare(
        r#"
        SELECT
            non_offsettable_carbon,
            non_offsettable_bio,
            restoration_bonus,
            eco_weight_min,
            eco_weight_max
        FROM blast_radius_eco_weight
        WHERE segment_id = ?1 AND region_code = ?2
        "#,
    )?;

    stmt.query_row(params![segment_id, region_code], |row| {
        let non_offsettable_carbon: i64 = row.get(0)?;
        let non_offsettable_bio: i64 = row.get(1)?;
        let restoration_bonus: f64 = row.get(2)?;
        let eco_weight_min: f64 = row.get(3)?;
        let eco_weight_max: f64 = row.get(4)?;

        Ok(EcoWeightConfig {
            non_offsettable_carbon: non_offsettable_carbon != 0,
            non_offsettable_bio: non_offsettable_bio != 0,
            restoration_bonus,
            eco_weight_min,
            eco_weight_max,
        })
    })
}

/// Compute the final eco weight factor in [eco_weight_min, eco_weight_max].
/// The function rewards non-offsettable carbon/bio flags and restoration_bonus,
/// but respects configured bounds.
pub fn compute_eco_weight_factor(config: &EcoWeightConfig) -> f64 {
    let mut weight = 1.0;

    if config.non_offsettable_carbon {
        weight += 0.25;
    }

    if config.non_offsettable_bio {
        weight += 0.25;
    }

    if config.restoration_bonus > 0.0 {
        weight += config.restoration_bonus;
    }

    if weight < config.eco_weight_min {
        config.eco_weight_min
    } else if weight > config.eco_weight_max {
        config.eco_weight_max
    } else {
        weight
    }
}
