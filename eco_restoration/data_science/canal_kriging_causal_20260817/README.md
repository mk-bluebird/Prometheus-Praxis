# PHX-CANAL-KRIGING-CAUSAL-20260817

Non-actuating data-science artifact set for canal sensor-gap screening and
observational restoration-effect estimation.

Files:
- cpp/ordinary_kriging.cpp
- java/OrdinaryKriging.java
- kotlin/RestorationAteMatching.kt
- sql/canal_kriging_causal.sql

Ordinary kriging:
  Z*(s0) = sum_i(lambda_i * Z(si))
  sum_i(lambda_i) = 1

The C++ and Java tools implement ordinary kriging for a supplied isotropic
exponential semivariogram:
  gamma(h) = nugget + sill * (1 - exp(-h / range)), for h > 0
  gamma(0) = 0

The ordinary-kriging system is:
  [ Gamma   1 ] [ lambda ] = [ gamma0 ]
  [  1^T    0 ] [   mu    ]   [   1    ]

The weights are unbiased with respect to an unknown constant mean because the
solver enforces sum(lambda)=1. This does not prove that a missing-data estimate
is unbiased in the statistical sense: unbiasedness additionally depends on
valid stationarity, an appropriate variogram, appropriate observation support,
no unmodeled drift, and representative sensor placement.

The tools report:
- estimated value;
- kriging variance;
- sum of weights;
- leave-one-out RMSE for the supplied variogram parameters.

Use leave-one-out error, residual spatial correlation, temporal stratification,
and field process knowledge to compare candidate variograms. Do not select a
variogram merely because it produces an estimate.

Causal average treatment effect:
  ATE = E[Y(1) - Y(0)]

The Kotlin matcher estimates an ATT-style matched contrast over treated records:
- exact matching by user-supplied stratum;
- nearest-neighbor matching within a declared standardized covariate-distance
  caliper;
- one matched control per treated record;
- no replacement of control records;
- unmatched treated records are reported, not silently discarded.

This is observational association after measured-confounder adjustment. It does
not identify a causal ATE without defensible assumptions including consistency,
positivity/overlap, no unmeasured confounding conditional on the supplied
covariates, and no interference across canal reaches.
