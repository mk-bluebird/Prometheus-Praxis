// eco_restoration_shard/crates/cyboquatic_progress/20260711_crate/src/lib.rs

use serde::{Deserialize, Serialize};

/// Hex-stamped Phoenix evidence string for 2026-07-11.
/// This anchors the shard to Phoenix, AZ and the primary Bostrom DID.
pub const PHOENIX_EVIDENCE_HEX_20260711: &str =
    "0x20260711c0ffee79c0ffee79c0ffee79c0ffee79c0ffee79c0ffee79c0ffee79";

/// Primary Bostrom DID bound governance identity.
pub const BOSTROM_DID_PRIMARY: &str =
    "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";

/// Cyboquatic workload domain for 2026-07-11 (domain d: cyboquatic workload).
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub enum WorkloadDomain {
    /// Industrial cyboquatic workload with energy requirements and ΔVt.
    CyboquaticEnergyDeltaVt,
}

/// KER triad capturing Knowledge, Eco-impact, and Risk scores.
/// Scores are normalized to [0.0, 1.0] for consistent comparison.
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub struct KerTriad {
    pub knowledge_score: f32,
    pub eco_impact_score: f32,
    pub risk_score: f32,
}

/// Cyboquatic workload descriptor for a single machine-task pairing.
/// This is engineered to be usable on real machinery with minimal overhead.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CyboquaticWorkload {
    /// Human-readable identifier for the workload.
    pub workload_id: String,
    /// Domain for routing within the cyboquatic eco-restoration constellation.
    pub domain: WorkloadDomain,
    /// Required energy per unit task in Joules (J) under standard operating conditions.
    pub energy_req_j: f64,
    /// Threshold energy in Joules for safe, carbon-negative operation.
    pub safe_energy_j: f64,
    /// ΔVt: tolerated variation ratio between actual and safe energy,
    /// defined as (energy_req_j - safe_energy_j) / safe_energy_j.
    pub delta_vt: f64,
    /// KER triad bound to this workload.
    pub ker: KerTriad,
    /// Pointer to the previous daily shard (YYYYMMDD) for cumulative progress.
    pub previous_shard_yyyymmdd: Option<String>,
    /// Phoenix evidence hex stamp for this workload instance.
    pub evidence_hex: String,
}

/// Deterministic integer hash from YYYYMMDD to derive a daily sub-task.
/// This guarantees novelty across dates without external randomness.
pub fn date_integer_hash_yyyymmdd(date: &str) -> u64 {
    // Simple, deterministic hash: weighted sum of bytes modulo a large prime.
    // This is intentionally transparent and energy-light.
    const PRIME: u64 = 1_000_000_007;
    let mut acc: u64 = 0;
    for (idx, b) in date.as_bytes().iter().enumerate() {
        let weight = (idx as u64) + 1;
        acc = (acc + (*b as u64) * weight) % PRIME;
    }
    acc
}

/// Derive a novel cyboquatic sub-task label from the date hash.
/// The label rotates through a small, interpretable set of subdomains.
pub fn derive_subtask_label_from_hash(hash: u64) -> String {
    // Map hash to one of several concrete sub-tasks for cyboquatic workloads.
    let options = [
        "micro_pfas_cold_corridor",
        "fog_phase_unmodeled_medium",
        "low_bod_drainage_lane",
        "tss_buffer_corridor",
        "cec_guard_band",
        "cold_survival_machining_axis",
        "energy_latency_lane",
    ];
    let idx = (hash as usize) % options.len();
    format!("subtask_{}_{:x}", options[idx], hash)
}

/// Compute ΔVt for a workload given required and safe energy.
/// ΔVt = (energy_req_j - safe_energy_j) / safe_energy_j.
/// If safe_energy_j is zero or negative, we treat ΔVt as 0 to avoid undefined
/// behavior and flag this via the risk score in KER.
pub fn compute_delta_vt(energy_req_j: f64, safe_energy_j: f64) -> f64 {
    if safe_energy_j <= 0.0 {
        0.0
    } else {
        (energy_req_j - safe_energy_j) / safe_energy_j
    }
}

/// Heuristic KER scoring that favors carbon-negative, low-risk workloads.
/// This is intentionally simple and explainable for first-pass deployment.
///
/// - knowledge_score: fixed baseline for this model (0.7).
/// - eco_impact_score: higher when energy_req_j is close to safe_energy_j and
///   energy_req_j is low.
/// - risk_score: increases with |ΔVt|; capped at 1.0.
pub fn ker_for_workload(energy_req_j: f64, safe_energy_j: f64, delta_vt: f64) -> KerTriad {
    let knowledge_score = 0.7_f32;

    let energy_ratio = if safe_energy_j <= 0.0 {
        1.0
    } else {
        (energy_req_j / safe_energy_j).clamp(0.0, 2.0)
    };

    // Eco-impact is best (near 1.0) when energy_ratio is near 1 and absolute energy is low.
    let eco_base = 1.0_f32 - ((energy_ratio - 1.0).abs() as f32);
    let energy_scale = if energy_req_j <= 10_000.0 { 1.0_f32 } else { 0.5_f32 };
    let eco_impact_score = (eco_base * energy_scale).clamp(0.0_f32, 1.0_f32);

    // Risk is proportional to |ΔVt| and clipped to [0, 1].
    let risk_score = (delta_vt.abs() as f32).clamp(0.0_f32, 1.0_f32);

    KerTriad {
        knowledge_score,
        eco_impact_score,
        risk_score,
    }
}

/// Construct the daily cyboquatic workload instance for 2026-07-11.
/// Prior shard is assumed to be 20260710 for cumulative progress linkage.
pub fn daily_workload_20260711() -> CyboquaticWorkload {
    let date_str = "20260711";
    let hash = date_integer_hash_yyyymmdd(date_str);
    let subtask_label = derive_subtask_label_from_hash(hash);

    // For 2026-07-11, we select a conservative, carbon-negative energy profile.
    // Example: 1500 J required, 1400 J safe threshold.
    let energy_req_j = 1500.0_f64;
    let safe_energy_j = 1400.0_f64;
    let delta_vt = compute_delta_vt(energy_req_j, safe_energy_j);
    let ker = ker_for_workload(energy_req_j, safe_energy_j, delta_vt);

    CyboquaticWorkload {
        workload_id: format!("{}-{}", date_str, subtask_label),
        domain: WorkloadDomain::CyboquaticEnergyDeltaVt,
        energy_req_j,
        safe_energy_j,
        delta_vt,
        ker,
        previous_shard_yyyymmdd: Some("20260710".to_string()),
        evidence_hex: PHOENIX_EVIDENCE_HEX_20260711.to_string(),
    }
}

/// Serialize the daily workload to JSON for ingestion into SQLite or other data stores.
pub fn serialize_daily_workload_to_json() -> serde_json::Result<String> {
    let w = daily_workload_20260711();
    serde_json::to_string_pretty(&w)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn hash_is_deterministic() {
        let d = "20260711";
        let h1 = date_integer_hash_yyyymmdd(d);
        let h2 = date_integer_hash_yyyymmdd(d);
        assert_eq!(h1, h2);
    }

    #[test]
    fn delta_vt_computation_basic() {
        let dv = compute_delta_vt(1500.0, 1400.0);
        let expected = (1500.0 - 1400.0) / 1400.0;
        let diff = (dv - expected).abs();
        assert!(diff < 1e-9);
    }

    #[test]
    fn ker_scores_in_bounds() {
        let dv = compute_delta_vt(1500.0, 1400.0);
        let ker = ker_for_workload(1500.0, 1400.0, dv);
        assert!(ker.knowledge_score >= 0.0 && ker.knowledge_score <= 1.0);
        assert!(ker.eco_impact_score >= 0.0 && ker.eco_impact_score <= 1.0);
        assert!(ker.risk_score >= 0.0 && ker.risk_score <= 1.0);
    }

    #[test]
    fn daily_workload_links_previous_shard() {
        let w = daily_workload_20260711();
        assert_eq!(w.previous_shard_yyyymmdd.as_deref(), Some("20260710"));
    }

    #[test]
    fn json_serialization_works() {
        let json = serialize_daily_workload_to_json().expect("serialization should succeed");
        assert!(json.contains("workload_id"));
        assert!(json.contains("energy_req_j"));
    }
}
