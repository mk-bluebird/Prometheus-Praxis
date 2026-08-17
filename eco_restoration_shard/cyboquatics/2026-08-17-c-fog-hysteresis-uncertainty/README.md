# PHX-FOG-HYSTERESIS-UNCERTAINTY-20260817

Non-actuating FOG-router artifact set for mixed oil/water/sediment screening.

Files:
- kotlin/FogHysteresisRouter.kt
- java/FogPredicateUncertainty.java
- sql/fog_hysteresis_uncertainty.sql
- aln/fog_hysteresis_uncertainty_20260817.aln2

Hysteresis model:
  P(t+1) = P(t) unless |score(t+1) - score(t)| > delta

The artifact does not assert a universal optimal delta. The optimal value is
site- and policy-specific because it depends on observed score noise, transition
cost, missed-hazard cost, calibration uncertainty, and required response time.

A defensible calibration procedure is:
1. Record field-calibrated score time series and independently confirmed hazards.
2. Select a candidate delta grid from 0 through the allowed operating range.
3. Calculate switch count, delayed hazard detections, and false-clear outcomes.
4. Reject deltas that violate the required safety recall or maximum detection delay.
5. Choose the remaining delta with the lowest switching count.
6. Version, approve, and persist the selected threshold profile.

Uncertainty propagation:
- A Boolean predicate has no ordinary differentiable standard deviation at its
  switching boundary.
- This set uses a continuous normalized margin:
    m = min(oil/tauOil, tss/tauTss, turbidity/tauTurb) - 1
- P_fog is 1 exactly when m > 0.
- Under independent sensor noise, the active limiting component has:
    sigma_m = sigma_x / tau_x
  away from a tie. At ties or near a boundary, the tool emits REVIEW_REQUIRED.
- A normal-approximation confidence interval for margin is:
    m +/- z * sigma_m
- The three-way Boolean decision is certain only when the full interval lies
  wholly above or wholly below zero. Otherwise the sample is indeterminate and
  must be routed for manual field review.
