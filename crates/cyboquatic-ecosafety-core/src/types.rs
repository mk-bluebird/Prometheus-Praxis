//! Schema‑bound types for Cyboquatic ecosafety diagnostics.
//!
//! These structs are designed to map 1:1 onto the ALN particle
//! `CyboquaticEcosafetyEnvelopePhoenix2026v1` and the corresponding
//! SQL migration `2026_07_07_01_cyboquatic_ecosafety_envelope_phoenix.sql`.
//!
//! They carry risk coordinates, ecosafety metrics, KER factors, and
//! provenance fields used by EcoNet and eco_restoration_shard.
//!
//! Fields are kept private with accessor methods to allow schema
//! extensions without breaking downstream users.

use chrono::{DateTime, Utc};

/// Per‑sample risk coordinates for a Cyboquatic node.
///
/// This struct is used as input to ecosafety frames
/// (e.g. covariance‑based envelopes). It does not include
/// governance fields; those live in envelope records.
#[derive(Clone, Debug)]
pub struct NodeRiskSample {
    r_pfas: f32,
    r_cec: f32,
    r_trap_fish: f32,
    r_trap_amphib: f32,
    r_sat: f32,
    r_surcharge: f32,
    r_biodiv: f32,
    vt: f32,
}

impl NodeRiskSample {
    /// Constructs a new risk sample from individual coordinates.
    pub fn new(
        r_pfas: f32,
        r_cec: f32,
        r_trap_fish: f32,
        r_trap_amphib: f32,
        r_sat: f32,
        r_surcharge: f32,
        r_biodiv: f32,
        vt: f32,
    ) -> Self {
        Self {
            r_pfas,
            r_cec,
            r_trap_fish,
            r_trap_amphib,
            r_sat,
            r_surcharge,
            r_biodiv,
            vt,
        }
    }

    /// PFAS risk coordinate (normalised, dimensionless).
    pub fn r_pfas(&self) -> f32 {
        self.r_pfas
    }

    /// CEC risk coordinate (normalised, dimensionless).
    pub fn r_cec(&self) -> f32 {
        self.r_cec
    }

    /// Fish trap‑guild risk coordinate.
    pub fn r_trap_fish(&self) -> f32 {
        self.r_trap_fish
    }

    /// Amphibian trap‑guild risk coordinate.
    pub fn r_trap_amphib(&self) -> f32 {
        self.r_trap_amphib
    }

    /// SAT (soil‑aqua treatment) performance risk coordinate.
    pub fn r_sat(&self) -> f32 {
        self.r_sat
    }

    /// Hydraulic surcharge risk coordinate.
    pub fn r_surcharge(&self) -> f32 {
        self.r_surcharge
    }

    /// Biodiversity risk coordinate.
    pub fn r_biodiv(&self) -> f32 {
        self.r_biodiv
    }

    /// Lyapunov residual Vt associated with this sample.
    pub fn vt(&self) -> f32 {
        self.vt
    }
}

/// Per‑node ecosafety envelope summary over a time window.
///
/// This mirrors the fields selected in `EcosafetyNodeStatus` and the
/// ALN particle `CyboNodeEcosafetyEnvelopePhoenix2026v1`.
#[derive(Clone, Debug)]
pub struct CyboNodeEcosafetyEnvelope {
    // Identity and window
    nodeid: String,
    region: String,
    medium: String,
    window_start_utc: DateTime<Utc>,
    window_end_utc: DateTime<Utc>,

    // Risk means
    r_pfas_mean: f32,
    r_cec_mean: f32,
    r_trap_fish_mean: f32,
    r_trap_amphib_mean: f32,
    r_sat_mean: f32,
    r_surcharge_mean: f32,
    r_biodiv_mean: f32,

    // Covariance metrics
    samples_used: u32,
    cov_condition_number: f32,
    cov_regularized: bool,

    // Ecosafety distance metrics
    ecosafety_distance: f32,
    ecosafety_distance_sq: f32,

    // Thresholds and status
    d_warn_threshold: f32,
    d_max_threshold: f32,
    ecosafety_status: String,

    // Lyapunov and lanes
    vt_at_window_end: f32,
    lane: String,
    kerdeployable: bool,

    // Provenance and KER
    evidencehex: String,
    kfactor: f32,
    efactor: f32,
    rfactor: f32,
    signingdid: String,
}

impl CyboNodeEcosafetyEnvelope {
    /// Constructs a new ecosafety envelope record.
    ///
    /// Callers are expected to supply values already consistent with
    /// corridor invariants and KER constraints; guard crates enforce
    /// global monotonicity.
    #[allow(clippy::too_many_arguments)]
    pub fn new(
        nodeid: String,
        region: String,
        medium: String,
        window_start_utc: DateTime<Utc>,
        window_end_utc: DateTime<Utc>,
        r_pfas_mean: f32,
        r_cec_mean: f32,
        r_trap_fish_mean: f32,
        r_trap_amphib_mean: f32,
        r_sat_mean: f32,
        r_surcharge_mean: f32,
        r_biodiv_mean: f32,
        samples_used: u32,
        cov_condition_number: f32,
        cov_regularized: bool,
        ecosafety_distance: f32,
        ecosafety_distance_sq: f32,
        d_warn_threshold: f32,
        d_max_threshold: f32,
        ecosafety_status: String,
        vt_at_window_end: f32,
        lane: String,
        kerdeployable: bool,
        evidencehex: String,
        kfactor: f32,
        efactor: f32,
        rfactor: f32,
        signingdid: String,
    ) -> Self {
        Self {
            nodeid,
            region,
            medium,
            window_start_utc,
            window_end_utc,
            r_pfas_mean,
            r_cec_mean,
            r_trap_fish_mean,
            r_trap_amphib_mean,
            r_sat_mean,
            r_surcharge_mean,
            r_biodiv_mean,
            samples_used,
            cov_condition_number,
            cov_regularized,
            ecosafety_distance,
            ecosafety_distance_sq,
            d_warn_threshold,
            d_max_threshold,
            ecosafety_status,
            vt_at_window_end,
            lane,
            kerdeployable,
            evidencehex,
            kfactor,
            efactor,
            rfactor,
            signingdid,
        }
    }

    /// Node identifier.
    pub fn nodeid(&self) -> &str {
        &self.nodeid
    }

    /// Region code (e.g., "Phoenix-AZ").
    pub fn region(&self) -> &str {
        &self.region
    }

    /// Medium (e.g., "canal", "wetland").
    pub fn medium(&self) -> &str {
        &self.medium
    }

    /// Start of the evaluation window.
    pub fn window_start_utc(&self) -> DateTime<Utc> {
        self.window_start_utc
    }

    /// End of the evaluation window.
    pub fn window_end_utc(&self) -> DateTime<Utc> {
        self.window_end_utc
    }

    /// Mean PFAS risk over the window.
    pub fn r_pfas_mean(&self) -> f32 {
        self.r_pfas_mean
    }

    /// Mean CEC risk over the window.
    pub fn r_cec_mean(&self) -> f32 {
        self.r_cec_mean
    }

    /// Mean fish trap‑guild risk.
    pub fn r_trap_fish_mean(&self) -> f32 {
        self.r_trap_fish_mean
    }

    /// Mean amphibian trap‑guild risk.
    pub fn r_trap_amphib_mean(&self) -> f32 {
        self.r_trap_amphib_mean
    }

    /// Mean SAT performance risk.
    pub fn r_sat_mean(&self) -> f32 {
        self.r_sat_mean
    }

    /// Mean surcharge risk.
    pub fn r_surcharge_mean(&self) -> f32 {
        self.r_surcharge_mean
    }

    /// Mean biodiversity risk.
    pub fn r_biodiv_mean(&self) -> f32 {
        self.r_biodiv_mean
    }

    /// Number of samples used in this envelope.
    pub fn samples_used(&self) -> u32 {
        self.samples_used
    }

    /// Condition number of the covariance estimate.
    pub fn cov_condition_number(&self) -> f32 {
        self.cov_condition_number
    }

    /// Whether covariance regularisation was applied.
    pub fn cov_regularized(&self) -> bool {
        self.cov_regularized
    }

    /// Ecosafety distance (Mahalanobis‑like, unitless).
    pub fn ecosafety_distance(&self) -> f32 {
        self.ecosafety_distance
    }

    /// Squared ecosafety distance.
    pub fn ecosafety_distance_sq(&self) -> f32 {
        self.ecosafety_distance_sq
    }

    /// Advisory ecosafety threshold.
    pub fn d_warn_threshold(&self) -> f32 {
        self.d_warn_threshold
    }

    /// Hard ecosafety threshold.
    pub fn d_max_threshold(&self) -> f32 {
        self.d_max_threshold
    }

    /// Ecosafety status label (GREEN/WARN/RED/UNDEFINED).
    pub fn ecosafety_status(&self) -> &str {
        &self.ecosafety_status
    }

    /// Lyapunov residual at window end.
    pub fn vt_at_window_end(&self) -> f32 {
        self.vt_at_window_end
    }

    /// Current governance lane (EXP/PROD/QUAR).
    pub fn lane(&self) -> &str {
        &self.lane
    }

    /// Suggested kerdeployable flag for this envelope.
    pub fn kerdeployable(&self) -> bool {
        self.kerdeployable
    }

    /// Evidence hex string for this record.
    pub fn evidencehex(&self) -> &str {
        &self.evidencehex
    }

    /// Knowledge factor K associated with this envelope.
    pub fn kfactor(&self) -> f32 {
        self.kfactor
    }

    /// Eco‑impact factor E associated with this envelope.
    pub fn efactor(&self) -> f32 {
        self.efactor
    }

    /// Risk‑of‑harm factor R associated with this envelope.
    pub fn rfactor(&self) -> f32 {
        self.rfactor
    }

    /// DID of the signer responsible for this envelope.
    pub fn signingdid(&self) -> &str {
        &self.signingdid
    }
}
