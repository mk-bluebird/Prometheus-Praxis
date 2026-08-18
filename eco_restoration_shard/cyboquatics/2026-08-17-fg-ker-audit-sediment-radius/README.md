# PHX-KER-AUDIT-SEDIMENT-RADIUS-20260817

Non-actuating domain (f/g) artifact set for append-only KER decision auditing and
conservative surcharge-breach radius screening that includes a sediment term.

Files:
- sql/ker_audit_and_sediment_radius.sql
- cpp/surcharge_sediment_radius.cpp
- aln/ker_audit_sediment_radius_20260817.aln2

KER audit:
- K, E, and R are constrained to [0, 1].
- KER score is K * (E - R), constrained to [-1, 1].
- Updates to a decision create an append-only audit event containing:
    delta_K = K_new - K_old
    delta_E = E_new - E_old
    delta_R = R_new - R_old
    delta_KER = KER_new - KER_old
- Decision deletions are rejected.
- Audit events cannot be updated or deleted.

Blast-radius screening:
  radius_m = c * sqrt(Q_m3_s * T_s) * (1 + alpha * bank_sensitivity)
             + beta * critical_shear_stress_pa

Inputs must be field-calibrated:
- Q is surcharge/breach discharge in m3/s.
- T is surcharge duration in seconds.
- bank sensitivity is a normalized site-specific vulnerability score [0, 1].
- critical shear stress is a sediment-property input in Pa.
- c, alpha, and beta are model coefficients with units selected so the output
  remains meters.

This is a conservative screening relationship, not a sediment-transport model or
a safe excavation/entry authorization. Verify hydraulic geometry, erosion,
sediment properties, receiving-water constraints, bank stability, and site
conditions with qualified engineering review.
