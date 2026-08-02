// Path: Prometheus-Praxis/eval/governance/cli/src/main.rs
// License: MIT OR Apache-2.0

#![deny(unsafe_code)]
#![forbid(hidden_glob_reexports)]

use std::fs::File;
use std::io::{Read, Write};
use std::path::PathBuf;

use clap::Parser;
use serde::{Deserialize, Serialize};

use ppx_eval_components::{
    AdvectionKernel,
    MarlArchitecture,
    PhoenixContext,
    PhoenixStack,
    StreamingPipeline,
};
use ppx_eval_rubric::{
    ComponentEvaluable,
    Dimension,
    PhoenixEligibilityThresholds,
    SystemEvaluable,
    SystemEvidence,
    emit_aln_evidence,
    profile_to_rows,
};

#[derive(Parser, Debug)]
#[command(
    name = "ppx-governance-cli",
    about = "Prometheus-Praxis governance CLI for Phoenix eligibility evaluation and ALN emission."
)]
struct Args {
    #[arg(long, default_value = "PhoenixIntegratedV1")]
    system_id: String,

    #[arg(long)]
    config: Option<PathBuf>,

    #[arg(long)]
    domain_ok: bool,

    #[arg(long)]
    safety_ok: bool,

    #[arg(long)]
    sovereignty_ok: bool,

    #[arg(long)]
    energy_ok: bool,

    #[arg(long)]
    explainability_ok: bool,

    #[arg(long)]
    output_aln: Option<PathBuf>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
struct GovConfig {
    domain_performance_ok: bool,
    safety_case_documented: bool,
    sovereignty_compliant: bool,
    energy_neutral_or_renew: bool,
    explainable_and_audited: bool,
}

impl GovConfig {
    fn default_not_eligible() -> Self {
        GovConfig {
            domain_performance_ok: false,
            safety_case_documented: false,
            sovereignty_compliant: false,
            energy_neutral_or_renew: false,
            explainable_and_audited: false,
        }
    }
}

fn main() {
    let args = Args::parse();

    let mut cfg = match &args.config {
        Some(path) => match load_config(path) {
            Ok(c) => c,
            Err(err) => {
                eprintln!("Failed to load config from {}: {err}", path.display());
                GovConfig::default_not_eligible()
            }
        },
        None => GovConfig::default_not_eligible(),
    };

    if args.domain_ok {
        cfg.domain_performance_ok = true;
    }
    if args.safety_ok {
        cfg.safety_case_documented = true;
    }
    if args.sovereignty_ok {
        cfg.sovereignty_compliant = true;
    }
    if args.energy_ok {
        cfg.energy_neutral_or_renew = true;
    }
    if args.explainability_ok {
        cfg.explainable_and_audited = true;
    }

    println!("=== Governance Evidence Configuration ===");
    println!("domain_performance_ok    = {}", cfg.domain_performance_ok);
    println!("safety_case_documented   = {}", cfg.safety_case_documented);
    println!("sovereignty_compliant    = {}", cfg.sovereignty_compliant);
    println!("energy_neutral_or_renew  = {}", cfg.energy_neutral_or_renew);
    println!("explainable_and_audited  = {}", cfg.explainable_and_audited);
    println!();

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

    let components: Vec<Box<dyn ComponentEvaluable>> =
        vec![Box::new(adv), Box::new(marl), Box::new(stream)];

    let eligibility = stack.evaluate_system(&components);

    println!("=== Phoenix Integrated Eligibility (Governance CLI) ===");
    println!("Eligible (raw, before governance gates): {}", eligibility.eligible);
    println!("Notes:\n{}", eligibility.notes);
    println!();
    println!("Integrated profile (seven dimensions):");
    for (dim, value) in profile_to_rows(&eligibility.profile) {
        println!(
            "  {:22} = {:.3}",
            dimension_name(dim),
            value
        );
    }

    let evidence = SystemEvidence {
        profile: eligibility.profile.clone(),
        domain_performance_ok: cfg.domain_performance_ok,
        safety_case_documented: cfg.safety_case_documented,
        sovereignty_compliant: cfg.sovereignty_compliant,
        energy_neutral_or_renew: cfg.energy_neutral_or_renew,
        explainable_and_audited: cfg.explainable_and_audited,
    };

    let aln_text = emit_aln_evidence(&args.system_id, &evidence, &thresholds);

    let output_path = args.output_aln.unwrap_or_else(|| {
        let mut p = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
        p.pop();
        p.push("phx_eligibility_gate_instance.aln");
        p
    });

    match write_aln_file(&output_path, &aln_text) {
        Ok(()) => {
            println!();
            println!("ALN evidence written to: {}", output_path.display());
        }
        Err(err) => {
            eprintln!("Failed to write ALN evidence file: {err}");
        }
    }
}

fn load_config(path: &PathBuf) -> Result<GovConfig, std::io::Error> {
    let mut file = File::open(path)?;
    let mut buf = String::new();
    file.read_to_string(&mut buf)?;
    let cfg: GovConfig = serde_json::from_str(&buf)
        .map_err(|e| std::io::Error::new(std::io::ErrorKind::InvalidData, e))?;
    Ok(cfg)
}

fn write_aln_file(path: &PathBuf, content: &str) -> Result<(), std::io::Error> {
    let mut file = File::create(path)?;
    file.write_all(content.as_bytes())?;
    file.flush()?;
    Ok(())
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
