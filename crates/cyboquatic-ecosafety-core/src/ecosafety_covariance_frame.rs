//! Ecosafety covariance frame.
//!
//! This frame computes per‑node ecosafety envelopes over a window of
//! `NodeRiskSample`s. It estimates means and diagonal covariance for
//! the core risk coordinates, derives an ecosafety distance, and
//! classifies the window as GREEN/WARN/RED/UNDEFINED.
//!
//! The output is a single `CyboNodeEcosafetyEnvelope` record suitable
//! for storage in the Phoenix ecosafety envelope shard
//! (`CyboNodeEcosafetyEnvelopePhoenix2026v1`), but this module remains
//! strictly non‑actuating.

use crate::config::EcosafetyConfig;
use crate::frame::Frame;
use crate::types::{CyboNodeEcosafetyEnvelope, NodeRiskSample};
use chrono::{DateTime, Utc};

/// Input window for ecosafety evaluation.
///
/// Callers are expected to group samples by node and time window
/// before calling the frame.
#[derive(Clone, Debug)]
pub struct EcosafetyInputWindow {
    nodeid: String,
    region: String,
    medium: String,
    window_start_utc: DateTime<Utc>,
    window_end_utc: DateTime<Utc>,
    samples: Vec<NodeRiskSample>,
    vt_at_window_end: f32,
    lane: String,
    kerdeployable_hint: bool,
    evidencehex: String,
    signingdid: String,
}

impl EcosafetyInputWindow {
    /// Constructs a new ecosafety input window.
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        nodeid: String,
        region: String,
        medium: String,
        window_start_utc: DateTime<Utc>,
        window_end_utc: DateTime<Utc>,
        samples: Vec<NodeRiskSample>,
        vt_at_window_end: f32,
        lane: String,
        kerdeployable_hint: bool,
        evidencehex: String,
        signingdid: String,
    ) -> Self {
        Self {
            nodeid,
            region,
            medium,
            window_start_utc,
            window_end_utc,
            samples,
            vt_at_window_end,
            lane,
            kerdeployable_hint,
            evidencehex,
            signingdid,
        }
    }

    /// Access to the underlying samples.
    pub fn samples(&self) -> &[NodeRiskSample] {
        &self.samples
    }
}

/// Non‑actuating frame computing ecosafety distance and covariance
/// quality for a single node/window.
#[derive(Clone, Debug)]
pub struct EcosafetyCovarianceFrame {
    cfg: EcosafetyConfig,
}

impl EcosafetyCovarianceFrame {
    /// Creates a new ecosafety covariance frame with the given config.
    pub fn new(cfg: EcosafetyConfig) -> Self {
        Self { cfg }
    }

    fn compute_means(samples: &[NodeRiskSample]) -> (f32, f32, f32, f32, f32, f32, f32, f32) {
        let n = samples.len() as f32;
        if n <= 0.0 {
            return (0.0; 8);
        }

        let mut sum_pfas = 0.0f32;
        let mut sum_cec = 0.0f32;
        let mut sum_trap_fish = 0.0f32;
        let mut sum_trap_amphib = 0.0f32;
        let mut sum_sat = 0.0f32;
        let mut sum_surcharge = 0.0f32;
        let mut sum_biodiv = 0.0f32;
        let mut sum_vt = 0.0f32;

        for s in samples {
            sum_pfas += s.r_pfas();
            sum_cec += s.r_cec();
            sum_trap_fish += s.r_trap_fish();
            sum_trap_amphib += s.r_trap_amphib();
            sum_sat += s.r_sat();
            sum_surcharge += s.r_surcharge();
            sum_biodiv += s.r_biodiv();
            sum_vt += s.vt();
        }

        (
            sum_pfas / n,
            sum_cec / n,
            sum_trap_fish / n,
            sum_trap_amphib / n,
            sum_sat / n,
            sum_surcharge / n,
            sum_biodiv / n,
            sum_vt / n,
        )
    }

    fn compute_variances(
        samples: &[NodeRiskSample],
        means: (f32, f32, f32, f32, f32, f32, f32),
    ) -> ((f32, f32, f32, f32, f32, f32, f32), f32) {
        let n = samples.len() as f32;
        if n <= 1.0 {
            return (
                (0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0),
                f32::INFINITY,
            );
        }

        let (m_pfas, m_cec, m_trap_fish, m_trap_amphib, m_sat, m_surcharge, m_biodiv) = means;

        let mut s_pfas = 0.0f32;
        let mut s_cec = 0.0f32;
        let mut s_trap_fish = 0.0f32;
        let mut s_trap_amphib = 0.0f32;
        let mut s_sat = 0.0f32;
        let mut s_surcharge = 0.0f32;
        let mut s_biodiv = 0.0f32;

        for s in samples {
            s_pfas += (s.r_pfas() - m_pfas).powi(2);
            s_cec += (s.r_cec() - m_cec).powi(2);
            s_trap_fish += (s.r_trap_fish() - m_trap_fish).powi(2);
            s_trap_amphib += (s.r_trap_amphib() - m_trap_amphib).powi(2);
            s_sat += (s.r_sat() - m_sat).powi(2);
            s_surcharge += (s.r_surcharge() - m_surcharge).powi(2);
            s_biodiv += (s.r_biodiv() - m_biodiv).powi(2);
        }

        let denom = n - 1.0;
        let var_pfas = (s_pfas / denom).max(1.0e-6);
        let var_cec = (s_cec / denom).max(1.0e-6);
        let var_trap_fish = (s_trap_fish / denom).max(1.0e-6);
        let var_trap_amphib = (s_trap_amphib / denom).max(1.0e-6);
        let var_sat = (s_sat / denom).max(1.0e-6);
        let var_surcharge = (s_surcharge / denom).max(1.0e-6);
        let var_biodiv = (s_biodiv / denom).max(1.0e-6);

        let vars = [
            var_pfas,
            var_cec,
            var_trap_fish,
            var_trap_amphib,
            var_sat,
            var_surcharge,
            var_biodiv,
        ];

        let mut vmin = f32::INFINITY;
        let mut vmax = 0.0f32;
        for v in vars {
            if v > 0.0 {
                vmin = vmin.min(v);
                vmax = vmax.max(v);
            }
        }
        let cond = if vmin > 0.0 { vmax / vmin } else { f32::INFINITY };

        (
            (
                var_pfas,
                var_cec,
                var_trap_fish,
                var_trap_amphib,
                var_sat,
                var_surcharge,
                var_biodiv,
            ),
            cond,
        )
    }

    fn compute_distance_sq(
        latest: &NodeRiskSample,
        means: (f32, f32, f32, f32, f32, f32, f32),
        vars: (f32, f32, f32, f32, f32, f32, f32),
    ) -> f32 {
        let (m_pfas, m_cec, m_trap_fish, m_trap_amphib, m_sat, m_surcharge, m_biodiv) = means;
        let (v_pfas, v_cec, v_trap_fish, v_trap_amphib, v_sat, v_surcharge, v_biodiv) = vars;

        let dp_pfas = latest.r_pfas() - m_pfas;
        let dp_cec = latest.r_cec() - m_cec;
        let dp_trap_fish = latest.r_trap_fish() - m_trap_fish;
        let dp_trap_amphib = latest.r_trap_amphib() - m_trap_amphib;
        let dp_sat = latest.r_sat() - m_sat;
        let dp_surcharge = latest.r_surcharge() - m_surcharge;
        let dp_biodiv = latest.r_biodiv() - m_biodiv;

        let mut d2 = 0.0f32;
        d2 += (dp_pfas * dp_pfas) / v_pfas;
        d2 += (dp_cec * dp_cec) / v_cec;
        d2 += (dp_trap_fish * dp_trap_fish) / v_trap_fish;
        d2 += (dp_trap_amphib * dp_trap_amphib) / v_trap_amphib;
        d2 += (dp_sat * dp_sat) / v_sat;
        d2 += (dp_surcharge * dp_surcharge) / v_surcharge;
        d2 += (dp_biodiv * dp_biodiv) / v_biodiv;

        d2
    }

    fn classify_status(
        &self,
        d: f32,
        cond: f32,
        samples_used: usize,
    ) -> &'static str {
        if samples_used < self.cfg.min_samples {
            return "UNDEFINED";
        }
        if cond > self.cfg.max_cov_condition {
            return "UNDEFINED";
        }
        if d <= self.cfg.d_warn_default {
            "GREEN"
        } else if d <= self.cfg.d_max_default {
            "WARN"
        } else {
            "RED"
        }
    }
}

/// This implementation maps an input window to a single
/// `CyboNodeEcosafetyEnvelope`. Callers can then serialise this to ALN,
/// CSV, or SQL using external tooling.
impl Frame<EcosafetyInputWindow, Option<CyboNodeEcosafetyEnvelope>> for EcosafetyCovarianceFrame {
    fn evaluate(&self, window: &EcosafetyInputWindow) -> Option<CyboNodeEcosafetyEnvelope> {
        let samples = window.samples();
        if samples.is_empty() {
            return None;
        }

        let samples_used = samples.len();
        let (m_pfas, m_cec, m_trap_fish, m_trap_amphib, m_sat, m_surcharge, m_biodiv, m_vt) =
            Self::compute_means(samples);

        let (vars, cond) = Self::compute_variances(
            samples,
            (m_pfas, m_cec, m_trap_fish, m_trap_amphib, m_sat, m_surcharge, m_biodiv),
        );

        let latest = match samples.last() {
            Some(s) => s,
            None => return None,
        };

        let d2 = Self::compute_distance_sq(
            latest,
            (m_pfas, m_cec, m_trap_fish, m_trap_amphib, m_sat, m_surcharge, m_biodiv),
            vars,
        );
        let d = d2.sqrt();
        let status = self.classify_status(d, cond, samples_used);

        let envelope = CyboNodeEcosafetyEnvelope::new(
            window.nodeid.clone(),
            window.region.clone(),
            window.medium.clone(),
            window.window_start_utc,
            window.window_end_utc,
            m_pfas,
            m_cec,
            m_trap_fish,
            m_trap_amphib,
            m_sat,
            m_surcharge,
            m_biodiv,
            samples_used as u32,
            cond,
            false, // cov_regularized: diagonal path only in this skeleton
            d,
            d2,
            self.cfg.d_warn_default,
            self.cfg.d_max_default,
            status.to_string(),
            window.vt_at_window_end,
            window.lane.clone(),
            window.kerdeployable_hint,
            window.evidencehex.clone(),
            self.cfg.kfactor,
            self.cfg.efactor,
            self.cfg.rfactor,
            window.signingdid.clone(),
        );

        Some(envelope)
    }
}
