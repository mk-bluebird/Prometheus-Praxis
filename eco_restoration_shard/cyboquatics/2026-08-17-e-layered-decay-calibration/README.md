# PHX-LAYERED-DECAY-CALIBRATION-20260817

Non-actuating domain (e) artifact set for deterministic multi-layer stormwater
treatment accounting and recursive calibration of BOD/TSS/CEC decay constants.

Files:
- sql/multilayer_drainagedecay_calibration.sql
- java/DecayConstantRls.java
- lua/decay_constant_rls.lua
- aln/multilayer_drainagedecay_calibration_20260817.aln2

Layered outlet model:
  C_out = C_in * product_i(1 - R_i)

where each R_i is a documented removal fraction for a physical treatment layer:
  0 <= R_i <= 1.

The SQL view calculates the product deterministically from an ordered layer stack
using:
  exp(sum(log(1 - R_i)))

For R_i = 1, the outlet concentration is exactly zero. For all other layers,
the logarithmic product is stable and equivalent to direct multiplication.

Online calibration:
  ln(C_t / C_0) = -k_eff * t

For a fixed reference concentration C_0, define:
  phi_t = -t
  y_t = ln(C_t / C_0)

Then a scalar exponentially weighted RLS update estimates k_eff:
  gain_t = P_t * phi_t / (lambda + phi_t^2 * P_t)
  k_(t+1) = k_t + gain_t * (y_t - phi_t * k_t)
  P_(t+1) = (P_t - gain_t * phi_t * P_t) / lambda

The implementation adds required limits:
- lambda in (0, 1]
- P is clipped to configured positive bounds
- concentration ratios must be positive
- flow is retained as an explicit covariate and drift indicator, but is not
  silently converted into a decay-rate correction without a hydraulically
  validated transport model.

The fitted output is an effective decay constant under supplied conditions.
Non-stationary flow, changing residence time, dilution, loading, and sensor drift
can make an apparent decay parameter non-transferable across canal conditions.
