<!-- Path: cpp/tools/README.md -->

# Prometheus-Praxis C++ Evaluation CLI

This directory contains `ppx_eval_report_cli.cpp`, a self-contained C++17 program that:

- Builds Phoenix-bound instances of the advection kernel, MARL architecture, and streaming pipeline.
- Prints a comparative 3×7 ASCII matrix (components vs. seven scoring dimensions).
- Evaluates the integrated Phoenix stack for "Eligible" deployment status and prints the result.
- Exports the same data as **JSON** (`eval_report.json`) and **ALN** (`eval_report.aln`) for consumption by Cybercore and other tooling.

## Scoring dimensions

The seven dimensions are:

- KnowledgeFactor
- EcoImpact
- RiskOfHarm
- Robustness
- Sovereignty
- EnergyEfficiency
- GovernanceAlignment

Each dimension is scored on a normalized range `[0.0, 1.0]` with conservative Phoenix thresholds.

## Compilation

```bash
g++ -std=c++17 -O2 -o ppx_eval_report_cli ppx_eval_report_cli.cpp
```

No external libraries are required; the program uses only the C++ standard library.

## Running

```bash
./ppx_eval_report_cli
```

After execution you will see the ASCII matrix and eligibility summary on stdout, and find two new files:

- `eval_report.json` – machine-readable JSON record of component profiles and integrated eligibility.
- `eval_report.aln` – ALN-format envelope suitable for Prometheus-Praxis governance shards.

## Example output (truncated)

```
=== Prometheus-Praxis Component Evaluation Matrix (Phoenix) ===

Component            |     Know      Eco     Risk    Robust   Sovereign     Energy           Governance
-------------------------------------------------------------------------------------------------------
advection_kernel     |    0.960    0.785    0.090     0.889       0.946     0.862                0.941
marl_architecture    |    0.870    0.882    0.114     0.850       0.940     0.827                0.930
streaming_pipeline   |    0.845    0.832    0.132     0.868       0.940     0.860                0.904
-------------------------------------------------------------------------------------------------------

=== Phoenix Integrated Eligibility ===
Eligible: true
...
```

## Integration

The JSON and ALN files can be consumed by:

- Cybercore governance modules that enforce deployment gates.
- Post-hoc audit and continuous verification pipelines.
- Manual review by eco-restoration and sovereignty compliance teams.

The ALN report follows the same schema as `UrbanClimateModelEvaluation2026v1.aln` and can be parsed by existing ALN tooling.
