// Filename: src/jlf/portenta_health_jlf.rs
// Destination: Prometheus-Praxis/ecosafety-core/src/jlf/portenta_health_jlf.rs
// License: MIT OR Apache-2.0

#![allow(clippy::float_cmp)]

use core::fmt;

// Local JLF scalar for Portenta H7 device health (battery, CPU temperature).
// This is non-actuating: it computes risk and a Lyapunov-style residual V(t)
// that other crates can use to throttle inference rate.

#[derive(Clone, Copy, Debug)]
pub struct PortentaHealthSample {
    pub soc_frac: f64,      // battery state-of-charge in [0,1]
    pub cpu_temp_c: f64,    // CPU temperature in degrees C
    pub vdd_mv: f64,        // supply voltage in millivolts
}

#[derive(Clone, Copy, Debug)]
pub struct PortentaHealthBands {
    // Battery SOC bands (gold, soft, hard)
    pub soc_gold_min: f64,
    pub soc_hard_min: f64,
    // CPU temperature bands (gold, soft, hard)
    pub temp_gold_max: f64,
    pub temp_soft_max: f64,
    pub temp_hard_max: f64,
    // Supply voltage bands (gold, hard)
    pub vdd_gold_min_mv: f64,
    pub vdd_hard_min_mv: f64,
    // Lyapunov weights
    pub w_soc: f64,
    pub w_temp: f64,
    pub w_vdd: f64,
}

impl Default for PortentaHealthBands {
    fn default() -> Self {
        // These bands must be calibrated from real Portenta H7 telemetry.
        // Values here are conservative Phoenix-style placeholders:
        // - SoC: gold >= 0.4, hard >= 0.2
        // - CPU temp: gold <= 65 C, soft <= 75 C, hard <= 85 C
        // - Vdd: gold >= 3200 mV, hard >= 3000 mV
        Self {
            soc_gold_min: 0.40,
            soc_hard_min: 0.20,
            temp_gold_max: 65.0,
            temp_soft_max: 75.0,
            temp_hard_max: 85.0,
            vdd_gold_min_mv: 3200.0,
            vdd_hard_min_mv: 3000.0,
            w_soc: 0.3,
            w_temp: 0.5,
            w_vdd: 0.2,
        }
    }
}

#[derive(Clone, Copy, Debug)]
pub struct PortentaHealthRisk {
    pub r_soc: f64,      // normalized battery risk in [0,1]
    pub r_temp: f64,     // normalized thermal risk in [0,1]
    pub r_vdd: f64,      // normalized supply risk in [0,1]
    pub jlf: f64,        // JLF scalar = V(t) = Σ w_i r_i^2
    pub corridorsafe: bool,
}

impl fmt::Display for PortentaHealthRisk {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "PortentaHealthRisk(r_soc={:.3}, r_temp={:.3}, r_vdd={:.3}, jlf={:.3}, safe={})",
            self.r_soc, self.r_temp, self.r_vdd, self.jlf, self.corridorsafe
        )
    }
}

pub fn compute_portenta_health_jlf(
    sample: PortentaHealthSample,
    bands: PortentaHealthBands,
) -> PortentaHealthRisk {
    // Battery risk: 0 when SoC >= gold_min, 1 when SoC <= hard_min, linear in between.
    let r_soc = if sample.soc_frac >= bands.soc_gold_min {
        0.0
    } else if sample.soc_frac <= bands.soc_hard_min {
        1.0
    } else {
        let num = bands.soc_gold_min - sample.soc_frac;
        let den = bands.soc_gold_min - bands.soc_hard_min;
        (num / den).clamp(0.0, 1.0)
    };

    // Thermal risk: 0 when temp <= gold_max, 1 when temp >= hard_max,
    // piecewise linear with a soft corridor between gold and soft, and
    // steeper growth from soft to hard.
    let r_temp = if sample.cpu_temp_c <= bands.temp_gold_max {
        0.0
    } else if sample.cpu_temp_c >= bands.temp_hard_max {
        1.0
    } else if sample.cpu_temp_c <= bands.temp_soft_max {
        let num = sample.cpu_temp_c - bands.temp_gold_max;
        let den = bands.temp_soft_max - bands.temp_gold_max;
        (num / den).clamp(0.0, 1.0) * 0.6
    } else {
        let num = sample.cpu_temp_c - bands.temp_soft_max;
        let den = bands.temp_hard_max - bands.temp_soft_max;
        (num / den).clamp(0.0, 1.0) * 0.4 + 0.6
    };

    // Supply risk: 0 when Vdd >= gold_min, 1 when Vdd <= hard_min, linear between.
    let r_vdd = if sample.vdd_mv >= bands.vdd_gold_min_mv {
        0.0
    } else if sample.vdd_mv <= bands.vdd_hard_min_mv {
        1.0
    } else {
        let num = bands.vdd_gold_min_mv - sample.vdd_mv;
        let den = bands.vdd_gold_min_mv - bands.vdd_hard_min_mv;
        (num / den).clamp(0.0, 1.0)
    };

    let jlf = bands.w_soc * r_soc * r_soc
        + bands.w_temp * r_temp * r_temp
        + bands.w_vdd * r_vdd * r_vdd;

    // Gold corridor: all risks zero.
    let corridorsafe = r_soc == 0.0 && r_temp == 0.0 && r_vdd == 0.0;

    PortentaHealthRisk {
        r_soc,
        r_temp,
        r_vdd,
        jlf,
        corridorsafe,
    }
}

// Simple inference throttle policy based on JLF and thermal corridor.
//
// Returns a multiplier in (0,1] that callers can apply to their nominal
// inference rate (e.g., RNN / CNN cycle frequency). When JLF or r_temp
// are high, the multiplier is reduced, decreasing workload and heat.

#[derive(Clone, Copy, Debug)]
pub struct InferenceThrottlePolicy {
    pub jlf_soft: f64,   // JLF at which throttling begins
    pub jlf_hard: f64,   // JLF at which minimum rate is enforced
    pub min_rate_frac: f64,
}

impl Default for InferenceThrottlePolicy {
    fn default() -> Self {
        // Soft start at JLF ~ 0.2, hard at ~0.6, never below 25% nominal rate.
        Self {
            jlf_soft: 0.2,
            jlf_hard: 0.6,
            min_rate_frac: 0.25,
        }
    }
}

pub fn compute_inference_rate_multiplier(
    risk: PortentaHealthRisk,
    policy: InferenceThrottlePolicy,
) -> f64 {
    let mut m = 1.0;

    // Thermal backoff: high r_temp forces additional throttling.
    if risk.r_temp > 0.0 {
        // r_temp in [0,1], scale a reduction up to 50%.
        let temp_factor = 1.0 - 0.5 * risk.r_temp;
        m *= temp_factor.clamp(policy.min_rate_frac, 1.0);
    }

    // JLF backoff: rising composite risk tightens rate further.
    if risk.jlf <= policy.jlf_soft {
        m
    } else if risk.jlf >= policy.jlf_hard {
        policy.min_rate_frac.max(0.0).min(m)
    } else {
        let num = risk.jlf - policy.jlf_soft;
        let den = policy.jlf_hard - policy.jlf_soft;
        let frac = (num / den).clamp(0.0, 1.0);
        let target = 1.0 - frac * (1.0 - policy.min_rate_frac);
        let combined = m.min(target);
        combined.clamp(policy.min_rate_frac, 1.0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_gold_corridor_safe() {
        let sample = PortentaHealthSample {
            soc_frac: 0.8,
            cpu_temp_c: 45.0,
            vdd_mv: 3300.0,
        };
        let bands = PortentaHealthBands::default();
        let risk = compute_portenta_health_jlf(sample, bands);
        assert_eq!(risk.r_soc, 0.0);
        assert_eq!(risk.r_temp, 0.0);
        assert_eq!(risk.r_vdd, 0.0);
        assert!(risk.corridorsafe);
        let policy = InferenceThrottlePolicy::default();
        let m = compute_inference_rate_multiplier(risk, policy);
        assert!((m - 1.0).abs() < 1e-9);
    }

    #[test]
    fn test_high_temp_throttles_rate() {
        let sample = PortentaHealthSample {
            soc_frac: 0.5,
            cpu_temp_c: 80.0,
            vdd_mv: 3100.0,
        };
        let bands = PortentaHealthBands::default();
        let risk = compute_portenta_health_jlf(sample, bands);
        assert!(risk.r_temp > 0.6);
        let policy = InferenceThrottlePolicy::default();
        let m = compute_inference_rate_multiplier(risk, policy);
        assert!(m < 1.0);
        assert!(m >= policy.min_rate_frac);
    }
}
