//! Integrity checks for ecosafety input windows.
//!
//! This frame performs cheap, non‑actuating sanity checks on
//! `EcosafetyInputWindow` contents before heavier diagnostics run.
//!
//! It rejects windows with:
//! - no samples,
//! - NaN/∞ coordinates,
//! - obviously impossible values (e.g., risk < 0 or > 10 as a
//!   conservative bound), or
//! - min_samples not met (delegated from config).
//!
//! The frame returns either the original window (if acceptable) or
//! `None` to signal that downstream frames should be skipped.

use crate::config::EcosafetyConfig;
use crate::ecosafety_covariance_frame::EcosafetyInputWindow;
use crate::frame::Frame;
use crate::types::NodeRiskSample;

/// Output of the integrity check stage.
///
/// `Some(window)` means “safe to proceed”; `None` means “skip”.
pub type IntegrityOutput = Option<EcosafetyInputWindow>;

/// Non‑actuating integrity check frame.
#[derive(Clone, Debug)]
pub struct IntegrityCheckFrame {
    cfg: EcosafetyConfig,
    /// Conservative bound for individual risk coordinates.
    max_abs_risk: f32,
}

impl IntegrityCheckFrame {
    /// Constructs a new integrity frame.
    pub fn new(cfg: EcosafetyConfig) -> Self {
        Self {
            cfg,
            max_abs_risk: 10.0,
        }
    }

    fn sample_ok(&self, sample: &NodeRiskSample) -> bool {
        let coords = [
            sample.r_pfas(),
            sample.r_cec(),
            sample.r_trap_fish(),
            sample.r_trap_amphib(),
            sample.r_sat(),
            sample.r_surcharge(),
            sample.r_biodiv(),
        ];

        for r in coords {
            if !r.is_finite() {
                return false;
            }
            if r < 0.0 || r > self.max_abs_risk {
                return false;
            }
        }

        let vt = sample.vt();
        vt.is_finite()
    }
}

impl Frame<EcosafetyInputWindow, IntegrityOutput> for IntegrityCheckFrame {
    fn evaluate(&self, window: &EcosafetyInputWindow) -> IntegrityOutput {
        let samples = window.samples();
        if samples.len() < self.cfg.min_samples {
            return None;
        }

        if samples.iter().any(|s| !self.sample_ok(s)) {
            return None;
        }

        Some(window.clone())
    }
}
