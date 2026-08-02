# ppx-eval-scenarios

`ppx-eval-scenarios` is the scenario-level evaluation crate for Prometheus-Praxis. It couples:

- City-specific configuration (Phoenix, Tucson, Las Vegas, Albuquerque), and
- The seven-dimension evaluation rubric,

to the ALN module `AridCityTransferability` and to the component-level evaluation crate `ppx-eval-components`.

The goal is to make it easy to:

- Instantiate `PhoenixContext`-compatible configurations for multiple arid cities.
- Generate scenario-specific `SevenDimProfile` summaries.
- Emit JSON/ALN fragments that can be consumed by higher-level governance tools.

> **Note:** This crate is wired for scenario logic and configuration emission. Do not run `cargo` commands or install new tools as part of this workflow; use existing tooling only. Any execution references here are conceptual (\"when the crate is built and executed in an appropriate environment\").

---

## Relation to `AridCityTransferability` (ALN)

The ALN file:

```text
Path: Prometheus-Praxis/eval/scenarios/arid_city_transfer_aln.aln
module AridCityTransferability { ... }
```

defines:

- Four `city` entries: `Phoenix`, `Tucson`, `LasVegas`, `Albuquerque`.
- Two invariants:
  - `CityAgnosticMechanisms` (PDE/MARL/Streaming mechanisms invariant across cities).
  - `CityBoundParameters` (boundary conditions, waste streams, governance corridors).
- Four `scenario` entries:
  - `PhoenixPrimary` – primary deployment evaluation.
  - `TucsonDiagnostic`, `LasVegasDiagnostic`, `AlbuquerqueDiagnostic` – transferability diagnostics.

`ppx-eval-scenarios` mirrors these definitions in Rust via:

- `CityConfig` and `ScenarioConfig` structs, and
- Helper functions that map `CityConfig` into `PhoenixContext` and assemble `SevenDimProfile` rows using `ppx-eval-components` and `ppx-eval-rubric`.

---

## Core Concepts

- **City config:** A compact struct that encodes:
  - `archetype` (e.g., `"arid_urban_canyon"`),
  - Monsoon and canyon indices,
  - FOG channel density,
  - Industrial waste load,
  - Sovereignty weight, and
  - Energy constraint.

- **Scenario config:** A struct that binds:
  - A city identifier,
  - A scenario role (primary deployment vs transferability diagnostic),
  - The active scoring dimensions.

- **Scenario evaluation:** When this crate is built and used in an appropriate environment, it can:

  - Construct a `PhoenixContext` from a `CityConfig`.
  - Instantiate component templates (`AdvectionKernel`, `MarlArchitecture`, `StreamingPipeline`).
  - Produce a `SevenDimProfile` for each scenario and emit a JSON/ALN fragment for reporting.

---

## Example Conceptual Usage

When the crate is built and executed in an appropriate environment, a caller can:

```rust
use ppx_eval_scenarios::{
    CityId,
    ScenarioId,
    scenario_profile_for_city,
};

fn example() {
    let city = CityId::Phoenix;
    let scenario = ScenarioId::PhoenixPrimary;

    let profile = scenario_profile_for_city(city, scenario);

    // Profile can be logged, serialized to JSON, or embedded into ALN text
    // compatible with AridCityTransferability and PhoenixEligibilityGate.
}
```

This keeps scenario wiring aligned with the existing evaluation and governance stack without prescribing any particular tooling or command-line interface.

---

## Design Constraints

- Rust edition: 2024.
- rust-version: 1.85.
- License: MIT OR Apache-2.0.
- No unsafe code.
- No instructions to install tools or run `cargo` commands; use existing tooling only.
- Scenario definitions must remain consistent with `arid_city_transfer_aln.aln` and the seven-dimension rubric.
