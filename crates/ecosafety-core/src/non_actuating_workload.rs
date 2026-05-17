// filename: ecosafety-core/src/non_actuating_workload.rs
// destination: ecosafety-core/src/non_actuating_workload.rs

#![forbid(unsafe_code)]

use crate::{RiskVector, LyapunovResidual, KerSnapshot, PlaneId, PlaneWeight};

/// Non-actuating workload contract: pure computation over telemetry / shards.
/// Implementors MUST NOT depend on crates that expose actuator APIs.
pub trait NonActuatingWorkload {
    /// Input is typically an ALN-backed shard or telemetry snapshot.
    type Input;

    /// Output MUST include full RiskVector, residual, and KER window.
    type Output: WorkloadResultView;

    /// Plane weights used in residual computation, from PlaneWeightsShard2026v1.aln.
    type PlaneWeights: PlaneWeightsProvider;

    /// Compute Lyapunov residual V_t from a normalized RiskVector and plane weights.
    fn compute_residual(
        &self,
        risk: &RiskVector,
        weights: &Self::PlaneWeights,
    ) -> LyapunovResidual;

    /// Compute K, E, R snapshot from residual and risk / plane weights.
    fn compute_ker(
        &self,
        residual: &LyapunovResidual,
        risk: &RiskVector,
        weights: &Self::PlaneWeights,
    ) -> KerSnapshot;

    /// Safestep check: enforce hard bands and V_{t+1} <= V_t (within configured epsilon).
    fn check_safestep(
        &self,
        prev: &LyapunovResidual,
        next: &LyapunovResidual,
        risk_prev: &RiskVector,
        risk_next: &RiskVector,
        weights: &Self::PlaneWeights,
    ) -> SafestepDecision;

    /// Pure, non-actuating execution over a single input shard / snapshot.
    /// Implementations MUST satisfy:
    /// - Boundedness: for all j, r_j in [0,1].
    /// - Monotonicity in harmful directions (per-plane contract).
    /// - Safestep under corridors in admissible lanes (V_{t+1} <= V_t).
    fn execute(&self, input: Self::Input) -> Self::Output;
}

/// Minimal view over a workload result for invariant tests and callers.
pub trait WorkloadResultView {
    /// Normalized risk coordinates r_j in [0,1].
    fn risk_coords(&self) -> &[f32];

    /// Lyapunov residual V_t = sum_j w_j r_j^2.
    fn residual(&self) -> &LyapunovResidual;

    /// K/E/R snapshot.
    fn ker(&self) -> &KerSnapshot;
}

/// Provider for plane weights, aligned with PlaneWeightsShard2026v1.aln.
pub trait PlaneWeightsProvider {
    /// Return the weight w_j for a given plane / coordinate.
    fn weight_for(&self, plane: PlaneId) -> PlaneWeight;

    /// Non-offsettable flag for a plane (e.g., carbon, biodiversity).
    fn is_non_offsettable(&self, plane: PlaneId) -> bool;
}

/// Safestep decision result.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SafestepDecision {
    /// Step accepted: no hard-band breach, residual non-increasing under policy.
    Accept,
    /// Step rejected due to hard-band breach for some coordinate.
    HardBandBreach,
    /// Step rejected due to residual increase beyond tolerance.
    ResidualIncrease,
}
