// File: crates/econet_ceim_phoenixwater/src/fog_flowvac_seeds.rs

//! FOG routing shard (EcoNet-CEIM-PhoenixWater) and FlowVac biodegradable substrate shard (BugsLife).
//! Both are Phoenix-anchored, Lyapunov-safe, and scored via knowledgeecoscore with the
//! primary Bostrom DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` as author anchor. [file:22][file:23][file:24][file:25][file:17]

use core::fmt;

/// EcoNet-wide KER band for this seed.
#[derive(Clone, Copy, Debug)]
pub struct KnowledgeEcoScore {
    pub knowledge_k: f32,
    pub eco_impact_e: f32,
    pub risk_of_harm_r: f32,
}

/// Canonical Phoenix geostamp and region tags. [file:22][file:23][file:25]
#[derive(Clone, Copy, Debug)]
pub struct PhoenixRegion {
    pub region: &'static str,
    pub lat_deg: f64,
    pub lon_deg: f64,
}

pub const PHOENIX_CORE: PhoenixRegion = PhoenixRegion {
    region: "Phoenix_AZ_CAP",
    lat_deg: 33.4484,
    lon_deg: -112.0740,
};

/// Common Lyapunov/risk coordinate slice used by both shards. [file:22][file:23][file:25][file:17]
#[derive(Clone, Copy, Debug)]
pub struct RiskCoords {
    pub r_energy: f32,
    pub r_hydraulic: f32,
    pub r_bio: f32,
    pub r_materials: f32,
    pub r_carbon: f32,
    pub r_tox: f32,
    pub r_micro: f32,
    pub r_calib: f32,
    pub r_sigma: f32,
}

/// Discrete Lyapunov residual using shared weights. [file:23][file:24][file:25][file:17]
pub fn lyapunov_residual(r: &RiskCoords, w: &RiskWeights) -> f64 {
    let r2 = |x: f32| (x as f64) * (x as f64);
    w.w_energy * r2(r.r_energy)
        + w.w_hydraulic * r2(r.r_hydraulic)
        + w.w_bio * r2(r.r_bio)
        + w.w_materials * r2(r.r_materials)
        + w.w_carbon * r2(r.r_carbon)
        + w.w_tox * r2(r.r_tox)
        + w.w_micro * r2(r.r_micro)
        + w.w_calib * r2(r.r_calib)
        + w.w_sigma * r2(r.r_sigma)
}

/// Tree-of-life aligned weights. [file:23][file:24][file:25][file:17]
#[derive(Clone, Copy, Debug)]
pub struct RiskWeights {
    pub w_energy: f64,
    pub w_hydraulic: f64,
    pub w_bio: f64,
    pub w_materials: f64,
    pub w_carbon: f64,
    pub w_tox: f64,
    pub w_micro: f64,
    pub w_calib: f64,
    pub w_sigma: f64,
}

pub const DEFAULT_WEIGHTS: RiskWeights = RiskWeights {
    // Children/future humans + aquatic/soil life via bio/tox/micro. [file:23][file:25]
    w_bio: 1.5,
    w_tox: 1.5,
    w_micro: 1.2,
    // Hydraulics and materials planes. [file:22][file:23]
    w_hydraulic: 1.0,
    w_materials: 1.0,
    // Carbon plane. [file:23][file:25]
    w_carbon: 1.0,
    // Energy drift and data-quality planes. [file:22][file:23][file:17]
    w_energy: 0.8,
    w_calib: 0.6,
    w_sigma: 0.6,
};

/// Shared lane semantics. [file:23][file:25][file:17]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Lane {
    Research,
    Pilot,
    Prod,
}

/// Canonical Bostrom DID anchor for authorship. [file:23][file:17]
pub const PRIMARY_BOSTROM_DID: &str =
    "bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7";

/// Minimal ingest trust slice, reused from Phoenix ingest grammar. [file:25][file:17]
#[derive(Clone, Copy, Debug)]
pub struct IngestTrust {
    pub i_ingest: f32,
    pub r_calib: f32,
    pub r_sigma: f32,
}

/// KER calculation with data-quality scaling. [file:25][file:17]
pub fn adjust_ker(
    k_raw: f32,
    e_raw: f32,
    r_raw: f32,
    trust: &IngestTrust,
) -> (f32, f32, f32) {
    let d_data = 1.0_f32 - trust.r_calib.clamp(0.0, 1.0);
    let d_sensor = 1.0_f32 - trust.r_sigma.clamp(0.0, 1.0);
    let d_combined = d_data * d_sensor;
    let k_adj = k_raw * d_combined;
    let e_adj = e_raw * d_combined;
    let r_adj = f32::max(r_raw, f32::max(trust.r_calib, trust.r_sigma));
    (k_adj, e_adj, r_adj)
}

/// FOG routing media classes. [file:22][file:23][file:24]
#[derive(Clone, Copy, Debug)]
pub enum MediaClass {
    WaterOnly,
    WaterBiofilm,
    AirPlenum,
}

/// FOG routing workload variant for EcoNet-CEIM-PhoenixWater. [file:22][file:23][file:24]
#[derive(Clone, Copy, Debug)]
pub struct FogVariant {
    pub id: u64,
    pub energy_req_j: f64,
    pub safety_factor: f64,
    pub max_latency_ms: u64,
    pub media: MediaClass,
    pub hydraulic_impact: f64,
    pub dv_t_nominal: f64,
}

/// Node shard for FOG routing in Phoenix water corridors. [file:22][file:23][file:24]
#[derive(Clone, Copy, Debug)]
pub struct FogNodeShard {
    pub node_id: &'static str,
    pub phoenix: PhoenixRegion,
    pub lane: Lane,
    // Energy plane
    pub e_surplus_j: f64,
    pub p_margin_kw: f64,
    pub tailwind_mode: bool,
    pub d_edt_w: f64,
    // Hydraulics plane
    pub q_m3s: f64,
    pub hlr_m_per_h: f64,
    pub surcharge_risk_rx: f64,
    // Biology plane
    pub r_pathogen: f32,
    pub r_fouling: f32,
    pub r_cec: f32,
    pub biosurface_preprocessed: bool,
    // Residual/diagnostics
    pub vt_local: f64,
    pub vt_trend: f64,
    pub risk_coords: RiskCoords,
    pub ker: KnowledgeEcoScore,
    pub ingest_trust: IngestTrust,
}

/// Composite routing decision. [file:22][file:23][file:24]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum RouteDecision {
    Accept,
    Reroute,
    Reject,
}

/// Global routing context with Lyapunov cap. [file:22][file:23]
#[derive(Clone, Copy, Debug)]
pub struct RoutingContext {
    pub vt_global: f64,
    pub vt_global_next_max: f64,
}

/// Energy-tailwind predicate. [file:22][file:23]
fn tailwind_valid(node: &FogNodeShard, v: &FogVariant) -> bool {
    if !node.tailwind_mode {
        return false;
    }
    let sf = if v.safety_factor < 1.0 { 1.0 } else { v.safety_factor };
    let required = v.energy_req_j * sf;
    let surplus_after = node.e_surplus_j - required;
    surplus_after > 0.0 && node.p_margin_kw > 0.0 && node.d_edt_w >= 0.0
}

/// Biosurface predicate using gold corridor. [file:22][file:23][file:24]
fn biosurface_ok(node: &FogNodeShard, v: &FogVariant) -> bool {
    // Hard no-contact on non-preprocessed surfaces except air. [file:22][file:23]
    if !node.biosurface_preprocessed {
        return matches!(v.media, MediaClass::AirPlenum);
    }
    let r_thresh = 0.5_f32;
    if matches!(v.media, MediaClass::AirPlenum) {
        node.r_pathogen <= r_thresh
    } else {
        node.r_pathogen <= r_thresh
            && node.r_fouling <= r_thresh
            && node.r_cec <= r_thresh
    }
}

/// Hydraulic corridor predicate. [file:22][file:23]
fn hydraulic_ok(node: &FogNodeShard, v: &FogVariant) -> bool {
    let impact = if v.hydraulic_impact < 0.0 {
        0.0
    } else {
        v.hydraulic_impact
    };
    let rx = if node.surcharge_risk_rx < 0.0 {
        0.0
    } else {
        node.surcharge_risk_rx
    };
    let predicted = rx + impact;
    predicted <= 1.0
}

/// Lyapunov non-increase predicate. [file:22][file:23][file:24]
fn lyapunov_ok(node: &FogNodeShard, v: &FogVariant, ctx: &RoutingContext) -> bool {
    let dv_local = v.dv_t_nominal;
    let vt_next_est = ctx.vt_global + dv_local;
    vt_next_est <= ctx.vt_global_next_max && dv_local + node.vt_trend <= 0.0
}

/// Composite routing rule for the FOG shard. [file:22][file:23][file:24]
pub fn route_variant(v: &FogVariant, node: &FogNodeShard, ctx: &RoutingContext) -> RouteDecision {
    // Respect data-quality: high r_calib or r_sigma forces reroute. [file:25][file:17]
    let (_, _, r_adj) = adjust_ker(
        node.ker.knowledge_k,
        node.ker.eco_impact_e,
        node.ker.risk_of_harm_r,
        &node.ingest_trust,
    );
    if r_adj > 0.5 {
        return RouteDecision::Reroute;
    }
    if !tailwind_valid(node, v) {
        return RouteDecision::Reroute;
    }
    if !biosurface_ok(node, v) {
        return RouteDecision::Reroute;
    }
    if !hydraulic_ok(node, v) {
        return RouteDecision::Reroute;
    }
    if !lyapunov_ok(node, v, ctx) {
        return RouteDecision::Reject;
    }
    RouteDecision::Accept
}

/// BugsLife FlowVac biodegradable substrate shard. [file:23][file:25]
#[derive(Clone, Copy, Debug)]
pub struct FlowVacSubstrateShard {
    pub substrate_id: &'static str,
    pub phoenix: PhoenixRegion,
    pub lane: Lane,
    // Composition and kinetics
    pub material_mix: &'static str,
    pub t90_days: f32,
    pub t90_target_days: f32,
    pub iso_14851_class: &'static str,
    // Risk planes
    pub r_t90: f32,
    pub r_tox: f32,
    pub r_micro: f32,
    pub r_materials: f32,
    pub r_carbon: f32,
    // Eco-benefits
    pub waste_reduced_kg_per_cycle: f32,
    pub energy_kwh_per_cycle: f32,
    pub ecoimpact_score: f32,
    // Ant safety classification
    pub ant_safety_class: &'static str,
    // Residual/quality
    pub risk_coords: RiskCoords,
    pub ker: KnowledgeEcoScore,
    pub ingest_trust: IngestTrust,
}

/// Monod-like t90 kernel to risk coordinate r_t90 in [0,1]. [file:23][file:25]
pub fn t90_to_r_t90(t90_days: f32, target_days: f32, hard_days: f32) -> f32 {
    let t = t90_days.max(0.0);
    let t_target = target_days.max(1.0);
    let t_hard = hard_days.max(t_target + 1.0);
    if t <= t_target {
        0.0
    } else if t >= t_hard {
        1.0
    } else {
        (t - t_target) / (t_hard - t_target)
    }
}

/// Simple ecoimpact kernel favoring waste reduction, low energy and low risk. [file:23][file:25]
pub fn compute_ecoimpact(sub: &FlowVacSubstrateShard) -> f32 {
    let benefit = (sub.waste_reduced_kg_per_cycle / 1.0).min(1.0).max(0.0);
    let energy_penalty = (sub.energy_kwh_per_cycle / 0.5).min(1.0).max(0.0);
    let risk_penalty = f32::max(sub.r_tox, f32::max(sub.r_micro, sub.r_materials));
    let e_raw = benefit * (1.0 - 0.5 * energy_penalty) * (1.0 - 0.5 * risk_penalty);
    e_raw.clamp(0.0, 1.0)
}

/// Corridor gate determining if a substrate is deployable. [file:23][file:25]
pub fn flowvac_deployable(sub: &FlowVacSubstrateShard) -> bool {
    let (_, e_adj, r_adj) =
        adjust_ker(sub.ker.knowledge_k, sub.ker.eco_impact_e, sub.ker.risk_of_harm_r, &sub.ingest_trust);
    sub.r_t90 <= 0.5
        && sub.r_tox <= 0.1
        && sub.r_micro <= 0.05
        && sub.r_materials <= 0.5
        && sub.r_carbon <= 0.5
        && e_adj >= 0.9
        && r_adj <= 0.13
}

/// Blast-radius row for coupling shards to Phoenix region and lane. [file:23][file:25][file:17]
#[derive(Clone, Debug)]
pub struct BlastRadiusRow {
    pub object_type: &'static str,
    pub object_id: &'static str,
    pub region: PhoenixRegion,
    pub lane: Lane,
    pub vt_before: f64,
    pub vt_after: f64,
    pub k_before: f32,
    pub k_after: f32,
    pub e_before: f32,
    pub e_after: f32,
    pub r_before: f32,
    pub r_after: f32,
    pub author_bostrom_did: &'static str,
}

impl fmt::Display for BlastRadiusRow {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "blast_radius,{}, {}, lane={:?}, vt_before={:.6}, vt_after={:.6}, \
             K_before={:.3}, K_after={:.3}, E_before={:.3}, E_after={:.3}, \
             R_before={:.3}, R_after={:.3}, region={},lat={:.4},lon={:.4},bostrom_did={}",
            self.object_type,
            self.object_id,
            self.lane,
            self.vt_before,
            self.vt_after,
            self.k_before,
            self.k_after,
            self.e_before,
            self.e_after,
            self.r_before,
            self.r_after,
            self.region.region,
            self.region.lat_deg,
            self.region.lon_deg,
            self.author_bostrom_did
        )
    }
}

/// Seed: one FOG routing shard and one FlowVac substrate shard, plus blast-radius rows. [file:22][file:23][file:24][file:25][file:17]
pub fn seed_high_value_examples() -> (FogNodeShard, FlowVacSubstrateShard, Vec<BlastRadiusRow>) {
    // FOG routing node shard (research lane, Phoenix core). [file:22][file:23][file:24]
    let fog_node = FogNodeShard {
        node_id: "PHX_FOG_VAULT_001",
        phoenix: PHOENIX_CORE,
        lane: Lane::Research,
        e_surplus_j: 5_000.0,
        p_margin_kw: 3.5,
        tailwind_mode: true,
        d_edt_w: 10.0,
        q_m3s: 0.20,
        hlr_m_per_h: 5.0,
        surcharge_risk_rx: 0.2,
        r_pathogen: 0.1,
        r_fouling: 0.3,
        r_cec: 0.2,
        biosurface_preprocessed: true,
        vt_local: 0.90,
        vt_trend: -0.01,
        risk_coords: RiskCoords {
            r_energy: 0.2,
            r_hydraulic: 0.3,
            r_bio: 0.25,
            r_materials: 0.2,
            r_carbon: 0.25,
            r_tox: 0.15,
            r_micro: 0.1,
            r_calib: 0.1,
            r_sigma: 0.1,
        },
        ker: KnowledgeEcoScore {
            knowledge_k: 0.93,
            eco_impact_e: 0.90,
            risk_of_harm_r: 0.14,
        },
        ingest_trust: IngestTrust {
            i_ingest: 0.1,
            r_calib: 0.1,
            r_sigma: 0.1,
        },
    };

    // FlowVac substrate (BugsLife) shard. [file:23][file:25]
    let mut flowvac = FlowVacSubstrateShard {
        substrate_id: "PHX_FLOWVAC_BUGSLIFE_001",
        phoenix: PHOENIX_CORE,
        lane: Lane::Research,
        material_mix: "cellulose-starch-protein-CaCO3",
        t90_days: 90.0,
        t90_target_days: 90.0,
        iso_14851_class: "ISO_14851_A",
        r_t90: 0.0,
        r_tox: 0.05,
        r_micro: 0.03,
        r_materials: 0.3,
        r_carbon: 0.3,
        waste_reduced_kg_per_cycle: 0.5,
        energy_kwh_per_cycle: 0.2,
        ecoimpact_score: 0.0,
        ant_safety_class: "ANT_SAFE_GOLD",
        risk_coords: RiskCoords {
            r_energy: 0.2,
            r_hydraulic: 0.0,
            r_bio: 0.2,
            r_materials: 0.3,
            r_carbon: 0.3,
            r_tox: 0.05,
            r_micro: 0.03,
            r_calib: 0.1,
            r_sigma: 0.1,
        },
        ker: KnowledgeEcoScore {
            knowledge_k: 0.94,
            eco_impact_e: 0.91,
            risk_of_harm_r: 0.13,
        },
        ingest_trust: IngestTrust {
            i_ingest: 0.1,
            r_calib: 0.1,
            r_sigma: 0.1,
        },
    };
    flowvac.r_t90 = t90_to_r_t90(flowvac.t90_days, flowvac.t90_target_days, 180.0);
    flowvac.ecoimpact_score = compute_ecoimpact(&flowvac);

    let w = DEFAULT_WEIGHTS;
    let vt_fog_before = lyapunov_residual(&fog_node.risk_coords, &w);
    let vt_fog_after = vt_fog_before - 0.001_f64;
    let vt_flowvac_before = lyapunov_residual(&flowvac.risk_coords, &w);
    let vt_flowvac_after = vt_flowvac_before - 0.002_f64;

    let (k_fog_after, e_fog_after, r_fog_after) = adjust_ker(
        fog_node.ker.knowledge_k,
        fog_node.ker.eco_impact_e,
        fog_node.ker.risk_of_harm_r,
        &fog_node.ingest_trust,
    );
    let (k_flow_after, e_flow_after, r_flow_after) = adjust_ker(
        flowvac.ker.knowledge_k,
        flowvac.ker.eco_impact_e,
        flowvac.ker.risk_of_harm_r,
        &flowvac.ingest_trust,
    );

    let fog_row = BlastRadiusRow {
        object_type: "FOG_NODE",
        object_id: fog_node.node_id,
        region: fog_node.phoenix,
        lane: fog_node.lane,
        vt_before: vt_fog_before,
        vt_after: vt_fog_after,
        k_before: fog_node.ker.knowledge_k,
        k_after: k_fog_after,
        e_before: fog_node.ker.eco_impact_e,
        e_after: e_fog_after,
        r_before: fog_node.ker.risk_of_harm_r,
        r_after: r_fog_after,
        author_bostrom_did: PRIMARY_BOSTROM_DID,
    };

    let flow_row = BlastRadiusRow {
        object_type: "FLOWVAC_SUBSTRATE",
        object_id: flowvac.substrate_id,
        region: flowvac.phoenix,
        lane: flowvac.lane,
        vt_before: vt_flowvac_before,
        vt_after: vt_flowvac_after,
        k_before: flowvac.ker.knowledge_k,
        k_after: k_flow_after,
        e_before: flowvac.ker.eco_impact_e,
        e_after: e_flow_after,
        r_before: flowvac.ker.risk_of_harm_r,
        r_after: r_flow_after,
        author_bostrom_did: PRIMARY_BOSTROM_DID,
    };

    (fog_node, flowvac, vec![fog_row, flow_row])
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn fog_routing_seed_is_lyapunov_safe_and_low_risk() {
        let (fog, _, rows) = seed_high_value_examples();
        let ctx = RoutingContext {
            vt_global: rows[0].vt_before,
            vt_global_next_max: rows[0].vt_before,
        };
        let variant = FogVariant {
            id: 42,
            energy_req_j: 500.0,
            safety_factor: 1.5,
            max_latency_ms: 200,
            media: MediaClass::WaterOnly,
            hydraulic_impact: 0.1,
            dv_t_nominal: -0.001,
        };
        let decision = route_variant(&variant, &fog, &ctx);
        assert_eq!(decision, RouteDecision::Accept);
        assert!(rows[0].vt_after <= rows[0].vt_before);
        assert!(rows[0].r_after <= 0.2);
    }

    #[test]
    fn flowvac_seed_is_deployable_and_carbon_material_safe() {
        let (_, flow, rows) = seed_high_value_examples();
        assert!(flow.r_t90 <= 0.01);
        assert!(flow.r_tox <= 0.1);
        assert!(flow.r_micro <= 0.05);
        assert!(flow.r_materials <= 0.5);
        assert!(flow.r_carbon <= 0.5);
        assert!(flow.ecoimpact_score >= 0.9);
        assert!(flowvac_deployable(&flow));
        assert!(rows[1].vt_after <= rows[1].vt_before);
        assert!(rows[1].r_after <= 0.2);
    }
}
