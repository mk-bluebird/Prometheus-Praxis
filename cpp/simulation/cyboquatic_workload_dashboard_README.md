# C++ Workload Simulator → Kotlin TornadoFX Dashboard and Albedo Sensitivity

## C++ → Kotlin GUI Wiring Pattern

- `cpp/simulation/cyboquatic_workload_simulator.cpp`:
  - Simulates cyboquatic workload telemetry and writes batches of rows into a SQLite database (`cyboquatic_workload_dashboard.db`) using WAL mode.
  - Fields include `energyreq_j`, `delta_vt_m_s`, FOG route decision, and KER triad (`ker_k`, `ker_e`, `ker_r`).
- `kotlin/src/main/kotlin/org/cyboquatic/dashboard/WorkloadDashboardApp.kt`:
  - Implements a TornadoFX desktop GUI that:
    - Polls the database every second.
    - Plots `energyreqJ` vs `ΔVt` with points colour-coded by FOG route:
      - `PRIMARY_CANAL` → green, `SECONDARY_CANAL` → orange, `HOLD_TANK` → red.
    - Checks for any `ker_e > 0` rows and raises a visual alert (status label turns red with “VIOLATION”) if eco-impact violates carbon-negative constraints.
  - This wiring provides real-time visual feedback on cyboquatic workloads and their eco-restoration compliance.

## Albedo Measurement Accuracy and Green-Fraction Sensitivity

- `cpp/simulation/albedo_sensitivity_note.cpp` encodes a sensitivity analysis for albedo measurement accuracy:
  - Assumes a linear relation between LST drop and green_fraction increase:
    - `Δg = ΔT_target / k_T`, with `k_T` the LST sensitivity (K per unit green_fraction).
  - Maps Sentinel-2 albedo α to green_fraction g via:
    - `g = c_0 + c_1 α`, so the uncertainty in g is `σ_g = |c_1| σα`.
  - Requires the relative error `σ_g / Δg` to be less than 10%:
    - `σ_g / Δg = |c_1| k_T σα / ΔT_target < 0.10`.
  - Derives the albedo measurement tolerance:
    - `σα_max = 0.10 * ΔT_target / (|c_1| k_T)`.
- This formula provides an explicit accuracy requirement for Sentinel-2 albedo estimates: if the absolute albedo uncertainty is kept below `σα_max`, the computed green-fraction increase needed for a 1°C LST drop will have a relative error below 10%, even when propagated through the `deltaVt` model. This guides remote-sensing quality thresholds for reliable eco-restoration planning in Phoenix hex cells.

Technical justification: The C++/Kotlin wiring ensures a low-latency feedback loop from simulated cyboquatic telemetry to an operator-facing dashboard, with KER-based eco-impact alerts. The sensitivity analysis quantifies how Sentinel-2 albedo uncertainty affects the accuracy of green-fraction interventions for thermal mitigation, specifying a clear tolerance formula that can be enforced in data ingestion and calibration pipelines to keep eco-restoration decisions within acceptable error margins.
