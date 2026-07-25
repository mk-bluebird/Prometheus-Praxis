<!-- Filename: crates/ker_hex_interpolation/README.md -->
<!-- Destination: Prometheus-Praxis/crates/ker_hex_interpolation/README.md -->

# ker_hex_interpolation

Hex-anchored KER interpolation crate for Prometheus-Praxis, providing a constrained ordinary kriging kernel over scalar KER scores and reconstructing KER triads for unmonitored hexes.

## Overview

- Interpolates scalar KER scores s = k * e - r for unmonitored hex anchors using neighbouring hex samples.
- Enforces nonnegative kriging weights that sum to one via simplex projection.
- Preserves positivity of s when all neighbours have positive KER and keeps triads consistent with scalar KER.
- Designed as a non-actuating numerical kernel callable from governance and diagnostics crates.

## Data model

- `KerScore`:
  - KER triad with k, e, r in [0, 1].
  - `scalar()` returns s = k * e - r.
  - `from_scalar_with_means(s, k_mean, e_mean)` reconstructs k, e, r from scalar s and local means.

- `HexCoord`:
  - 2D coordinate of a hex anchor (x, y) in metres.

- `HexKerSample`:
  - Sample at a hex with `coord` and `ker`.

- `VariogramModel`:
  - Isotropic variogram gamma(h) with nugget, sill, range.
  - Used to build the ordinary kriging system.

- `KrigingResult`:
  - Interpolated scalar KER and triad at the target hex.
  - The nonnegative weights assigned to neighbours.

## Core functions

- `constrained_ordinary_kriging(neighbours, target_coord, variogram)`:
  - Builds the kriging system for scalar KER.
  - Solves the linear system and projects unconstrained weights into the probability simplex.
  - Computes interpolated scalar KER and triad at the target hex.
  - Returns `None` for degenerate configurations (e.g. ill-conditioned systems).

- Internal helpers:
  - `solve_linear_system(a, b, n)`:
    - Gaussian elimination with partial pivoting for n x n systems.
  - `project_to_simplex(lambda)`:
    - Euclidean projection onto lambda_i >= 0, sum lambda_i = 1.

## SQLite and ALN integration

- The crate is paired with:
  - `sql/ker_hex_interpolation_schema.sql`:
    - Tables `hex_ker_sample` and `hex_ker_interpolated`.
    - Trigger enforcing scalar KER consistency and positivity.
  - `aln/ker_hex_interpolation.aln2`:
    - ALN particles `ker.hex.sample` and `ker.hex.interpolated`.
    - `ker.hex.interpolated.accept` rule hex-stamping interpolations.

## Usage

1. Populate `hex_ker_sample` with neighbour KER samples per hex, including coordinates and triads.
2. For each target hex, query neighbours and construct `HexKerSample` inputs.
3. Call `constrained_ordinary_kriging` with an appropriate `VariogramModel`.
4. Persist the resulting `KrigingResult` to `hex_ker_interpolated` and hex-stamp via ALN.

## Safety and constraints

- `#![forbid(unsafe_code)]` ensures no unsafe Rust.
- The crate is non-actuating and performs only numeric interpolation.
- KER triads and scalar KER remain in [0, 1] and are checked by SQL triggers and ALN rules.
