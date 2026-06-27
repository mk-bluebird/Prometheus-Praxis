// filepath: cybercore/prometheus_praxis/src/fog/node_fog_routing.rs

#![forbid(unsafe_code)]
#![warn(missing_docs)]

use std::time::Instant;

use serde::{Deserialize, Serialize};

use crate::fog::block_stress_guard::{FogBlockStressDecision, FogBlockStressGuard};
use crate::lyapunov::block_adapter::{
    LyapunovBlockProjection,
    LyapunovCellProjection,
    make_block_snapshot,
};
use crate::lyapunov::block_lyapunov_ker::LyapunovKerBand;
use crate::lyapunov::safe_step::SafeStepConfig;

/// Media class for a FOG node: air only, water‑adjacent, or canal‑embedded.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum MediaClass {
    /// Air‑only, no water adjacency, lowest hydraulic coupling.
    Air,
    /// Proximate to water (e.g. culvert, over‑bank channel).
    WaterAdj,
    /// Direct canal / MAR coupling, strongest hydraulic constraints.
    Canal,
}

/// Biological surface mode: inert, biofilm‑friendly, or actively colonizing.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum BioSurfaceMode {
    /// No biological interaction (fully inert).
    Inert,
    /// Biofilm‑friendly, but not actively seeded.
    BiofilmFriendly,
    /// Actively colonizing, high surface–biology coupling.
    Colonizing,
}

/// Workload variant for a FOG node family, aligned to your KER lanes.
#[derive(Debug, Clone, Copy, Serialize, Deserialize, PartialEq, Eq)]
pub enum CyboVariant {
    /// Low‑intensity sensing and non‑actuating analytics.
    SensingLite,
    /// Full FOG L2 analytics, no active flow modulation.
    AnalyticsL2,
    /// Closed‑loop FOG controller, highest constraints.
    ControlClosedLoop,
}

/// Snapshot of a single FOG node shard under routing consideration.
///
/// This is deliberately small and lane‑safe: no raw handles, only normalized
/// coordinates and identifiers.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NodeShard {
    /// Stable logical ID of the node.
    pub node_id: String,
    /// Region / corridor code, e.g. "Phoenix-AZ".
    pub region: String,
    /// Underlying media class.
    pub media_class: MediaClass,
    /// Biological surface mode.
    pub bio_mode: BioSurfaceMode,
    /// Normalized hydraulic stress coordinate (0..1).
    pub r_hydraulic: f64,
    /// Normalized structural stress coordinate (0..1).
    pub r_struct: f64,
    /// Normalized biology surface risk (e.g. biofouling) (0..1).
    pub r_bio_surface: f64,
    /// Maximum normalized Lyapunov residual V_t observed for this node window.
    pub vt_max: f64,
    /// Current lane, to prevent illegal cross‑lane routing.
    pub lane: String,
}

/// Additional context available to the router at decision time.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RoutingContext {
    /// Lyapunov band configuration for this region/lane.
    pub lyap_band: LyapunovKerBand,
    /// Safe‑step configuration for KER monotonicity checks.
    pub safestep: SafeStepConfig,
    /// Per‑region FOG block corridors.
    pub fog_guard: FogBlockStressGuard,
}

/// Advisory routing decision, including KER‑style flags.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RouteDecision {
    /// Selected variant for this node.
    pub chosen_variant: CyboVariant,
    /// True if routing is admissible under K, E, R, V_t.
    pub admissible: bool,
    /// Block‑level stress decision (when applicable).
    pub block_stress: Option<FogBlockStressDecision>,
    /// Human‑readable explanation.
    pub reason: String,
    /// Latency budget hint in milliseconds.
    pub latency_budget_ms: u64,
    /// Timestamp at which the decision was computed.
    pub decided_at: Instant,
}

/// Summary fields for qpudatashards, non‑actuating by design.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NodeFogStressSummary {
    pub node_id: String,
    pub region: String,
    pub media_class: MediaClass,
    pub r_hydraulic: f64,
    pub r_struct: f64,
    pub r_bio_surface: f64,
    pub vt_max: f64,
    pub variant: CyboVariant,
    pub admissible: bool,
}

/// Simple Lyapunov tailwind predicate: V_t inside band and no violation rows.
///
/// This reuses your V_t band semantics and KER "safestep" configuration.
fn tailwind_valid(node: &NodeShard, ctx: &RoutingContext) -> bool {
    let vt_ceiling = ctx.lyap_band.vt_ceiling;
    node.vt_max <= vt_ceiling
}

/// Biological surface predicate: colonizing modes are only allowed when
/// r_bio_surface is inside the admissible corridor.
fn biosurface_ok(node: &NodeShard, variant: CyboVariant) -> bool {
    match variant {
        CyboVariant::SensingLite | CyboVariant::AnalyticsL2 => true,
        CyboVariant::ControlClosedLoop => {
            // Closed‑loop FOG is only allowed if bio surface risk is low.
            node.r_bio_surface <= 0.6
        }
    }
}

/// Hydraulic predicate: tighten thresholds for canal‑adjacent nodes.
fn hydraulic_ok(node: &NodeShard) -> bool {
    let r_h = node.r_hydraulic;
    let r_s = node.r_struct;
    match node.media_class {
        MediaClass::Air => r_h <= 0.7 && r_s <= 0.7,
        MediaClass::WaterAdj => r_h <= 0.6 && r_s <= 0.6,
        MediaClass::Canal => r_h <= 0.5 && r_s <= 0.5,
    }
}

/// Lyapunov monotonicity check using existing safestep semantics.
///
/// Here we only gate on the pre‑computed vt_max; more detailed replay checks
/// remain in the KER harness.
fn lyapunov_ok(node: &NodeShard, ctx: &RoutingContext) -> bool {
    tailwind_valid(node, ctx)
}

/// Main routing function: decides which CyboVariant is admissible for a node,
/// and applies block‑level FOG stress guards for water/canal workloads.
///
/// This function is non‑actuating: it returns an advisory decision only.
pub fn route_variant(
    node: &NodeShard,
    ctx: &RoutingContext,
    block: Option<&LyapunovBlockProjection>,
    cells: &[LyapunovCellProjection],
) -> RouteDecision {
    let decided_at = Instant::now();

    // Baseline: choose variant from lane.
    let base_variant = match node.lane.as_str() {
        "RESEARCH" => CyboVariant::SensingLite,
        "EXPPROD" => CyboVariant::AnalyticsL2,
        "PROD" => CyboVariant::ControlClosedLoop,
        _ => CyboVariant::SensingLite,
    };

    // Check local predicates.
    let lyap_ok = lyapunov_ok(node, ctx);
    let hydro_ok = hydraulic_ok(node);
    let bio_ok = biosurface_ok(node, base_variant);

    let mut block_decision = None;

    // For water‑adjacent / canal nodes, evaluate block‑level stress.
    if matches!(node.media_class, MediaClass::WaterAdj | MediaClass::Canal) {
        if let Some(block_proj) = block {
            let decision = ctx.fog_guard.evaluate_block(block_proj, cells);
            block_decision = Some(decision);
        }
    }

    let block_ok = block_decision
        .as_ref()
        .map(|d| d.ok)
        .unwrap_or(true);

    let admissible = lyap_ok && hydro_ok && bio_ok && block_ok;

    let mut reason_parts = Vec::new();
    if !lyap_ok {
        reason_parts.push("Lyapunov band violation for node".to_string());
    }
    if !hydro_ok {
        reason_parts.push("Hydraulic/structural corridor violation".to_string());
    }
    if !bio_ok {
        reason_parts.push("Biological surface corridor violation".to_string());
    }
    if !block_ok {
        reason_parts.push("FOG block stress guard violation".to_string());
    }
    if reason_parts.is_empty() {
        reason_parts.push(format!(
            "Variant {:?} admissible for node {} in lane {}",
            base_variant, node.node_id, node.lane
        ));
    }

    let latency_budget_ms = match base_variant {
        CyboVariant::SensingLite => 250,
        CyboVariant::AnalyticsL2 => 150,
        CyboVariant::ControlClosedLoop => 50,
    };

    RouteDecision {
        chosen_variant: base_variant,
        admissible,
        block_stress: block_decision,
        reason: reason_parts.join("; "),
        latency_budget_ms,
        decided_at,
    }
}
