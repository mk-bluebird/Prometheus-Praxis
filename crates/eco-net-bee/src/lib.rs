// filename: crates/eco-net-bee/src/lib.rs
// destination: mk-bluebird/Prometheus-Praxis/crates/eco-net-bee/src/lib.rs

//! eco-net-bee
//!
//! Bee corridor governance primitives for the EcoNet / EcoFort constellation.
//!
//! This crate is strictly non-actuating. It defines:
//! - `BeeEnvelope`: corridor envelopes for bee habitat cells.
//! - `BeeLedgerRecord`: readonly ledger entries for Sunflower / bee assets.
//! - `SunflowerPlacementRequest`: proposals to place Sunflowers.
//! - `PlacementJustification`: structured justifications for accept/reject.
//! - `BeeCorridorProvider`: trait abstraction for testable backends.
//! - `BeeCorridorProviderMock`: in-memory CI/testing implementation.
//! - `BeeCorridorProviderOnChain`: on-chain / spine-backed implementation.
//!
//! All types are `serde`-serializable to support REST / MCP integration.

use std::collections::HashMap;
use std::sync::Arc;

use serde::{Deserialize, Serialize};

/// Bee corridor envelope definition for a single cell or region.
/// This mirrors ecosafety corridor semantics for the bee plane.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BeeEnvelope {
    /// Corridor identifier (bound to corridordefinition / ALN particle).
    pub envelope_id: String,
    /// Cell identifier in the corridor grid.
    pub cell_id: String,
    /// Minimum uninterrupted habitat area required in this cell (m^2).
    /// This is A_min in the D_max = 1 / A_min formula.
    pub min_habitat_area_m2: f64,
    /// Maximum permitted Sunflower density (Sunflowers per m^2) for this cell.
    /// Typically D_max = 1 / A_min, with safety factors applied.
    pub max_sunflower_density: f64,
    /// Additional metadata: nectar buffer enrichment, corridor class, etc.
    pub metadata: HashMap<String, String>,
}

/// Ledger query describing how a Sunflower asset is bound into the bee corridor
/// governance surface (artifactregistry + shardinstance).
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct BeeLedgerRecord {
    /// Unique asset identifier (e.g., shardid or artifactid).
    pub asset_id: String,
    /// Region or corridor region identifier.
    pub region: String,
    /// Cell identifier for placement.
    pub cell_id: String,
    /// Lane (RESEARCH / EXPPROD / PROD) at registration time.
    pub lane: String,
    /// KER band (SAFE / GUARDED / BLOCKED / OTHER).
    pub ker_band: String,
    /// EcoBeeImpactScore for this asset over the registration window.
    pub eco_bee_impact_score: f64,
    /// Evidence hex string (RoH-compatible commitment).
    pub evidence_hex: String,
    /// Signing DID that owns this record.
    pub signing_did: String,
}

/// Validation request for placing a Sunflower at a specific corridor cell.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SunflowerPlacementRequest {
    /// Proposed asset identifier (local to caller).
    pub asset_id: String,
    /// Corridor cell into which the Sunflower is to be placed.
    pub cell_id: String,
    /// Physical footprint of the Sunflower (m^2).
    pub footprint_area_m2: f64,
    /// Effective reduction of uninterrupted habitat area in this cell (Δ in m^2).
    pub delta_habitat_loss_m2: f64,
    /// Optional bee impact model parameters (e.g., RF duty cycle).
    pub metadata: HashMap<String, String>,
}

/// Structured justification for accepting or rejecting a placement proposal.
/// Designed to be serialized as JSON and included in MCP / REST responses.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct PlacementJustification {
    /// Whether the placement is allowed under the current bee corridor envelope.
    pub allowed: bool,
    /// Human-readable summary of the decision.
    pub summary: String,
    /// Machine-readable reasons and invariants checked.
    pub reasons: Vec<String>,
    /// Evidence bundle links (e.g., to shardinstance, artifactregistry, RoH).
    pub evidence_links: Vec<String>,
    /// Ledger entries that will be written if placement is accepted.
    pub prospective_ledger_entries: Vec<BeeLedgerRecord>,
}

/// Core trait that abstracts bee corridor access and placement decisions.
/// This trait is strictly non-actuating: implementations must only read from
/// governance / evidence stores and compute readonly justifications.
pub trait BeeCorridorProvider: Send + Sync {
    /// Retrieve the bee corridor envelope for a given cell.
    ///
    /// Implementations must return a deterministic result for a given cell_id
    /// over the lifetime of a test run to keep integration tests repeatable.
    fn get_envelope(&self, cell_id: &str) -> Option<BeeEnvelope>;

    /// Query the ledger for existing Sunflower / corridor entries associated
    /// with a given asset_id or cell_id.
    ///
    /// The key is a logical identifier; implementations may interpret it as
    /// asset_id, cell_id, or a composite key depending on backend.
    fn query_ledger(&self, key: &str) -> Vec<BeeLedgerRecord>;

    /// Validate a proposed Sunflower placement against the bee corridor envelope,
    /// KER bands, and EcoBeeImpactScore invariants.
    ///
    /// This function must be pure and deterministic: given the same request and
    /// backing state, it must always return the same justification.
    fn validate_sunflower_placement(
        &self,
        request: &SunflowerPlacementRequest,
    ) -> PlacementJustification;
}

/// In-memory, deterministic BeeCorridorProvider implementation for CI and unit tests.
/// This mock stores envelopes and ledger records in HashMaps and never touches
/// any external resources.
#[derive(Debug, Clone)]
pub struct BeeCorridorProviderMock {
    envelopes: HashMap<String, BeeEnvelope>,
    ledger_by_key: HashMap<String, Vec<BeeLedgerRecord>>,
}

impl BeeCorridorProviderMock {
    /// Create a new mock provider from explicit envelope and ledger maps.
    pub fn new(
        envelopes: HashMap<String, BeeEnvelope>,
        ledger_by_key: HashMap<String, Vec<BeeLedgerRecord>>,
    ) -> Self {
        Self {
            envelopes,
            ledger_by_key,
        }
    }

    /// Create an empty mock provider.
    pub fn empty() -> Self {
        Self {
            envelopes: HashMap::new(),
            ledger_by_key: HashMap::new(),
        }
    }

    /// Add or replace a BeeEnvelope for testing.
    pub fn with_envelope(mut self, envelope: BeeEnvelope) -> Self {
        self.envelopes.insert(envelope.cell_id.clone(), envelope);
        self
    }

    /// Add a ledger record under a specific logical key for testing.
    pub fn with_ledger_record(mut self, key: String, record: BeeLedgerRecord) -> Self {
        self.ledger_by_key
            .entry(key)
            .or_insert_with(Vec::new)
            .push(record);
        self
    }
}

impl BeeCorridorProvider for BeeCorridorProviderMock {
    fn get_envelope(&self, cell_id: &str) -> Option<BeeEnvelope> {
        self.envelopes.get(cell_id).cloned()
    }

    fn query_ledger(&self, key: &str) -> Vec<BeeLedgerRecord> {
        self.ledger_by_key
            .get(key)
            .cloned()
            .unwrap_or_else(Vec::new)
    }

    fn validate_sunflower_placement(
        &self,
        request: &SunflowerPlacementRequest,
    ) -> PlacementJustification {
        let envelope_opt = self.get_envelope(&request.cell_id);
        if envelope_opt.is_none() {
            return PlacementJustification {
                allowed: false,
                summary: format!(
                    "No bee corridor envelope defined for cell {}",
                    request.cell_id
                ),
                reasons: vec![
                    "Missing BeeEnvelope prevents safe evaluation under ecosafety grammar"
                        .to_string(),
                ],
                evidence_links: Vec::new(),
                prospective_ledger_entries: Vec::new(),
            };
        }

        let envelope = envelope_opt.unwrap();
        let a_min = envelope.min_habitat_area_m2;
        let delta = request.delta_habitat_loss_m2;
        let remaining = a_min - delta;

        let mut reasons = Vec::new();
        if remaining < a_min {
            reasons.push(format!(
                "Remaining habitat {:.3} m^2 falls below minimum A_min {:.3} m^2",
                remaining, a_min
            ));
        }

        let d_max_formula = if a_min > 0.0 {
            1.0 / a_min
        } else {
            f64::INFINITY
        };

        let d_max = envelope
            .max_sunflower_density
            .min(d_max_formula);

        let density_contribution = if request.footprint_area_m2 > 0.0 {
            1.0 / request.footprint_area_m2
        } else {
            f64::INFINITY
        };

        if density_contribution > d_max {
            reasons.push(format!(
                "Proposed Sunflower density contribution {:.6} exceeds D_max {:.6}",
                density_contribution, d_max
            ));
        }

        let allowed = reasons.is_empty();

        let mut evidence_links = Vec::new();
        evidence_links.push(format!("envelope://{}", envelope.envelope_id));

        let prospective_ledger_entries = if allowed {
            let record = BeeLedgerRecord {
                asset_id: request.asset_id.clone(),
                region: envelope
                    .metadata
                    .get("region")
                    .cloned()
                    .unwrap_or_else(|| "unknown-region".to_string()),
                cell_id: request.cell_id.clone(),
                lane: "RESEARCH".to_string(),
                ker_band: "SAFE".to_string(),
                eco_bee_impact_score: 0.0,
                evidence_hex: String::new(),
                signing_did: envelope
                    .metadata
                    .get("signing_did")
                    .cloned()
                    .unwrap_or_else(|| "unknown-did".to_string()),
            };
            vec![record]
        } else {
            Vec::new()
        };

        PlacementJustification {
            allowed,
            summary: if allowed {
                format!(
                    "Placement allowed: habitat and density remain within BeeEnvelope for cell {}",
                    request.cell_id
                )
            } else {
                format!(
                    "Placement rejected: BeeEnvelope constraints violated for cell {}",
                    request.cell_id
                )
            },
            reasons,
            evidence_links,
            prospective_ledger_entries,
        }
    }
}

/// On-chain backend implementation that queries an EcoNet / EcoFort discovery spine.
/// This implementation is read-only and non-actuating; it uses a Rust client to
/// query SQLite or a REST proxy that exposes artifactregistry, shardinstance,
/// and corridordefinition for bee corridors.
///
/// The exact backend wiring (SQLite path, HTTP base URL) is injected as configuration.
#[derive(Clone)]
pub struct BeeCorridorProviderOnChain<C> {
    /// Backend client used to query envelopes and ledger records.
    ///
    /// This client must be non-actuating and restricted to readonly endpoints
    /// (e.g., EcoFort REST or direct SQLite via rusqlite).
    backend_client: Arc<C>,
}

impl<C> BeeCorridorProviderOnChain<C>
where
    C: BeeCorridorBackendClient + Send + Sync + 'static,
{
    /// Create a new on-chain bee corridor provider from a backend client.
    pub fn new(backend_client: C) -> Self {
        Self {
            backend_client: Arc::new(backend_client),
        }
    }
}

/// Backend client abstraction for on-chain corridor queries.
/// This keeps the core trait free of concrete networking / DB details.
pub trait BeeCorridorBackendClient {
    fn fetch_envelope(&self, cell_id: &str) -> Option<BeeEnvelope>;
    fn fetch_ledger_by_key(&self, key: &str) -> Vec<BeeLedgerRecord>;
    fn compute_eco_bee_impact_score(&self, _asset_id: &str) -> f64 {
        0.0
    }
}

impl<C> BeeCorridorProvider for BeeCorridorProviderOnChain<C>
where
    C: BeeCorridorBackendClient + Send + Sync + 'static,
{
    fn get_envelope(&self, cell_id: &str) -> Option<BeeEnvelope> {
        self.backend_client.fetch_envelope(cell_id)
    }

    fn query_ledger(&self, key: &str) -> Vec<BeeLedgerRecord> {
        self.backend_client.fetch_ledger_by_key(key)
    }

    fn validate_sunflower_placement(
        &self,
        request: &SunflowerPlacementRequest,
    ) -> PlacementJustification {
        let envelope_opt = self.get_envelope(&request.cell_id);
        if envelope_opt.is_none() {
            return PlacementJustification {
                allowed: false,
                summary: format!(
                    "No bee corridor envelope defined for cell {} (on-chain backend)",
                    request.cell_id
                ),
                reasons: vec![
                    "Missing BeeEnvelope prevents safe evaluation under ecosafety grammar"
                        .to_string(),
                ],
                evidence_links: Vec::new(),
                prospective_ledger_entries: Vec::new(),
            };
        }

        let envelope = envelope_opt.unwrap();
        let a_min = envelope.min_habitat_area_m2;
        let delta = request.delta_habitat_loss_m2;
        let remaining = a_min - delta;

        let mut reasons = Vec::new();
        if remaining < a_min {
            reasons.push(format!(
                "Remaining habitat {:.3} m^2 falls below minimum A_min {:.3} m^2",
                remaining, a_min
            ));
        }

        let d_max_formula = if a_min > 0.0 {
            1.0 / a_min
        } else {
            f64::INFINITY
        };

        let d_max = envelope
            .max_sunflower_density
            .min(d_max_formula);

        let density_contribution = if request.footprint_area_m2 > 0.0 {
            1.0 / request.footprint_area_m2
        } else {
            f64::INFINITY
        };

        if density_contribution > d_max {
            reasons.push(format!(
                "Proposed Sunflower density contribution {:.6} exceeds D_max {:.6}",
                density_contribution, d_max
            ));
        }

        let mut evidence_links = Vec::new();
        evidence_links.push(format!("envelope://{}", envelope.envelope_id));

        let existing = self.query_ledger(&request.cell_id);
        if !existing.is_empty() {
            evidence_links.push(format!(
                "ledger://cell/{} ({} existing entries)",
                request.cell_id,
                existing.len()
            ));
        }

        let allowed = reasons.is_empty();

        let prospective_ledger_entries = if allowed {
            let eco_score = self
                .backend_client
                .compute_eco_bee_impact_score(&request.asset_id);
            let record = BeeLedgerRecord {
                asset_id: request.asset_id.clone(),
                region: envelope
                    .metadata
                    .get("region")
                    .cloned()
                    .unwrap_or_else(|| "unknown-region".to_string()),
                cell_id: request.cell_id.clone(),
                lane: "EXPPROD".to_string(),
                ker_band: "SAFE".to_string(),
                eco_bee_impact_score: eco_score,
                evidence_hex: String::new(),
                signing_did: envelope
                    .metadata
                    .get("signing_did")
                    .cloned()
                    .unwrap_or_else(|| "unknown-did".to_string()),
            };
            vec![record]
        } else {
            Vec::new()
        };

        PlacementJustification {
            allowed,
            summary: if allowed {
                format!(
                    "Placement allowed (on-chain): habitat and density remain within BeeEnvelope for cell {}",
                    request.cell_id
                )
            } else {
                format!(
                    "Placement rejected (on-chain): BeeEnvelope constraints violated for cell {}",
                    request.cell_id
                )
            },
            reasons,
            evidence_links,
            prospective_ledger_entries,
        }
    }
}
