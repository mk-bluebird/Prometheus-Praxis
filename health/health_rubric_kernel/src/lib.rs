// Prometheus-Praxis/rust/health/health_rubric_kernel/src/lib.rs
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
pub enum GateVerdict {
    Allow,
    Derate,
    Stop,
    Appeal,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct HealthRubricEnvelope {
    pub id: String,
    pub host_did: String,
    pub mt6883_window_id: String,
    pub risk_plane_coordinates_id: String,
    pub hestia_continuity_proof_id: Option<String>,

    // Seven-dimensional rubric projection
    pub k_score: f32,
    pub eco_score: f32,
    pub roh_score: f32,
    pub neurorights_ok: bool,
    pub continuity_grade: char, // 'A'..'E'
    pub consent_ok: bool,
    pub governance_ok: bool,

    // Non-offsettable healthcare coordinates
    pub rohscalarnorm: f32,
    pub thermalburdennorm: f32,
    pub nanoswarmburdennorm: f32,
    pub psychcontinuitypressure: f32,
    pub immune_status: ImmuneStatus,

    pub created_at_utc: String,
}

#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum ImmuneStatus {
    Green,
    Amber,
    Red,
}

/// Core healthcare gate for Perknos-Nexus high-intensity sessions:
/// RoH ≤ 0.30, thermal ≤ 0.30, immune Green, psychcontinuitypressure ≤ 0.40,
/// plus neurorights, consent, and governance invariants enforced.
pub fn check_health_gate(env: &HealthRubricEnvelope) -> GateVerdict {
    // Hard non-offsettable ceilings
    if env.rohscalarnorm > 0.30 {
        return GateVerdict::Stop;
    }
    if env.thermalburdennorm > 0.30 {
        return GateVerdict::Stop;
    }
    if env.immune_status != ImmuneStatus::Green {
        return GateVerdict::Stop;
    }
    if env.psychcontinuitypressure > 0.40 {
        return GateVerdict::Stop;
    }

    // Neuro-rights and consent invariants
    if !env.neurorights_ok {
        return GateVerdict::Stop;
    }
    if !env.consent_ok {
        return GateVerdict::Stop;
    }

    // Governance/audit invariants
    if !env.governance_ok {
        return GateVerdict::Stop;
    }

    // Continuity grade can modulate intensity (Derate vs Allow)
    match env.continuity_grade {
        'A' | 'B' => GateVerdict::Allow,
        'C' => GateVerdict::Derate,
        'D' | 'E' => GateVerdict::Stop,
        _ => GateVerdict::Appeal,
    }
}
