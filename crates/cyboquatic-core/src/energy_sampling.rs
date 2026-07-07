// filename: crates/cyboquatic-core/src/energy_sampling.rs
// destination: github.com/mk-bluebird/Prometheus-Praxis

#![forbid(unsafe_code)]

use crate::Scalar;

#[derive(Debug, Clone)]
pub struct EnergyAwareSamplingParams {
    pub energy_cost_per_sample_j: Scalar,
    pub noise_variance: Scalar,
    pub missed_event_target: Scalar,
    pub max_sampling_hz: Scalar,
    pub min_sampling_hz: Scalar,
}

#[derive(Debug, Clone)]
pub struct EnergyAwareSamplingAdvice {
    pub recommended_hz: Scalar,
    pub expected_energy_per_hour_j: Scalar,
    pub comment: String,
}

pub struct EnergyAwareSamplingFrame;

impl EnergyAwareSamplingFrame {
    pub fn advise(params: &EnergyAwareSamplingParams) -> EnergyAwareSamplingAdvice {
        let mut f = params.max_sampling_hz;

        if params.noise_variance > 0.0 {
            let k_noise = 0.5;
            let f_noise = (k_noise / params.noise_variance).sqrt();
            if f_noise < f {
                f = f_noise;
            }
        }

        if params.missed_event_target > 0.0 && params.missed_event_target < 1.0 {
            let k_event = 1.0;
            let f_event = -k_event * (1.0 - params.missed_event_target).ln();
            if f_event > 0.0 && f_event > f {
                f = f_event;
            }
        }

        if f < params.min_sampling_hz {
            f = params.min_sampling_hz;
        }
        if f > params.max_sampling_hz {
            f = params.max_sampling_hz;
        }

        let samples_per_hour = f * 3600.0;
        let expected_energy_per_hour_j = samples_per_hour * params.energy_cost_per_sample_j;

        let comment = format!(
            "Recommended sampling rate {:.3} Hz based on energy cost, noise variance, and missed-event target.",
            f
        );

        EnergyAwareSamplingAdvice {
            recommended_hz: f,
            expected_energy_per_hour_j,
            comment,
        }
    }
}
