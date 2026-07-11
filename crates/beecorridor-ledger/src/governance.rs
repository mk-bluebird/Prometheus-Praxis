// filename: crates/beecorridor-ledger/src/governance.rs

use crate::query::CommittedLedger;
use crate::types::{BeeRiskVector, CorridorId, LyapunovResidual};
use rusqlite::{params, Result};

/// KerDeployable decision for a corridor, based on risk vector and Lyapunov residual.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct KerDeployableDecision {
    pub admissible: bool,
    pub reason: Option<&'static str>,
}

/// Check non-offsettable mechanical contact and Lyapunov safestep gate.
/// This function is intended to be verified with Kani.
pub fn kerdeployable_for_corridor(
    ledger: &CommittedLedger<'_>,
    shard_id: &str,
    corridor_id: &CorridorId,
    current_v: LyapunovResidual,
) -> Result<KerDeployableDecision> {
    let rv = ledger.latest_bee_risk_vector(corridor_id)?;
    let next_v = ledger.lyapunov_residual(shard_id, &rv)?;

    // Non-offsettable mechanical contact: any r_contact > 0 is a structural rejection.
    if rv.r_contact.0 > 0.0 {
        return Ok(KerDeployableDecision {
            admissible: false,
            reason: Some("non-offsettable contact risk"),
        });
    }

    // Safestep gate outside the Bee Safety Kernel: require V_next <= V_current.
    if next_v.0 > current_v.0 {
        return Ok(KerDeployableDecision {
            admissible: false,
            reason: Some("Lyapunov residual increased"),
        });
    }

    Ok(KerDeployableDecision {
        admissible: true,
        reason: None,
    })
}

/// Check bee-dominant weight mass invariant (bee planes >= 70%).
pub fn bee_weight_mass_invariant(conn: &rusqlite::Connection, shard_id: &str) -> Result<bool> {
    let mut stmt = conn.prepare(
        r#"
        SELECT bee_mass, total_mass
        FROM v_bee_weight_mass
        WHERE shard_id = ?1
        "#,
    )?;

    let row = stmt.query_row(params![shard_id], |r| {
        let bee_mass: f32 = r.get(0)?;
        let total_mass: f32 = r.get(1)?;
        Ok((bee_mass, total_mass))
    })?;

    let (bee_mass, total_mass) = row;
    if total_mass <= 0.0 {
        Ok(false)
    } else {
        Ok(bee_mass / total_mass >= 0.7)
    }
}
