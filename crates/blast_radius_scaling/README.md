<!-- Filename: crates/blast_radius_scaling/README.md -->
<!-- Destination: Prometheus-Praxis/crates/blast_radius_scaling/README.md -->

# blast_radius_scaling

Blast-radius scaling law crate for Prometheus-Praxis, computing dimensionless similarity variables and fitting a universal master curve relating dimensionless energy to dimensionless blast radius.

## Overview

- Computes dimensionless energy E* from surcharge head, canal velocity, and FOG confinement geometry.
- Computes dimensionless blast radius R* by normalizing overtopping and scour radii using a hydraulic length scale.
- Fits a power-law master curve R* = a * (E*)^beta on log-log data for corridor analysis and eco-restoration design.

## Data model

- `BlastInput`:
  - Physical inputs: fluid density, gravity, surcharge head, channel width, depth, length, velocity, fog confinement factor.

- `BlastOutputs`:
  - Diagnostic outputs: overtopping radius, scour radius.

- `CorridorScales`:
  - Reference scales: head_ref, width_ref, depth_ref, velocity_ref.
  - Configured per corridor and hex registry.

- `SimilarityVariables`:
  - E*, R* for overtopping, R* for scour.

## Core functions

- `compute_similarity_variables(input, outputs, scales, alpha_h, alpha_v, alpha_fog)`:
  - Computes:
    - E_H* from potential energy.
    - E_v* from kinetic energy.
    - E* = alpha_h E_H* + alpha_v E_v* + alpha_fog C*.
    - R* for overtopping and scour using L_c = sqrt(W * D).

- `fit_power_law_master_curve(samples)`:
  - Uses least squares on log-log data:
    - x = ln(E*), y = ln(R*).
  - Fits slope beta and intercept ln(a).
  - Returns (a, beta) for the master curve.

## SQLite and ALN integration

- Paired artifacts:
  - `sql/blast_radius_scaling_schema.sql`:
    - Table `blast_radius_event` for storing similarity variables per event.
    - Table `blast_radius_master_curve_fit` for storing fitted parameters per corridor.
  - `aln/blast_radius_scaling.aln2`:
    - Particle `blast.radius.event` for event-level data.
    - Particle `blast.radius.masterCurveFit` for curve fits and corridor metadata.

## Usage

1. Record raw blast-radius diagnostics for surcharge events in `blast_radius_event`.
2. Compute similarity variables via this crate and populate E*, R* fields.
3. Collect samples per corridor and call `fit_power_law_master_curve`.
4. Store fitted (a, beta) and sample counts in `blast_radius_master_curve_fit`.
5. Use the master curve to:
   - Predict blast radius thresholds for new designs.
   - Evaluate corridor safety margins under KER and Lyapunov constraints.

## Safety and constraints

- `#![forbid(unsafe_code)]` enforced.
- All inputs and outputs are bounded in the ALN specification to preserve physical realism.
- The crate is non-actuating and is intended for diagnostics, planning, and governance analytics only.
