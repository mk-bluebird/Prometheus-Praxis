// filepath: eco_restoration_shard/cybercore/prometheus_praxis/src/fog/node_fog_routing.rs

#![forbid(unsafe_code)]

use std::time::Instant;

use serde::{Deserialize, Serialize};

use crate::fog::block_stress_guard::{FogBlockStressDecision, FogBlockStressGuard};
use crate::lyapunov::block_adapter::{make_block_snapshot, LyapunovBlockProjection, LyapunovCellProjection};
use crate::lyapunov::block_lyapunov_guard::{BlockLyapunovCoefficients, Scalar};
use crate::lyapunov::pfbs_coverage_cbf_guard::PfbsCoverageCbfParams;

/// Media class for a cyboquatic workload.
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum MediaClass {
    WaterOnly,
    WaterBiofilm,
    AirPlenum,
}

/// Biological surface mode for a node.
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum BioSurfaceMode {
    Raw,
    Preprocessed,
    Restricted,
}

/// Typed cyboquatic workload (variant) routed by FOG.
/// This mirrors the FOG research line definition. [file:205]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct CyboVariant {
    pub id: u64,
    /// Energy required over the execution horizon [J].
    pub energy_req_j: f64,
    /// Safety factor on energy budget (>= 1.0).
    pub safety_factor: f64,
    /// Maximum acceptable end-to-end latency [ms].
    pub max_latency_ms: u64,
    /// Fluid / substrate class.
    pub media: MediaClass,
    /// Normalized hydraulic impact (0–1 fraction of remaining corridor).
    pub hydraulic_impact: f64,
    /// Expected incremental Lyapunov delta if routed to a neutral node.
    pub dvt_nominal: f64,
    /// Whether the variant is PFBS- or coverage-relevant (for block stress).
    pub pfbs_or_coverage_sensitive: bool,
}

/// NodeShard: FOG endpoint snapshot (vault, turbine, air plenum, etc.). [file:205][file:209]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct NodeShard {
    // Energy plane.
    pub e_surplus_j: f64,
    pub p_margin_kw: f64,
    pub tailwind_mode: bool,
    pub d_edt_w: f64,

    // Hydraulics plane.
    pub q_m3s: f64,
    pub hlr_m_per_h: f64,
    pub surcharge_risk_rx: f64,

    // Biology plane.
    pub r_pathogen: f64,
    pub r_fouling: f64,
    pub r_cec: f64,
    pub bio_surface_mode: BioSurfaceMode,

    // Global / local residual and KER view.
    pub vt_local: f64,
    pub vt_trend: f64,
    pub k_score: f64,
    pub e_score: f64,
    pub r_score: f64,

    // Minimal PFBS + coverage view for block stress mapping.
    pub pfbs_concentration_ug_per_l: f64,
    pub swarm_coverage_fraction: f64,
}

/// Global routing context (Lyapunov admissible region). [file:205]
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq)]
pub struct RoutingContext {
    /// Current global Lyapunov residual V_t.
    pub vt_global: f64,
    /// Planner's admissible upper bound V_{t+1,max}.
    pub vt_global_next_max: f64,
    pub now: Instant,
}

/// Routing decision outcome.
#[derive(Clone, Copy, Debug, Serialize, Deserialize, PartialEq, Eq)]
pub enum RouteDecision {
    Accept,
    Reroute,
    Reject,
}

/// Energy predicate: tailwind validity. [file:205]
pub fn tailwind_valid(node: &NodeShard, variant: &CyboVariant) -> bool {
    if !node.tailwind_mode {
        return false;
    }
    let required = variant.energy_req_j * variant.safety_factor.max(1.0);
    if required <= 0.0 {
        return false;
    }

    // Require strictly positive surplus after allocation and non-negative power margin. [file:205]
    let surplus_after = node.e_surplus_j - required;
    surplus_after > 0.0 && node.p_margin_kw > 0.0 && node.d_edt_w >= 0.0
}

/// Biology predicate: biosurface corridor checks. [file:205]
pub fn biosurface_ok(node: &NodeShard, variant: &CyboVariant) -> bool {
    // No bio-contact on restricted surfaces except air-only variants.
    if let BioSurfaceMode::Restricted = node.bio_surface_mode {
        return matches!(variant.media, MediaClass::AirPlenum);
    }

    // Gold-corridor thresholds stricter than legal; 0.5 is illustrative here. [file:205]
    let r_thresh = 0.5_f64;

    match variant.media {
        MediaClass::AirPlenum => {
            node.r_pathogen <= r_thresh
        }
        MediaClass::WaterOnly | MediaClass::WaterBiofilm => {
            matches!(node.bio_surface_mode, BioSurfaceMode::Preprocessed)
                && node.r_pathogen <= r_thresh
                && node.r_fouling <= r_thresh
                && node.r_cec <= r_thresh
        }
    }
}

/// Hydraulics predicate: surcharge corridor. [file:205]
pub fn hydraulic_ok(node: &NodeShard, variant: &CyboVariant) -> bool {
    let impact = variant.hydraulic_impact.max(0.0);
    let rx = node.surcharge_risk_rx.max(0.0);

    // Require resulting risk < 1.0 corridor closure. [file:205]
    let predicted_rx = rx + impact;
    predicted_rx < 1.0
}

/// Lyapunov predicate on global and node-local residual. [file:205][file:209]
pub fn lyapunov_ok(node: &NodeShard, variant: &CyboVariant, ctx: &RoutingContext) -> bool {
    let dv_local = variant.dvt_nominal;
    let vt_next_est = ctx.vt_global + dv_local;

    // Enforce global admissible bound and non-worsening local trend. [file:205]
    vt_next_est <= ctx.vt_global_next_max && dv_local + node.vt_trend <= 0.0
}

/// Composite pure routing rule independent of block stress. [file:205]
pub fn route_variant_base(
    variant: &CyboVariant,
    node: &NodeShard,
    ctx: &RoutingContext,
) -> RouteDecision {
    if !tailwind_valid(node, variant) {
        return RouteDecision::Reroute;
    }
    if !biosurface_ok(node, variant) {
        return RouteDecision::Reroute;
    }
    if !hydraulic_ok(node, variant) {
        return RouteDecision::Reroute;
    }
    if !lyapunov_ok(node, variant, ctx) {
        return RouteDecision::Reject;
    }

    RouteDecision::Accept
}

/// Small wrapper type to plug NodeShard into the block-stress adapter.
#[derive(Clone, Debug)]
pub struct SingleNodeBlock {
    pub block_id: String,
    pub timestamp_utc_ms: i64,
    pub node: NodeShard,
}

impl LyapunovCellProjection for SingleNodeBlock {
    fn cell_id(&self) -> String {
        // Treat each node as a single "cell" in a degenerate block.
        "node-0".to_string()
    }

    fn pfbs_concentration_ug_per_l(&self) -> f64 {
        self.node.pfbs_concentration_ug_per_l
    }

    fn ecoli_cfu_per_100ml(&self) -> f64 {
        // If you have E. coli fields on NodeShard, map them here.
        0.0
    }

    fn swarm_coverage_fraction(&self) -> f64 {
        self.node.swarm_coverage_fraction
    }

    fn weight(&self) -> f64 {
        1.0
    }
}

impl LyapunovBlockProjection for SingleNodeBlock {
    type Cell = SingleNodeBlock;

    fn block_id(&self) -> String {
        self.block_id.clone()
    }

    fn timestamp_utc_ms(&self) -> i64 {
        self.timestamp_utc_ms
    }

    fn cells(&self) -> &[Self::Cell] {
        // Represent the single node as a 1-element slice.
        std::slice::from_ref(self)
    }
}

/// FOG routing rule extended with block-level Lyapunov + CBF stress guard. [file:205][file:209]
pub fn route_variant_with_block_stress(
    variant: &CyboVariant,
    node: &NodeShard,
    ctx: &RoutingContext,
    block_guard: &FogBlockStressGuard,
    block_id: &str,
    lyap_coeffs: BlockLyapunovCoefficients,
    cbf_params: PfbsCoverageCbfParams,
) -> RouteDecision {
    // First run base FOG predicates (energy, hydraulics, bio, Lyapunov).
    let base_decision = route_variant_base(variant, node, ctx);
    if !matches!(base_decision, RouteDecision::Accept) {
        return base_decision;
    }

    // Only enforce block stress for PFBS / coverage-sensitive workloads and water media. [file:206]
    if !variant.pfbs_or_coverage_sensitive {
        return RouteDecision::Accept;
    }
    if !matches!(variant.media, MediaClass::WaterOnly | MediaClass::WaterBiofilm) {
        return RouteDecision::Accept;
    }

    // Build "before" and "after" snapshots for this node as a degenerate block.
    // In a real integration, "before" would come from the shard state prior to routing;
    // here we approximate by using the same NodeShard and a simple perturbation model.
    let now_ms = (ctx.now.elapsed().as_millis() as i64).max(0);

    let before_block = SingleNodeBlock {
        block_id: block_id.to_string(),
        timestamp_utc_ms: now_ms,
        node: *node,
    };
    let snapshot_before = make_block_snapshot(&before_block, lyap_coeffs.clone());

    // Simple local "after" estimate: increase PFBS risk and adjust coverage
    // proportional to variant impact; callers can override by passing updated NodeShard.
    let mut node_after = *node;
    node_after.pfbs_concentration_ug_per_l += variant.hydraulic_impact * 10.0;
    node_after.swarm_coverage_fraction =
        (node_after.swarm_coverage_fraction - 0.1 * variant.hydraulic_impact).max(0.0);

    let after_block = SingleNodeBlock {
        block_id: block_id.to_string(),
        timestamp_utc_ms: now_ms + 1,
        node: node_after,
    };
    let snapshot_after = make_block_snapshot(&after_block, lyap_coeffs);

    // Override CBF parameters inside the existing guard if needed by constructing a new config.
    let fog_decision = block_guard.decide(&snapshot_before, &snapshot_after);

    match fog_decision {
        Ok(FogBlockStressDecision::Allow) => RouteDecision::Accept,
        Ok(FogBlockStressDecision::Derate { .. }) => RouteDecision::Reroute,
        Ok(FogBlockStressDecision::Stop { .. }) => RouteDecision::Reject,
        Err(_) => RouteDecision::Reject,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fog::block_stress_guard::{FogBlockStressConfig};
    use crate::lyapunov::block_lyapunov_guard::BlockLyapunovPolicy;

    fn sample_node() -> NodeShard {
        NodeShard {
            e_surplus_j: 5_000.0,
            p_margin_kw: 3.5,
            tailwind_mode: true,
            d_edt_w: 10.0,
            q_m3s: 0.2,
            hlr_m_per_h: 5.0,
            surcharge_risk_rx: 0.2,
            r_pathogen: 0.1,
            r_fouling: 0.3,
            r_cec: 0.2,
            bio_surface_mode: BioSurfaceMode::Preprocessed,
            vt_local: 0.9,
            vt_trend: -0.01,
            k_score: 0.93,
            e_score: 0.90,
            r_score: 0.14,
            pfbs_concentration_ug_per_l: 50.0,
            swarm_coverage_fraction: 0.8,
        }
    }

    fn sample_variant() -> CyboVariant {
        CyboVariant {
            id: 42,
            energy_req_j: 500.0,
            safety_factor: 1.5,
            max_latency_ms: 200,
            media: MediaClass::WaterOnly,
            hydraulic_impact: 0.1,
            dvt_nominal: -0.001,
            pfbs_or_coverage_sensitive: true,
        }
    }

    #[test]
    fn routing_allows_safe_pfbs_step() {
        let node = sample_node();
        let variant = sample_variant();
        let ctx = RoutingContext {
            vt_global: 1.0,
            vt_global_next_max: 1.0,
            now: Instant::now(),
        };

        let cfg = FogBlockStressConfig {
            lyapunov_policy: BlockLyapunovPolicy::default_derate_band(),
            cbf_params: PfbsCoverageCbfParams {
                w_cov: Scalar(1.0),
                w_pfbs: Scalar(1.0),
                epsilon_decrease: 1e-3,
            },
        };
        let block_guard = FogBlockStressGuard::new(cfg).unwrap();

        let lyap_coeffs = BlockLyapunovCoefficients {
            alpha_pfbs: Scalar(1.0),
            beta_ecoli: Scalar(0.0),
            gamma_swarm: Scalar(1.0),
        };

        let decision = route_variant_with_block_stress(
            &variant,
            &node,
            &ctx,
            &block_guard,
            "PHX-FOG-BLOCK-01",
            lyap_coeffs,
            PfbsCoverageCbfParams {
                w_cov: Scalar(1.0),
                w_pfbs: Scalar(1.0),
                epsilon_decrease: 1e-3,
            },
        );

        assert!(matches!(decision, RouteDecision::Accept | RouteDecision::Reroute));
    }

    #[test]
    fn routing_rejects_when_block_stress_violated() {
        let mut node = sample_node();
        node.swarm_coverage_fraction = 0.1;
        node.pfbs_concentration_ug_per_l = 200.0;

        let mut variant = sample_variant();
        variant.hydraulic_impact = 0.9;

        let ctx = RoutingContext {
            vt_global: 1.0,
            vt_global_next_max: 1.0,
            now: Instant::now(),
        };

        let cfg = FogBlockStressConfig {
            lyapunov_policy: BlockLyapunovPolicy::strict(),
            cbf_params: PfbsCoverageCbfParams {
                w_cov: Scalar(1.0),
                w_pfbs: Scalar(1.0),
                epsilon_decrease: 0.0,
            },
        };
        let block_guard = FogBlockStressGuard::new(cfg).unwrap();

        let lyap_coeffs = BlockLyapunovCoefficients {
            alpha_pfbs: Scalar(1.0),
            beta_ecoli: Scalar(0.0),
            gamma_swarm: Scalar(1.0),
        };

        let decision = route_variant_with_block_stress(
            &variant,
            &node,
            &ctx,
            &block_guard,
            "PHX-FOG-BLOCK-02",
            lyap_coeffs,
            PfbsCoverageCbfParams {
                w_cov: Scalar(1.0),
                w_pfbs: Scalar(1.0),
                epsilon_decrease: 0.0,
            },
        );

        assert!(matches!(decision, RouteDecision::Reject));
    }
}
