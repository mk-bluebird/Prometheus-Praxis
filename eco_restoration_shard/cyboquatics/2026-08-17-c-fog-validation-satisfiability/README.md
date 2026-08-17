# PHX-FOG-VALIDATION-SAT-20260817

Non-actuating domain (c) artifact set for validating and applying FOG predicates
for unmodeled oil/water/sediment media.

Files:
- sql/fog_cross_validation.sql
- kotlin/FogCrossValidation.kt
- lua/fog_minimal_rule.lua
- aln/fog_validation_satisfiability_20260817.aln2

Cross-validation definition:
  TP = predicted FOG and reference-confirmed hazardous mixed media
  FP = predicted FOG and reference-confirmed non-hazardous media
  FN = predicted non-FOG and reference-confirmed hazardous mixed media

  precision = TP / (TP + FP)
  recall    = TP / (TP + FN)
  F1        = 2TP / (2TP + FP + FN)

SQLite implementation:
- Uses a generated prediction column and a composite covering index on
  (fold_id, predicted_fog, reference_hazard, sample_id).
- The index enables fold-specific confusion-matrix aggregation with indexed
  predicates. Query planning remains deployment- and SQLite-version-specific;
  run EXPLAIN QUERY PLAN against production-scale data before claiming a query
  avoids every full scan.

Minimal safe rule:
  P = AND_j(x_j >= tau_j)

The Lua rule evaluator:
- Accepts one threshold set and values in the same positional order.
- Fails closed when any required criterion is absent, non-numeric, or below its
  threshold.
- Produces an ordered explanation of the failed criteria.
- Does not claim that a logical conjunction replaces laboratory confirmation,
  field context, calibration, or a regulatory decision.

Use only validated reference labels for cross-validation. Threshold selection and
test folds must be temporally or spatially separated where data dependence would
otherwise inflate reported performance.
