// Path: Prometheus-Praxis/eval/cli/src/main.rs
// License: MIT OR Apache-2.0

#![deny(unsafe_code)]
#![forbid(hidden_glob_reexports)]

use ppx_eval_components::{
    AdvectionKernel, MarlArchitecture, PhoenixContext, PhoenixStack, StreamingPipeline,
    phoenix_component_matrix,
};
use ppx_eval_rubric::{
    ComponentEvaluable, Dimension, PhoenixEligibilityThresholds, SevenDimProfile,
    SystemEvidence, SystemEligibility, emit_aln_evidence, profile_to_rows,
};

fn main() {
    let ctx = PhoenixContext::phoenix_default();

    let adv = AdvectionKernel {
        scheme_name: "upwind_cfl_safe".to_string(),
        cfl_safety_margin: 0.9,
        physical_fidelity_index: 0.92,
        restored_flow_ratio: 0.80,
        numerical_robustness_index: 0.88,
        ctx: ctx.clone(),
    };

    let marl = MarlArchitecture {
        policy_alignment_index: 0.90,
        rogue_pattern_resilience: 0.86,
        multi_actor_scalability: 0.84,
        consent_corridor_strength: 0.93,
        cybercore_binding_strength: 0.95,
        ctx: ctx.clone(),
    };

    let stream = StreamingPipeline {
        end_to_end_latency_ms: 150.0,
        failure_recovery_index: 0.88,
        data_sovereignty_index: 0.94,
        energy_cost_per_event: 0.30,
        biosignal_integration_index: 0.89,
        ctx,
    };

    let thresholds = PhoenixEligibilityThresholds::default();
    let stack = PhoenixStack::new(adv.clone(), marl.clone(), stream.clone(), thresholds);

    print_header();
    let matrix = phoenix_component_matrix(&adv, &marl, &stream);
    for (id, profile) in matrix.iter() {
        print_row(id, profile);
    }
    print_footer();

    let components: Vec<Box<dyn ComponentEvaluable>> =
        vec![Box::new(adv), Box::new(marl), Box::new(stream)];

    let eligibility = stack.evaluate_system(&components);

    println!();
    println!("=== Phoenix Integrated Eligibility ===");
    println!("Eligible: {}", eligibility.eligible);
    println!("Notes:");
    println!("{}", eligibility.notes.trim_end());
    println!();

    println!("Integrated profile:");
    for (dim, value) in profile_to_rows(&eligibility.profile) {
        println!("  {:22} = {:.3}", dimension_name(dim), value);
    }

    let system_id = "PhoenixIntegratedV1";
    export_phoenix_stack_aln(system_id, &eligibility);
}

fn export_phoenix_stack_aln(system_id: &str, eligibility: &SystemEligibility) {
    let thresholds = PhoenixEligibilityThresholds::default();

    let evidence = SystemEvidence {
        profile: eligibility.profile.clone(),
        domain_performance_ok: false,
        safety_case_documented: false,
        sovereignty_compliant: false,
        energy_neutral_or_renew: false,
        explainable_and_audited: false,
    };

    let aln_text = emit_aln_evidence(system_id, &evidence, &thresholds);
    println!();
    println!("=== Phoenix Eligibility ALN Export ===");
    println!("{}", aln_text);
}

fn print_header() {
    println!("=== Prometheus-Praxis Component Evaluation Matrix (Phoenix) ===");
    println!();
    println!(
        "{:<20} | {:>8} {:>8} {:>8} {:>10} {:>12} {:>10} {:>20}",
        "Component",
        "Know",
        "Eco",
        "Risk",
        "Robust",
        "Sovereign",
        "Energy",
        "Governance"
    );
    println!("{}", "-".repeat(20 + 3 + 8 * 3 + 10 + 12 + 10 + 20));
}

fn print_row(id: &str, profile: &SevenDimProfile) {
    println!(
        "{:<20} | {:>8.3} {:>8.3} {:>8.3} {:>10.3} {:>12.3} {:>10.3} {:>20.3}",
        id,
        profile.knowledge_factor.0,
        profile.eco_impact.0,
        profile.risk_of_harm.0,
        profile.robustness.0,
        profile.sovereignty.0,
        profile.energy_efficiency.0,
        profile.governance_alignment.0,
    );
}

fn print_footer() {
    println!("{}", "-".repeat(20 + 3 + 8 * 3 + 10 + 12 + 10 + 20));
}

fn dimension_name(dim: Dimension) -> &'static str {
    match dim {
        Dimension::KnowledgeFactor => "KnowledgeFactor",
        Dimension::EcoImpact => "EcoImpact",
        Dimension::RiskOfHarm => "RiskOfHarm",
        Dimension::Robustness => "Robustness",
        Dimension::Sovereignty => "Sovereignty",
        Dimension::EnergyEfficiency => "EnergyEfficiency",
        Dimension::GovernanceAlignment => "GovernanceAlignment",
    }
}
