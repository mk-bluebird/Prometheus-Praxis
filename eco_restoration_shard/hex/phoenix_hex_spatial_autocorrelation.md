---
title: "Phoenix Hex-Anchor Canopy-Temperature Residual Autocorrelation"
description: "Conceptual data-science wiring for variogram estimation and Gaussian Process correlation length selection for eco-restoration need interpolation."
version: "1.0.0"
city: "Phoenix, AZ"
---

# Spatial Autocorrelation and Correlation Length

We consider canopy-temperature residuals per H3 hex-cell in Phoenix:

- Residuals `r(h)` are defined as observed canopy temperature minus the value predicted by a baseline model (e.g., from LST, albedo, and canopy cover).
- Spatial autocorrelation decays with distance between hex-cell centers due to heterogeneous land use and microclimate.

## Variogram Estimation from Satellite Data

Given satellite-derived canopy-temperature residuals for hex-cells:

1. Compute empirical semi-variogram:
   - For pairs of hexes `(h_i, h_j)` at separation distance `d_ij`, compute:
     - `γ_emp(d) = (1 / (2 N(d))) Σ (r(h_i) - r(h_j))^2` over all pairs in distance bin `d`.
   - Use Phoenix-specific LST/canopy datasets to estimate `γ_emp(d)` up to several kilometers.

2. Fit a variogram model:
   - Common choice: exponential or spherical model:
     - `γ(d) = c0 + c * (1 - exp(-d / ℓ))` (exponential).
   - The correlation length `ℓ` is the distance where the variogram approaches its sill (most of the spatial correlation has decayed).

Studies of Phoenix SUHI and green space patterns suggest correlation lengths on the order of neighborhood scales (hundreds of meters to a few kilometers), reflecting localized cooling from parks and canopy interventions.

## Gaussian Process Interpolation of Restoration Need

For a Gaussian Process (GP) interpolation of eco-restoration need across hex-cells:

- Use a covariance function with correlation length `ℓ` derived from the fitted variogram:
  - `k(d) = σ^2 exp(-d / ℓ)`.

- The chosen `ℓ` directly impacts:
  - **Kriging variance** of intervention recommendations: larger `ℓ` smooths over broader spatial scales, reducing variance in data-rich regions but potentially over-smoothing fine-scale needs; smaller `ℓ` increases variance in sparsely sampled areas and respects sharp spatial gradients.
  - **Actionability**: an `ℓ` that matches Phoenix neighborhood scales ensures clusters of hex-cells are treated coherently, aligning with community-led restoration projects and heat-action planning.

Selecting `ℓ` via variogram fit ensures that GP-based restoration-need interpolation faithfully reflects observed spatial autocorrelation of canopy-temperature residuals, balancing local detail against robust, low-variance recommendations for cyboquatic machinery allocation.
