# Canal Node KER Parameters and Time-Varying BOD Decay

## Minimal CanalNode Parameters for Carbon-Negative KER

The ALN v2 shard `aln_v2/canal_node_ker_carbon_negative.aln` defines the minimal parameter set for a `CanalNode` needed to pose a formal probabilistic invariant:

- Hydraulic bounds: `capacity_m3_s`, `baseline_flow_m3_s`.
- Stochastic inflow: `inflow_ln_mu`, `inflow_ln_sigma` (log-normal flow).
- Net carbon flux per sample: `carbon_flux_mean_kg`, `carbon_flux_std_kg`.
- Telemetry cadence: `sample_interval_s`, one-year `horizon_s`.
- Initial eco-impact: `ker_e_initial`.
- Required confidence: `ker_negative_confidence`.

The invariants `KERValidTelemetry` and `KerERecurrence` ensure that any telemetry sequence respectful of these parameters reliably evolves `ker_e` as a cumulative random-walk process. The probabilistic property:

> P ≥ ker_negative_confidence [ ker_e_N ≤ 0 ]

for N samples over one year is then discharged by a probabilistic model checker, guaranteeing that KER-valid telemetry remains carbon-negative with specified confidence.

## Time-Varying BOD Decay with SQL Wiring

The SQL shard `sql/bod_decay_time_varying_temperature.sql` encodes first-order BOD decay with temperature-dependent rate:

- Continuous model:
  - `dBOD/dt = -k(T(t)) * BOD(t)`
  - `k(T) = k20 * θ^(T - 20)`.

- Analytical solution at downstream sensor after plug-flow travel time τ:
  - `BOD(τ) = BOD0 * exp(- ∫_0^τ k20 * θ^(T(s) - 20) ds)`.

The SQL wiring uses:

- `canal_telemetry_upstream` for upstream BOD and temperature samples.
- `canal_decay_config` for `k20`, `θ`, and `travel_time_s`.
- A helper view `canal_decay_kernel` to compute `k(T)` and sample-wise Δt.
- A downstream view `canal_bod_downstream` that approximates the integral via a SUM over `k_t_per_s * delta_t_s` in the Lagrangian window `[t_up, t_up + τ]`, then computes downstream BOD using the exponential solution.

Technical justification: The ALN v2 canal_node object captures the minimal stochastic and KER parameters required for probabilistic verification of long-horizon carbon-negative behavior, while the SQL formulation of temperature-dependent BOD decay translates the integral solution over a Lagrangian temperature history into windowed sums over telemetry, enabling practical downstream BOD prediction and eco-restoration planning without external simulators.
