// Path: Prometheus-Praxis/eval/governance/cli/src/main.rs
// License: MIT OR Apache-2.0

#![deny(unsafe_code)]
#![forbid(hidden_glob_reexports)]

use std::fs::File;
use std::io::Write;
use std::path::PathBuf;

use ppx_eval_components::{
    AdvectionKernel,
    MarlArchitecture,
    PhoenixContext,
    PhoenixStack,
    StreamingPipeline,
};
use ppx_eval_rubric::{
    PhoenixEligibilityThresholds,
    SystemEvaluable,
    SystemEvidence,
    emit_aln_evidence,
};

fn main() {
    // System identifier for this evaluation instance.
    let system_id = "PhoenixIntegratedV1";

    // Phoenix context for this governance run.
    let ctx = PhoenixContext::phoenix_default();

    // Construct components with reasonable, but explicit, placeholder metrics.
    // These should be replaced by real metrics derived from DUSTIEAIM, pilot programs,
    // and sensor data as the system matures.[130]
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

    // Phoenix thresholds including the five gates, aligned with the report.[130]
    let thresholds = PhoenixEligibilityThresholds::default();

    let stack = PhoenixStack::new(adv.clone(), marl.clone(), stream.clone(), thresholds);

    // Collect components into a vector for system evaluation.
    let components: Vec<Box<dyn ppx_eval_rubric::ComponentEvaluable>> =
        vec![Box::new(adv), Box::new(marl), Box::new(stream)];

    // Run Phase B: integrated Phoenix eligibility.
    let eligibility = stack.evaluate_system(&components);

    println!("=== Phoenix Integrated Eligibility (Governance CLI) ===");
    println!("Eligible (raw): {}", eligibility.eligible);
    println!("Notes:\n{}", eligibility.notes);

    println!();
    println!("Integrated profile (seven dimensions):");
    for (dim, value) in ppx_eval_rubric::profile_to_rows(&eligibility.profile) {
        println!(
            "  {:22} = {:.3}",
            dimension_name(dim),
            value
        );
    }

    // Build SystemEvidence for ALN emission.
    // For now, we set all governance flags to false, consistent with the report’s
    // conclusion that the system is not yet Eligible and requires further proof.[130]
    let evidence = SystemEvidence {
        profile: eligibility.profile.clone(),
        domain_performance_ok: false,
        safety_case_documented: false,
        sovereignty_compliant: false,
        energy_neutral_or_renew: false,
        explainable_and_audited: false,
    };

    // Emit ALN fragment compatible with PhoenixEligibilityGate.phx_eligibility_gate.aln.
    let aln_text = emit_aln_evidence(system_id, &evidence, &thresholds);

    // Write ALN to a file next to phx_eligibility_gate.aln for ingestion by Cybercore.
    let mut path = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    path.pop(); // cli
    path.push("phx_eligibility_gate_instance.aln");

    match write_aln_file(&path, &aln_text) {
        Ok(()) => {
            println!();
            println!("ALN evidence written to: {}", path.display());
        }
        Err(err) => {
            eprintln!("Failed to write ALN evidence file: {err}");
        }
    }
}

fn write_aln_file(path: &PathBuf, content: &str) -> Result<(), std::io::Error> {
    let mut file = File::create(path)?;
    file.write_all(content.as_bytes())?;
    file.flush()?;
    Ok(())
}

fn dimension_name(dim: ppx_eval_rubric::Dimension) -> &'static str {
    match dim {
        ppx_eval_rubric::Dimension::KnowledgeFactor => "KnowledgeFactor",
        ppx_eval_rubric::Dimension::EcoImpact => "EcoImpact",
        ppx_eval_rubric::Dimension::RiskOfHarm => "RiskOfHarm",
        ppx_eval_rubric::Dimension::Robustness => "Robustness",
        ppx_eval_rubric::Dimension::Sovereignty => "Sovereignty",
        ppx_eval_rubric::Dimension::EnergyEfficiency => "EnergyEfficiency",
        ppx_eval_rubric::Dimension::GovernanceAlignment => "GovernanceAlignment",
    }
}
