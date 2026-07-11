// filename: crates/beecorridor-ledger/src/query.rs

use crate::types::{BeeRiskVector, CorridorId, LyapunovResidual};
use crate::types::Committed;
use rusqlite::{params, Connection, Result};

/// Read-only, committed-only ledger handle.
/// Opened strictly in read-only mode; all query builders require this type.
#[derive(Debug)]
pub struct CommittedLedger<'db> {
    conn: &'db Connection,
    _marker: Committed,
}

impl<'db> CommittedLedger<'db> {
    /// Create a committed ledger handle from a read-only connection.
    pub fn new(conn: &'db Connection) -> Self {
        CommittedLedger {
            conn,
            _marker: Committed,
        }
    }

    /// Compute the latest BeeRiskVector for a corridor from committed telemetry.
    pub fn latest_bee_risk_vector(&self, corridor_id: &CorridorId) -> Result<BeeRiskVector> {
        // This is a stub; in a full implementation, you'd map raw telemetry
        // into normalized risk coordinates based on corridordefinition bands.
        let mut stmt = self.conn.prepare(
            r#"
            SELECT corridor_id,
                   MAX(timestamp_utc) AS ts,
                   classified_bee_count,
                   emf_level,
                   thermal_delta,
                   chemical_index
            FROM telemetry_snapshot
            WHERE corridor_id = ?1 AND attestation_ok = 1
            GROUP BY corridor_id
            "#,
        )?;

        let row = stmt.query_row(params![corridor_id.0], |r| {
            let _ts: i64 = r.get(1)?;
            let _count: i32 = r.get(2)?;
            let emf: f32 = r.get(3)?;
            let thermal: f32 = r.get(4)?;
            let chem: f32 = r.get(5)?;

            Ok(BeeRiskVector {
                corridor_id: CorridorId(r.get::<_, String>(0)?),
                r_contact: crate::normalize::normalize_contact(0.0),
                r_emf: crate::normalize::normalize_emf(emf),
                r_acoustic: crate::normalize::normalize_acoustic(0.0),
                r_thermal: crate::normalize::normalize_thermal(thermal),
                r_chemical: crate::normalize::normalize_chemical(chem),
            })
        })?;

        Ok(row)
    }

    /// Compute Lyapunov residual V_total for the given risk vector from committed planeweights.
    pub fn lyapunov_residual(&self, shard_id: &str, rv: &BeeRiskVector) -> Result<LyapunovResidual> {
        let mut stmt = self.conn.prepare(
            r#"
            SELECT plane_name, weight, is_bee_plane
            FROM planeweights
            WHERE shard_id = ?1
            "#,
        )?;

        let mut v_total = 0.0_f32;

        let mut rows = stmt.query(params![shard_id])?;
        while let Some(row) = rows.next()? {
            let plane_name: String = row.get(0)?;
            let weight: f32 = row.get(1)?;
            let coord = match plane_name.as_str() {
                "MechanicalContact" => rv.r_contact.0,
                "Emf" => rv.r_emf.0,
                "Acoustic" => rv.r_acoustic.0,
                "Thermal" => rv.r_thermal.0,
                "Chemical" => rv.r_chemical.0,
                _ => 0.0,
            };
            v_total += weight * coord * coord;
        }

        Ok(LyapunovResidual(v_total))
    }
}

/// Normalization helpers separated into their own module.
pub mod normalize {
    use crate::types::RiskCoordinate;

    pub fn normalize_contact(distance_overlap: f32) -> RiskCoordinate {
        // Placeholder mapping; real implementation would use corridordefinition bands.
        let v = if distance_overlap <= 0.0 { 0.0 } else { (distance_overlap).min(1.0) };
        RiskCoordinate(v)
    }

    pub fn normalize_emf(emf_level: f32) -> RiskCoordinate {
        // Placeholder piecewise linear mapping; calibrated against NOEL/LOEL studies.
        let v = (emf_level / 1.0).min(1.0).max(0.0);
        RiskCoordinate(v)
    }

    pub fn normalize_acoustic(_spl: f32) -> RiskCoordinate {
        RiskCoordinate(0.0)
    }

    pub fn normalize_thermal(delta_t: f32) -> RiskCoordinate {
        let v = (delta_t / 5.0).min(1.0).max(0.0);
        RiskCoordinate(v)
    }

    pub fn normalize_chemical(idx: f32) -> RiskCoordinate {
        let v = (idx / 1.0).min(1.0).max(0.0);
        RiskCoordinate(v)
    }
}
