# Spectral analyzers in `src/engine/cpp`

This document describes the **spectral** helper functions and analyzers shipped with Prometheus‑Praxis. These kernels are non‑actuating, developer‑facing utilities for inspecting Lyapunov residuals and related diagnostics. They are intentionally treated as unknown / not directly controllable models: they surface patterns but never drive actuators or schedules on their own.

## Location and scope

- Directory: `src/engine/cpp/`
- Files (current examples):
  - `lyapunov_vt_spectral_analysis.cpp` – periodogram‑style analyzer over Vt time series.[file:3]
  - Other future spectral kernels SHOULD live in this directory and follow the same non‑actuating, diagnostic‑only pattern.

These functions are:

- Non‑harmful by design.
- Strictly numerical (no IO, no actuator bindings).
- Intended for offline or forensic analysis, or gated use inside higher‑level governance lanes.

## Design principles

- Unknown / not controllable:
  - Spectral outputs (frequency components, anomaly flags) are **signals**, not commands.
  - They must not be wired directly to pumps, gates, or other actuators.
  - Any use in production must pass through KER, Lyapunov, and ALN governance layers that can reject unsafe actions.[file:3]

- Non‑actuating:
  - Kernels operate on in‑memory arrays and return scalar or vector diagnostics.
  - No hardware control, network calls, or side effects beyond filling output structs.

- Eco‑aligned:
  - Spectral tools are meant to help detect anomalous patterns (e.g., gate malfunction, illegal discharge, cooling contagion drift) that could harm ecological stability.[file:3]
  - They support corridor design and auditing, but decisions remain in governance particles and human‑reviewed policies.

## `lyapunov_vt_spectral_analysis.cpp`

This module implements a periodogram‑style analyzer over Lyapunov residual time series \(V_t\).[file:3]

- Input:
  - `VtSpectralInput` POD with:
    - `vt_series`: array of \(V_t\) samples.
    - `freqs`: frequencies of interest (normalized 0..0.5).
    - `window_size`, `step_size`: sliding window parameters.
    - `phase`: monsoon phase (pre‑onset, active, withdrawal) to adapt thresholds.[file:3]
- Output:
  - `VtSpectralOutput` POD with:
    - `power[k]`: median window power at each frequency.
    - `background[k]`: monsoon‑aware background estimate.
    - `S[k]`: simple statistic `power / background`.
    - `anomaly[k]`: flag for potential malfunction or illegal discharge patterns.[file:3]

Key properties:

- Detrends each window before computing power.
- Uses median statistics for robustness.
- Adjusts background and anomaly thresholds by monsoon phase, acknowledging seasonal variability.[file:3]
- Returns only diagnostics; it does not change gates, pumps, or workloads.

## Developer usage

- Treat spectral functions as **helpers**:
  - Call them from Rust / Kotlin / Lua / Java telemetry layers to compute diagnostics.
  - Store outputs in telemetry tables or pass them into KER / Lyapunov guards as additional risk coordinates.[file:3]
- Never bypass governance:
  - If a spectral analyzer flags an anomaly, downstream logic must still respect:
    - KER thresholds.
    - Lyapunov non‑regression (no increase in \(V_t\)).
    - ALN v2 particle rules (e.g., autoban on repeated high risk).[file:3]

Recommended patterns:

- Use spectral outputs to:
  - Tag windows for deeper forensic review.
  - Inform corridor calibration (e.g., adjust risk weights when a frequency band consistently misbehaves).[file:3]
- Do NOT:
  - Implement closed‑loop control directly from spectral anomalies.
  - Use spectral metrics to weaken non‑offsettable planes (carbon, biodiversity, neurorights).

## Safety and discoverability

- This README should live close to the C++ kernels, e.g.:

  - `src/engine/cpp/README_spectral.md`  
  - referenced from the top‑level `README.md` or `docs/INDEX.md` so developers can find it quickly.[file:3]

- All future spectral functions MUST:
  - Be documented here.
  - Declare clearly that they are non‑actuating and not sufficient for control decisions.
  - Align with Prometheus‑Praxis ecosafety and KER discipline.

By keeping spectral analyzers strictly diagnostic, we ensure they remain non‑harmful tools for understanding complex canal and heatisland dynamics, while governance and corridor design remain the only sources of actuation.
