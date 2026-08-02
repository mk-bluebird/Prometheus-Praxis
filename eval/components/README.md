<!-- Path: Prometheus-Praxis/eval/components/README.md -->

# ppx-eval-components

`ppx-eval-components` is the component-level evaluation crate for Prometheus-Praxis. It provides Rust types and traits to score three core modeling modules:

- Advection kernel
- MARL architecture
- Streaming pipeline

Each module is evaluated against the seven scoring dimensions defined in the rubric crate:

- Knowledge-factor
- Eco-impact
- Risk-of-harm
- Robustness
- Sovereignty
- Energy efficiency
- Governance alignment

The crate is designed for **Phase A** component validation and **Phase B** integrated Phoenix eligibility, consistent with Cybercore and Prometheus-Praxis rules.

## Design goals

- Treat component-level validation as primary: each module must satisfy per-dimension thresholds before system-level eligibility is considered.
- Preserve sovereignty and consent corridors: no hidden control panels, downgrades, or rollbacks.
- Align with eco-restoration and energy efficiency for Phoenix and compatible arid-city archetypes.

## Structure

- `PhoenixContext` captures city-specific parameters (monsoon intensity, canyon heat, FOG density, industrial load, sovereignty weight, energy constraints).
- `AdvectionKernel`, `MarlArchitecture`, and `StreamingPipeline` implement `ComponentEvaluable` from the rubric crate.
- `PhoenixStack` implements `SystemEvaluable`, aggregating component profiles and enforcing Phoenix deployment thresholds.

## Basic usage

```rust
use ppx_eval_components::{
    AdvectionKernel, MarlArchitecture, StreamingPipeline, PhoenixContext, PhoenixStack,
};
use ppx_eval_rubric::PhoenixThresholds;

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

    let thresholds = PhoenixThresholds::default();
    let stack = PhoenixStack::new(adv.clone(), marl.clone(), stream.clone(), thresholds);

    // Phase A: component matrix
    let matrix = ppx_eval_components::phoenix_component_matrix(&adv, &marl, &stream);
    for (id, profile) in matrix.iter() {
        println!("Component: {}", id);
        println!("  knowledge_factor     = {}", profile.knowledge_factor.0);
        println!("  eco_impact           = {}", profile.eco_impact.0);
        println!("  risk_of_harm         = {}", profile.risk_of_harm.0);
        println!("  robustness           = {}", profile.robustness.0);
        println!("  sovereignty          = {}", profile.sovereignty.0);
        println!("  energy_efficiency    = {}", profile.energy_efficiency.0);
        println!("  governance_alignment = {}", profile.governance_alignment.0);
    }

    // Phase B: integrated eligibility
    let components: Vec<Box<dyn ppx_eval_rubric::ComponentEvaluable>> =
        vec![Box::new(adv), Box::new(marl), Box::new(stream)];
    let eligibility = stack.evaluate_system(&components);
    println!("Eligible: {}", eligibility.eligible);
    println!("Notes:\n{}", eligibility.notes);
}
```

## Integration

This crate assumes the presence of `ppx-eval-rubric` in `Prometheus-Praxis/eval/rubric`, which defines:

- `Score`
- `SevenDimProfile`
- `ComponentEvaluable`
- `SystemEvaluable`
- `PhoenixThresholds`

Use this crate as the component-focused layer in your evaluation pipeline, and bind system-level decisions to Cybercore governance modules and ALN scenario shards for Phoenix and other arid cities.
