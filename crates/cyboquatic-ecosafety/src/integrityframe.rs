//! Integrity-check frame for ecosafety diagnostics.
//!
//! This frame performs fast, non-actuating input validation and
//! normalisation on `EcosafetyInputWindow` before deeper numerical
//! work. It is designed to fail-closed: if anything looks malformed
//! or adversarial, the frame returns an `IntegrityOutput` with
//! `accepted = false` and leaves downstream frames free to skip
//! computation or mark the shard as non-deployable.

#![forbid(unsafe_code)]

use crate::frame::Frame;
use crate::ecosafetycovarianceframe::EcosafetyInputWindow;

/// Outcome of the integrity check for a single window.
#[derive(Clone, Debug)]
pub struct IntegrityOutput {
    /// The (possibly normalised) input window.
    window: EcosafetyInputWindow,
    /// Whether the window passed integrity checks.
    accepted: bool,
    /// Short reason code for rejection, if any.
    rejection_reason: Option<String>,
}

impl IntegrityOutput {
    /// Construct a new integrity output.
    pub fn new(window: EcosafetyInputWindow, accepted: bool, rejection_reason: Option<String>) -> Self {
        Self {
            window,
            accepted,
            rejection_reason,
        }
    }

    /// Borrow the (possibly normalised) input window.
    pub fn window(&self) -> &EcosafetyInputWindow {
        &self.window
    }

    /// Consume and return the window.
    pub fn into_window(self) -> EcosafetyInputWindow {
        self.window
    }

    /// Whether this window passed integrity checks.
    pub fn accepted(&self) -> bool {
        self.accepted
    }

    /// Optional rejection reason.
    pub fn rejection_reason(&self) -> Option<&str> {
        self.rejection_reason.as_deref()
    }
}

/// Integrity-check frame.
///
/// This frame enforces simple, cheap conditions:
/// - Node, region, medium must be non-empty.
/// - Window start must be strictly before window end.
/// - Sample count must be within configured bounds.
/// - Risk coordinates must be finite and within [0, 1] for all samples.
///
/// It never touches SQL, files, or OS APIs, and is safe to use in
/// WASM or embedded contexts.
#[derive(Clone, Debug)]
pub struct IntegrityCheckFrame {
    /// Minimum number of samples required to accept a window.
    min_samples: usize,
    /// Maximum number of samples allowed; larger windows are rejected.
    max_samples: usize,
}

impl IntegrityCheckFrame {
    /// Construct a new `IntegrityCheckFrame`.
    pub fn new(min_samples: usize, max_samples: usize) -> Self {
        Self {
            min_samples,
            max_samples,
        }
    }

    fn reject(window: EcosafetyInputWindow, reason: &str) -> Option<IntegrityOutput> {
        Some(IntegrityOutput::new(
            window,
            false,
            Some(reason.to_string()),
        ))
    }

    fn accept(window: EcosafetyInputWindow) -> Option<IntegrityOutput> {
        Some(IntegrityOutput::new(window, true, None))
    }
}

impl Frame<EcosafetyInputWindow, IntegrityOutput> for IntegrityCheckFrame {
    fn evaluate(&self, mut window: EcosafetyInputWindow) -> Option<IntegrityOutput> {
        // Identity fields must be present.
        if window.nodeid.trim().is_empty() {
            return Self::reject(window, "missing_nodeid");
        }
        if window.region.trim().is_empty() {
            return Self::reject(window, "missing_region");
        }
        if window.medium.trim().is_empty() {
            return Self::reject(window, "missing_medium");
        }

        // Window time ordering. The underlying type is a chrono DateTime in the
        // crate; here we only rely on its Ord implementation.
        if window.window_start_utc >= window.window_end_utc {
            return Self::reject(window, "invalid_window_order");
        }

        let sample_len = window.samples.len();

        if sample_len < self.min_samples {
            return Self::reject(window, "too_few_samples");
        }

        if sample_len > self.max_samples {
            return Self::reject(window, "too_many_samples");
        }

        // Validate each sample's risk coordinates.
        for sample in &window.samples {
            if !sample.rpfas.is_finite()
                || !sample.rcec.is_finite()
                || !sample.rtrap_fish.is_finite()
                || !sample.rtrap_amphib.is_finite()
                || !sample.rsat.is_finite()
                || !sample.rsurcharge.is_finite()
                || !sample.rbiodiv.is_finite()
            {
                return Self::reject(window, "non_finite_risk_coordinate");
            }

            if !is_unit_interval(sample.rpfas)
                || !is_unit_interval(sample.rcec)
                || !is_unit_interval(sample.rtrap_fish)
                || !is_unit_interval(sample.rtrap_amphib)
                || !is_unit_interval(sample.rsat)
                || !is_unit_interval(sample.rsurcharge)
                || !is_unit_interval(sample.rbiodiv)
            {
                return Self::reject(window, "risk_coordinate_out_of_bounds");
            }
        }

        // Normalisation hook: if you want to clamp tiny negative values
        // due to numerical noise, you can uncomment and adjust this.
        //
        // for sample in &mut window.samples {
        //     clamp01(&mut sample.rpfas);
        //     clamp01(&mut sample.rcec);
        //     clamp01(&mut sample.rtrap_fish);
        //     clamp01(&mut sample.rtrap_amphib);
        //     clamp01(&mut sample.rsat);
        //     clamp01(&mut sample.rsurcharge);
        //     clamp01(&mut sample.rbiodiv);
        // }

        Self::accept(window)
    }
}

fn is_unit_interval(x: f32) -> bool {
    x.is_finite() && x >= 0.0 && x <= 1.0
}

// Optional helper if you later enable normalisation.
// fn clamp01(x: &mut f32) {
//     if !x.is_finite() {
//         *x = 0.0;
//     } else if *x < 0.0 {
//         *x = 0.0;
//     } else if *x > 1.0 {
//         *x = 1.0;
//     }
// }
