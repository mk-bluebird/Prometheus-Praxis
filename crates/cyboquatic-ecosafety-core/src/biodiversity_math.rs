//! CPVM-style biodiversity risk function over core risk coordinates.
//!
//! r_biodiv = 1 - Π_k (1 - w_k * r_k)
//!
//! This function is monotone in each risk coordinate and bounded in [0,1]
//! when r_k ∈ [0,1] and w_k ∈ [0,1]. It can be used both to compute
//! r_biodiv and to expose partial derivatives for sensitivity analysis.

/// Biodiversity sensitivity weights for each risk coordinate.
#[derive(Clone, Debug)]
pub struct BiodiversityWeights {
    pub w_pfas: f32,
    pub w_cec: f32,
    pub w_trap_fish: f32,
    pub w_trap_amphib: f32,
}

impl Default for BiodiversityWeights {
    fn default() -> Self {
        Self {
            w_pfas: 0.4,
            w_cec: 0.3,
            w_trap_fish: 0.2,
            w_trap_amphib: 0.1,
        }
    }
}

/// Compute CPVM-style biodiversity risk from four coordinates.
///
/// r_biodiv = 1 - Π_k (1 - w_k * r_k)
///
/// Caller should ensure r_k ∈ [0,1], w_k ∈ [0,1] for a strict [0,1] bound.
pub fn biodiversity_risk(
    r_pfas: f32,
    r_cec: f32,
    r_trap_fish: f32,
    r_trap_amphib: f32,
    w: &BiodiversityWeights,
) -> f32 {
    let t_p = 1.0 - w.w_pfas * r_pfas;
    let t_c = 1.0 - w.w_cec * r_cec;
    let t_f = 1.0 - w.w_trap_fish * r_trap_fish;
    let t_a = 1.0 - w.w_trap_amphib * r_trap_amphib;

    let viability = t_p * t_c * t_f * t_a;
    (1.0 - viability).clamp(0.0, 1.0)
}

/// Partial derivative ∂r_biodiv / ∂r_pfas.
pub fn dr_biodiv_dr_pfas(
    r_cec: f32,
    r_trap_fish: f32,
    r_trap_amphib: f32,
    w: &BiodiversityWeights,
) -> f32 {
    // ∂r_biodiv/∂r_p = w_p * Π_{k≠p}(1 - w_k r_k)
    let t_c = 1.0 - w.w_cec * r_cec;
    let t_f = 1.0 - w.w_trap_fish * r_trap_fish;
    let t_a = 1.0 - w.w_trap_amphib * r_trap_amphib;

    w.w_pfas * t_c * t_f * t_a
}
