# ppx-governance-cli

`ppx-governance-cli` is the governance command-line interface for Prometheus-Praxis. It couples the technical evaluation of the Phoenix environmental stack (advection kernel, MARL architecture, streaming pipeline) to the ALN governance module `PhoenixEligibilityGate` and emits an ALN evidence block that encodes both:

- Seven-dimension system scores, and
- Five governance gates required for Phoenix “Eligible” status.[130]

The tool is designed to keep the **official deployment status** of any stack aligned with:

- Phoenix-specific performance evidence (e.g., DUSTIEAIM campaign, pilot programs).
- Documented safety case and adversarial stress tests.
- Data sovereignty and consent compliance (including Indigenous Data Sovereignty).
- Energy neutrality or renewable-only operation.
- Transparency and explainability consistent with NIST AI RMF and ISO 42001.[130]

## Inputs

`ppx-governance-cli` consumes:

1. Technical scores from Rust:
   - `SevenDimProfile` from `ppx-eval-rubric` for the integrated `PhoenixStack`.
   - Component-level profiles from `ppx-eval-components` (advection, MARL, streaming).[130]

2. Governance evidence from configuration and flags:
   - JSON config file:
     ```json
     {
       "domain_performance_ok": true,
       "safety_case_documented": false,
       "sovereignty_compliant": false,
       "energy_neutral_or_renew": false,
       "explainable_and_audited": false
     }
     ```
   - CLI overrides:
     - `--domain-ok`
     - `--safety-ok`
     - `--sovereignty-ok`
     - `--energy-ok`
     - `--explainability-ok`

Flags take precedence over config values, allowing you to progressively mark gates as satisfied as real evidence accumulates (validation reports, safety case documents, signed data-sharing agreements, energy analyses, explainability audits).[130]

## Outputs

The tool emits:

- An ALN `system` block, e.g.:

  ```aln
  system PhoenixIntegratedV1 {
    profile = SystemProfile {
      KnowledgeFactor     = 0.853000;
      EcoImpact           = 0.821000;
      RiskOfHarm          = 0.410000;
      Robustness          = 0.802000;
      Sovereignty         = 0.785000;
      EnergyEfficiency    = 0.794000;
      GovernanceAlignment = 0.812000;
    };

    evidence = SystemEvidence {
      profile                  = profile;
      domain_performance_ok    = true;
      safety_case_documented   = false;
      sovereignty_compliant    = false;
      energy_neutral_or_renew  = false;
      explainable_and_audited  = false;
    };

    thresholds = PhoenixEligibilityThresholds {
      component_min      = 0.750000;
      system_min         = 0.800000;
      max_risk_of_harm   = 0.250000;
      require_domain_performance     = true;
      require_safety_case            = true;
      require_sovereignty_compliance = true;
      require_energy_neutrality      = true;
      require_explainability         = true;
    };

    status = DecideStatus(evidence, thresholds);
  }
  ```

- The ALN file is written to `eval/governance/phx_eligibility_gate_instance.aln` by default, or to a custom path specified via `--output-aln`.

When `PhoenixEligibilityGate` is run over this evidence, it applies:

- Per-dimension minima and `max_risk_of_harm`, and
- The five governance invariants,

to return one of: `Eligible`, `NotEligible`, `Pilot`, or `Experimental`.

## Usage

From `Prometheus-Praxis/eval/governance/cli`:

```bash
# Initial run (no evidence, all gates default false).
cargo run --bin ppx-governance-cli

# Run with a governance config file.
cargo run --bin ppx-governance-cli -- --config phx_evidence.json

# Run with config plus overrides once specific gates are proven.
cargo run --bin ppx-governance-cli \
  -- \
  --config phx_evidence.json \
  --safety-ok \
  --sovereignty-ok \
  --energy-ok \
  --explainability-ok \
  --output-aln phx_eligibility_gate_instance.aln
```

## Relation to PhoenixEligibilityGate

- `ppx-governance-cli` is the Rust-side instrument that:
  - Computes the seven-dimension `SevenDimProfile` for the integrated Phoenix stack.
  - Assembles `SystemEvidence` from config and flags.
  - Calls `emit_aln_evidence` to generate ALN text compatible with `PhoenixEligibilityGate`.[130]

- `PhoenixEligibilityGate` (ALN) is the sovereign gate that:
  - Applies the five thresholds and per-dimension minima.
  - Decides the official deployment status for Phoenix.[130]

Together, they ensure that Prometheus-Praxis only labels a stack **Eligible** when both technical performance and governance evidence satisfy the thresholds defined in the Phoenix eligibility report.[130]
