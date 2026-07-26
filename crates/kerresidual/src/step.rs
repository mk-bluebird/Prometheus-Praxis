// filename: crates/ker-residual/src/step.rs
#![forbid(unsafe_code)]

use serde::{Deserialize, Serialize};

use crate::{KerWindow, LyapunovWeights, Residual, RiskVector, SafeStepDecision, SafeStepGate};

pub trait LyapunovStep {
    type State;

    fn compute_next(
        &self,
        state_t: &Self::State,
    ) -> (RiskVector, Residual);
}

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct PhoenixReplayResult {
    pub shardid: String,
    pub lane: String,
    pub vt_before: f64,
    pub vt_after: f64,
    pub s_t: f64,
    pub decision: SafeStepDecision,
    pub kerwindow: Option<KerWindow>,
}

pub fn run_replay_profile<S, Stepper>(
    shardid: &str,
    lane: &str,
    weights: LyapunovWeights,
    stepper: &Stepper,
    states: &[S::State],
    s_t_series: &[f64],
) -> Vec<PhoenixReplayResult>
where
    Stepper: LyapunovStep<State = S::State>,
{
    let mut results = Vec::new();
    if states.len() < 2 || states.len() != s_t_series.len() {
        return results;
    }

    let (rv0, res0) = stepper.compute_next(&states[0]);
    let mut vt_series = vec![res0.vt];
    let mut maxrisks = vec![rv0.renergy.value.max(rv0.rhydraulics.value)];

    let mut vt_current = res0;
    for i in 1..states.len() {
        let (rv_next, res_next) = stepper.compute_next(&states[i]);
        let gate = SafeStepGate::new(weights, vt_current);
        let (res_eval, decision) = gate.evaluate_next(rv_next);

        vt_series.push(res_eval.vt);
        maxrisks.push(rv_next.renergy.value.max(rv_next.rhydraulics.value));

        let s_t = s_t_series[i];
        results.push(PhoenixReplayResult {
            shardid: shardid.to_owned(),
            lane: lane.to_owned(),
            vt_before: vt_current.vt,
            vt_after: res_eval.vt,
            s_t,
            decision,
            kerwindow: KerWindow::from_residual_series(&vt_series, &maxrisks),
        });

        vt_current = res_eval;
    }

    results
}
