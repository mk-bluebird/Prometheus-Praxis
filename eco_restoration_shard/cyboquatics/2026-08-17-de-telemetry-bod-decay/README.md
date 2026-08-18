# PHX-TELEMETRY-BOD-DECAY-20260817

Non-actuating domain (d/e) artifact set for machinery telemetry invariants and
temperature-corrected drainagedecay BOD fitting.

Files:
- kotlin/TemperatureCorrectedBodFit.kt
- java/TemperatureCorrectedBodFit.java
- sql/cyboquatic_telemetry_bod_decay.sql
- aln/cyboquatic_telemetry_bod_decay_20260817.aln2

Telemetry invariants:
  0 <= energyreqJ <= 1,000,000,000
  0 <= deltaVt <= 10,000

BOD remaining-demand model:
  BOD(t) = BOD0 * exp(-k20 * theta^(T - 20) * t)

where:
- BOD0 is the modeled initial BOD concentration in mg/L.
- k20 is the first-order decay constant at 20 C in 1/day.
- theta is a unitless temperature correction factor.
- T is water temperature in C.
- t is elapsed time in days.

The fitters use grid search over declared k20 and theta bounds. This is a
deterministic screening fit, not experimental proof of Phoenix-wide kinetics.

For estimating both k20 and theta, the input must include temperature variation.
With a single temperature, only the composite effective rate:
  k_eff = k20 * theta^(T - 20)
is identified. Multiple temperature levels and time points are required to
separate k20 from theta without relying only on prior assumptions.

The SQL schema preserves temperature, flow, BOD, KER, and FOG context. Flow is
stored for mass-loading analysis but is not falsely inserted into the concentration
decay equation without a declared hydraulic transport model.
