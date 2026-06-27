// filepath: cybercore/prometheus_praxis/tests/fog_routing_smoke.rs

#![forbid(unsafe_code)]

use cybercore_prometheus_praxis::fog::{
    BioSurfaceMode,
    CyboVariant,
    MediaClass,
    NodeShard,
    RouteDecision,
    RoutingContext,
    route_variant,
    FogBlockStressGuard,
};

use cybercore_prometheus_praxis::lyapunov::block_adapter::{
    LyapunovBlockProjection,
    LyapunovCellProjection,
};

use cybercore_prometheus_praxis::lyapunov::block_lyapunov_ker::LyapunovKerBand;
use cybercore_prometheus_praxis::lyapunov::safe_step::SafeStepConfig;

/// Helper to build a simple Lyapunov band with a fixed Vt ceiling.
fn make_band(vt_ceiling: f64) -> LyapunovKerBand {
    LyapunovKerBand {
        vt_ceiling,
        // Any additional fields in LyapunovKerBand should be filled with
        // safe defaults or test constants. If the struct evolves, update
        // this helper accordingly.
        ..LyapunovKerBand::default()
    }
}

/// Helper to build a minimal SafeStepConfig for tests.
///
/// This should mirror your production defaults for non‑actuating routing.
fn make_safestep() -> SafeStepConfig {
    SafeStepConfig {
        // Use a small epsilon consistent with your KER replay harness.
        eps_k: 1.0e-6,
        eps_e: 1.0e-6,
        eps_r: 1.0e-6,
        eps_vt: 1.0e-6,
        // Fill other fields with safe defaults.
        ..SafeStepConfig::default()
    }
}

/// Helper to build a simple FOG guard with relaxed corridors suitable for tests.
fn make_fog_guard() -> FogBlockStressGuard {
    FogBlockStressGuard::new(0.90, 0.80, 0.05)
}

/// Helper to build a synthetic block projection and cells for a given Vt and stress.
fn make_block_and_cells(vt: f64, stress: f64) -> (LyapunovBlockProjection, Vec<LyapunovCellProjection>) {
    let block = LyapunovBlockProjection {
        // If your real struct has more fields, extend this initializer
        // with safe test values.
        vt_max: Some(vt),
        stress_max: Some(stress),
        ..LyapunovBlockProjection::default()
    };

    let cell = LyapunovCellProjection {
        vt: Some(vt),
        stress_index: Some(stress),
        cbf_margin: Some(0.10),
        ..LyapunovCellProjection::default()
    };

    (block, vec![cell])
}

/// Build a NodeShard with the given lane and media class and fixed stress coordinates.
fn make_node(node_id: &str, lane: &str, media: MediaClass) -> NodeShard {
    NodeShard {
        node_id: node_id.to_string(),
        region: "Phoenix-AZ".to_string(),
        media_class: media,
        bio_mode: BioSurfaceMode::BiofilmFriendly,
        // Fixed stress coordinates inside the test corridors.
        r_hydraulic: 0.40,
        r_struct: 0.40,
        r_bio_surface: 0.30,
        vt_max: 0.50,
        lane: lane.to_string(),
    }
}

/// Convenience wrapper to run routing and return the decision.
fn decide(
    node: &NodeShard,
    ctx: &RoutingContext,
    vt: f64,
    stress: f64,
) -> RouteDecision {
    let (block, cells) = make_block_and_cells(vt, stress);
    route_variant(node, ctx, Some(&block), &cells)
}

/// Smoke test: for identical stress coordinates and Vt, admissible should be
/// monotone across lanes: if PROD is admissible, EXPPROD and RESEARCH must be.
/// Conversely, if RESEARCH is rejected, higher lanes must also be rejected.
#[test]
fn lane_monotonicity_for_identical_stress() {
    let ctx = RoutingContext {
        lyap_band: make_band(0.90),
        safestep: make_safestep(),
        fog_guard: make_fog_guard(),
    };

    let vt = 0.50;
    let stress = 0.40;

    let node_research = make_node("node-r", "RESEARCH", MediaClass::Canal);
    let node_expprod = make_node("node-e", "EXPPROD", MediaClass::Canal);
    let node_prod = make_node("node-p", "PROD", MediaClass::Canal);

    let dec_r = decide(&node_research, &ctx, vt, stress);
    let dec_e = decide(&node_expprod, &ctx, vt, stress);
    let dec_p = decide(&node_prod, &ctx, vt, stress);

    // If PROD is admissible, EXPPROD and RESEARCH must be admissible as well.
    if dec_p.admissible {
        assert!(
            dec_e.admissible,
            "Lane monotonicity violated: PROD admissible but EXPPROD is not"
        );
        assert!(
            dec_r.admissible,
            "Lane monotonicity violated: PROD admissible but RESEARCH is not"
        );
    }

    // If RESEARCH is not admissible, higher lanes must not be admissible either.
    if !dec_r.admissible {
        assert!(
            !dec_e.admissible,
            "Lane monotonicity violated: RESEARCH blocked but EXPPROD admissible"
        );
        assert!(
            !dec_p.admissible,
            "Lane monotonicity violated: RESEARCH blocked but PROD admissible"
        );
    }

    // Sanity: chosen variants align with lanes.
    assert_eq!(dec_r.chosen_variant, CyboVariant::SensingLite);
    assert_eq!(dec_e.chosen_variant, CyboVariant::AnalyticsL2);
    assert_eq!(dec_p.chosen_variant, CyboVariant::ControlClosedLoop);
}

/// Smoke test: for identical node stress and lane, admissible should be
/// monotonically non‑increasing as we move from Air -> WaterAdj -> Canal.
#[test]
fn media_class_monotonicity_for_identical_node() {
    let ctx = RoutingContext {
        lyap_band: make_band(0.90),
        safestep: make_safestep(),
        fog_guard: make_fog_guard(),
    };

    let vt = 0.50;
    let stress = 0.40;

    let node_air = make_node("node-air", "EXPPROD", MediaClass::Air);
    let node_water = make_node("node-water", "EXPPROD", MediaClass::WaterAdj);
    let node_canal = make_node("node-canal", "EXPPROD", MediaClass::Canal);

    let dec_air = decide(&node_air, &ctx, vt, stress);
    let dec_water = decide(&node_water, &ctx, vt, stress);
    let dec_canal = decide(&node_canal, &ctx, vt, stress);

    // If a stricter media class is admissible, the looser one must be too.
    if dec_canal.admissible {
        assert!(
            dec_water.admissible,
            "Media monotonicity violated: Canal admissible but WaterAdj is not"
        );
        assert!(
            dec_air.admissible,
            "Media monotonicity violated: Canal admissible but Air is not"
        );
    }

    if dec_water.admissible {
        assert!(
            dec_air.admissible,
            "Media monotonicity violated: WaterAdj admissible but Air is not"
        );
    }
}

/// Smoke test: FOG block stress guard should reject a clearly unsafe block and
/// propagate that rejection into the routing decision for water/canal nodes.
#[test]
fn block_stress_guard_propagates_to_routing() {
    // Tight corridors to force a violation.
    let ctx = RoutingContext {
        lyap_band: make_band(0.60),
        safestep: make_safestep(),
        fog_guard: FogBlockStressGuard::new(0.60, 0.30, 0.10),
    };

    // Node is otherwise inside local corridors.
    let mut node = make_node("node-guard", "EXPPROD", MediaClass::Canal);
    node.r_hydraulic = 0.25;
    node.r_struct = 0.25;
    node.r_bio_surface = 0.10;
    node.vt_max = 0.55;

    // But the block‑level Vt and stress violate the guard.
    let vt = 0.80;
    let stress = 0.50;

    let (block, cells) = make_block_and_cells(vt, stress);
    let decision = route_variant(&node, &ctx, Some(&block), &cells);

    assert!(
        !decision.admissible,
        "Expected routing to be inadmissible when block stress guard fails"
    );

    let block_decision = decision
        .block_stress
        .expect("Expected block_stress to be populated for canal node");

    assert!(
        !block_decision.ok,
        "Expected block_stress.ok to be false for violating block"
    );
}
