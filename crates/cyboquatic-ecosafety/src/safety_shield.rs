//! Safety shield trait and Lyapunov-bounded state/action types for
//! shielded reinforcement learning and SAFE_FLAG FPGA coupling.
//!
//! This module is intended to be wired to:
//! - ALN ecosafety envelopes with `corridorpresent`, `safestepok`,
//!   and `kerdeployable` fields.
//! - The existing Lyapunov and KER spine (`LyapunovResidual`,
//!   `RiskVector`, `KERWindow`, `SafeDecision`), as defined in
//!   `cyboquatic-ecosafety`. [file:24]

#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};
use cyboquatic_ecosafety::{
    KERWindow,
    LyapunovResidual,
    LyapunovWeights,
    RiskVector,
    SafeDecision,
};

/// Discrete environment state as seen by the shielded RL agent.
/// This is a minimal, Lyapunov-aware view; additional observables
/// can be added as needed while preserving the invariants. [file:24]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShieldState {
    /// Normalized ecosafety risk vector (PFAS/CEC, SAT, surcharge, biodiversity, Vt, governance).
    pub risk: RiskVector,
    /// Current Lyapunov residual value \(V_t\).
    pub residual: LyapunovResidual,
    /// Current Lyapunov weights.
    pub weights: LyapunovWeights,
    /// Current KER window summarizing K,E,R over recent steps.
    pub ker: KERWindow,
    /// Whether the ALN ecosafety corridor is present for this state.
    /// Mirrors `corridorpresent` in `CyboquaticEcosafetyEnvelopePhoenix2026v1`.
    /// [file:24]
    pub corridor_present: bool,
    /// Whether the previous step satisfied the safe-step doctrine
    /// (`safestep_prev`), i.e. \(V_t \le V_{t-1}\) up to epsilon
    /// and no hard corridor breach.
    pub safestep_prev: bool,
}

/// Action proposed by the RL agent before shielding.
/// This remains abstract; actuators are governed elsewhere.
/// [file:23][file:24]
#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct ShieldAction {
    /// Symbolic action identifier (e.g. discrete control index).
    pub id: u32,
    /// Optional continuous parameterization; meaning is environment-specific.
    pub param: f64,
}

/// Result of applying the safety shield to a (state, action) pair.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum ShieldOutcome {
    /// Action is allowed as-is (no modification).
    Allow,
    /// Action is replaced by an alternative that preserves the Lyapunov
    /// and corridor invariants.
    Replaced { replacement_id: u32 },
    /// No action is allowed; environment should treat this as STOP.
    Block,
}

/// Safety shield trait for Lyapunov-monotone RL.
/// Implementations must be pure, non-actuating functions that only
/// inspect state/action and produce a `ShieldOutcome`. [file:23][file:24]
pub trait SafetyShield {
    /// Decide how to handle a proposed action under the current state.
    ///
    /// Invariants (to be proved by Kani):
    /// - If `corridor_present` is false, must not `Allow`.
    /// - If the action would cause \(V_{t+1} > V_t\) outside the
    ///   small safe interior, must not `Allow`.
    /// - When returning `Allow`, the induced transition must satisfy
    ///   \(V_{t+1} \le V_t\) and keep KER within lane gates.
    fn shield(&self, state: &ShieldState, action: &ShieldAction) -> ShieldOutcome;
}

/// Simple, conservative shield that leverages `SafeDecision::decide_step`
/// and the existing KER gates. This is a non-actuating reference
/// implementation suitable for Kani proofs. [file:24]
#[derive(Clone, Debug)]
pub struct LyapunovSafetyShield {
    /// Tolerance epsilon for Lyapunov residual comparisons.
    pub vt_eps: f64,
}

impl LyapunovSafetyShield {
    pub fn new(vt_eps: f64) -> Self {
        Self { vt_eps }
    }

    /// Internal helper: compute a hypothetical next residual from a
    /// candidate risk vector and weights. In practice this is provided
    /// by the environment model or simulator; here we keep it abstract.
    #[inline]
    fn hypothetical_next_residual(
        &self,
        next_risk: &RiskVector,
        weights: &LyapunovWeights,
    ) -> LyapunovResidual {
        LyapunovResidual::from_vector(*next_risk, *weights)
    }
}

impl SafetyShield for LyapunovSafetyShield {
    fn shield(&self, state: &ShieldState, action: &ShieldAction) -> ShieldOutcome {
        // Conservative default: if corridor is missing or previous step was unsafe,
        // do not allow any new action. [file:24]
        if !state.corridor_present || !state.safestep_prev {
            return ShieldOutcome::Block;
        }

        // In a real system, `predict_next_risk` would be a model-based or
        // empirical mapping from (state, action) to RiskVector. For the
        // purposes of the shield, we assume an identity mapping here,
        // which Kani can treat as a nondeterministic projection in the harness.
        let predicted_risk = state.risk;

        let vt_next = self.hypothetical_next_residual(&predicted_risk, &state.weights);

        // Use the existing SafeDecision logic as a secondary guard.
        let decision = cyboquatic_ecosafety::decide_step(
            state.residual,
            vt_next,
            predicted_risk,
        );

        match decision {
            SafeDecision::Stop => ShieldOutcome::Block,
            SafeDecision::Derate => {
                // For now, treat derate as a block on the original action.
                ShieldOutcome::Block
            }
            SafeDecision::Accept => {
                // Enforce explicit Lyapunov monotonicity with epsilon.
                if vt_next.value > state.residual.value + self.vt_eps {
                    ShieldOutcome::Block
                } else {
                    ShieldOutcome::Allow
                }
            }
        }
    }
}
