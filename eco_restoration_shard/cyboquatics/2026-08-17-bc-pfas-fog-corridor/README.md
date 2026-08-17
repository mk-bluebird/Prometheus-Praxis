# PHX-PFAS-FOG-CORRIDOR-20260817

Non-actuating ecological-restoration artifact set for:

1. PFAS-cold residual-corridor local stability screening.
2. FOG routing for oil/water/sediment media without assuming the medium is modeled water.

Files:
- cpp/pfas_cold_jacobian.cpp
- lua/fog_router.lua
- kotlin/FogRouter.kt
- sql/pfas_bifurcation_and_fog_media.sql
- aln/pfas_cold_fog_corridor_20260817.aln2

PFAS-cold residual model:
  dC/dt = source - removalRate * C - partitionRate * C * S
  dS/dt = releaseRate * C - settlingRate * S

The C++ tool evaluates the 2x2 Jacobian:
  J = [ -removalRate - partitionRate*S, -partitionRate*C ]
      [  releaseRate,                 -settlingRate     ]

For a 2x2 continuous-time system, local contraction is screened by:
  trace(J) < 0
  determinant(J) > 0

The corridor loses contraction when the maximum real eigenvalue crosses zero:
  lambdaMax(J) >= 0

A result near zero is a review condition, not proof of a physical bifurcation.
A real PFAS fate model requires validated site-specific processes, measurements,
temperature dependencies, transport boundaries, and uncertainty analysis.

FOG routing:
- This tool accepts oil concentration, TSS, turbidity, and field-calibrated
  thresholds supplied by an authorized sampling plan.
- It does not embed generic regulatory or universal threshold values.
- When all three thresholds are exceeded, it classifies the sample as
  UNMODELED_MIXED_OIL_WATER_SEDIMENT and routes it for manual review.
- Unknown, absent, non-finite, or negative inputs are rejected.

The tools provide analytical screening only. They do not operate pumps, valves,
treatment equipment, chemical dosing, or external systems.
