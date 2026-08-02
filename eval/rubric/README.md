<!-- Path: Prometheus-Praxis/eval/rubric/README.md -->

# ppx-eval-rubric

`ppx-eval-rubric` is the canonical scoring and eligibility crate for Prometheus-Praxis. It defines:

- The seven scoring dimensions.
- Normalized score and profile types.
- Component and system evaluation traits.
- Phoenix-centric deployment thresholds.

This crate is intended to be reused across multiple evaluation layers (components, systems, scenarios) without embedding any city-specific logic.

## Scoring primitives

### `Score`

A simple numeric wrapper:

- Range: `0.0` (unacceptable) to `1.0` (excellent).
- Methods:
  - `clamp()` ensures the value stays in `0.0..1.0`.
  - `is_min(min: f32)` checks if the score meets a minimum threshold.

### `Dimension`

The seven scoring dimensions:

- `KnowledgeFactor`
- `EcoImpact`
- `RiskOfHarm`
- `Robustness`
- `Sovereignty`
- `EnergyEfficiency`
- `GovernanceAlignment`

These are used to label rows and columns in evaluation matrices.

### `SevenDimProfile`

A full per-module or per-system profile:

- `knowledge_factor: Score`
- `eco_impact: Score`
- `risk_of_harm: Score`
- `robustness: Score`
- `sovereignty: Score`
- `energy_efficiency: Score`
- `governance_alignment: Score`

Helpers:

- `min_score()` returns the minimum score across all dimensions.
- `satisfies_threshold(per_dim_min)` checks that all dimensions meet a required minimum.

## Evaluation traits

### `ComponentEvaluable`

Implemented by any module that can be evaluated on the seven dimensions:

```rust
pub trait ComponentEvaluable {
    fn id(&self) -> &'static str;
    fn evaluate_component(&self) -> SevenDimProfile;
}
```

Examples include the advection kernel, MARL architecture, and streaming pipeline.

### `SystemEvaluable`

Implemented by integrated stacks (e.g., city-level deployments):

```rust
pub trait SystemEvaluable<C: ComponentEvaluable> {
    fn evaluate_system(&self, components: &[C]) -> SystemEligibility;
}
```

Where `SystemEligibility` contains:

- `profile: SevenDimProfile`
- `eligible: bool`
- `notes: String` (human-readable explanation of eligibility and violations)

## Phoenix thresholds

`PhoenixThresholds` encodes conservative deployment thresholds:

- `component_min`: minimum per-dimension score for each component.
- `system_min`: minimum per-dimension score for the integrated stack.
- `max_risk_of_harm`: strict upper bound on risk-of-harm.

```rust
let thresholds = PhoenixThresholds::default();
```

Governance can override these values explicitly but they are not hidden or implicit.

## Integration pattern

Other crates (e.g., `ppx-eval-components`, `ppx-eval-cli`) typically:

1. Define component structs bound to city contexts.
2. Implement `ComponentEvaluable` using domain-specific metrics.
3. Define system-level structs and implement `SystemEvaluable`.
4. Use `PhoenixThresholds` (or scenario-specific thresholds) to gate deployment.

This keeps the rubric as a sovereign, reusable instrument for eco-restoration, sovereignty, and safety scoring in Prometheus-Praxis.
