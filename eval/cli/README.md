<!-- Path: Prometheus-Praxis/eval/cli/README.md -->

# ppx-eval-cli

`ppx-eval-cli` is a small command-line tool for Prometheus-Praxis that:

- Builds Phoenix-bound instances of the advection kernel, MARL architecture, and streaming pipeline.
- Prints a 3 × 7 ASCII matrix of component scores across the seven scoring dimensions.
- Evaluates the integrated Phoenix stack for “Eligible” deployment status and prints a summary.

It is designed as a low-friction way to inspect component-level and system-level behavior in a single run, consistent with Prometheus-Praxis governance rules.

## Scoring dimensions

The CLI uses the seven dimensions defined in `ppx-eval-rubric`:

- Knowledge-factor
- Eco-impact
- Risk-of-harm
- Robustness
- Sovereignty
- Energy efficiency
- Governance alignment

Each dimension is scored on a normalized range `0.0..1.0`, with conservative thresholds for Phoenix.

## What it does

On execution, `ppx-eval-cli`:

1. Constructs a default `PhoenixContext` (monsoon intensity, canyon heat, FOG density, industrial load, sovereignty weight, energy constraint).
2. Instantiates:
   - `AdvectionKernel`
   - `MarlArchitecture`
   - `StreamingPipeline`
3. Computes `SevenDimProfile` for each component and prints a table:

   ```text
   === Prometheus-Praxis Component Evaluation Matrix (Phoenix) ===

   Component            |     Know      Eco     Risk    Robust   Sovereign     Energy           Governance
   -------------------------------------------------------------------------------------------------------
   advection_kernel     |    0.960    0.785    0.090     0.889       0.946     0.862                0.941
   marl_architecture    |    0.870    0.882    0.114     0.850       0.940     0.827                0.930
   streaming_pipeline   |    0.845    0.832    0.132     0.868       0.940     0.860                0.904
   -------------------------------------------------------------------------------------------------------
   ```

4. Builds a `PhoenixStack` and calls `evaluate_system` to compute:

   - Integrated `SevenDimProfile`
   - Boolean `eligible`
   - Governance-readable `notes` (violations, risk flags, eligibility outcome)

5. Prints the eligibility summary and integrated profile.

## Usage

From the `Prometheus-Praxis/eval/cli` directory:

```bash
cargo run --bin ppx-eval-cli
```

This assumes:

- `ppx-eval-rubric` is available at `Prometheus-Praxis/eval/rubric`.
- `ppx-eval-components` is available at `Prometheus-Praxis/eval/components`.
- Both crates compile with Rust `1.85` and edition `2024`.

## Extending the CLI

You can extend `ppx-eval-cli` to:

- Read component parameters from JSON or ALN scenario files.
- Swap city contexts (Phoenix, Tucson, LasVegas, Albuquerque) by mapping ALN archetypes to `PhoenixContext`-like structs.
- Emit machine-readable reports (JSON, ALN) in addition to the ASCII matrix.

The current implementation keeps everything explicit and inspectable, in line with Prometheus-Praxis sovereignty rules.
