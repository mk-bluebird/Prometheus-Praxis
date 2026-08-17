# PHX-BIO-IDENT-20260817

This artifact set supports two non-actuating ecological-restoration analyses:

1. ISO 14851-style BOD curve identifiability for:
   BOD(t) = BODu * (1 - exp(-k * t))

2. OECD TG 201-inspired algal growth screening with a time-varying exposure:
   mu(t) = muMax * S / (Ks + S) * max(0, 1 - C(t) / EC50)

Files:
- cpp/bod_identifiability.cpp
- java/TimeVaryingAlgalGrowth.java
- kotlin/TimeVaryingAlgalGrowth.kt
- lua/bod_identifiability.lua
- sql/biodegradation_and_algal_exposure.sql
- aln/biodegradation_and_algal_exposure.aln2

BOD identifiability:
- With known BODu and at least one measurement at t > 0 where 0 < BOD(t) < BODu,
  k is structurally identifiable:
  k = -ln(1 - BOD(t)/BODu) / t.
- With both BODu and k unknown, one point cannot identify both parameters.
- Two or more distinct positive-time measurements can provide local identifiability only when
  the sensitivity Jacobian has numerical rank 2.
- A rank-2 Jacobian alone does not prove unbiased estimation. Noise, early-time-only samples,
  plateau-only samples, poorly known BODu, temperature variation, blanks, and nitrification
  can make the estimate practically unstable or biased.

Algal EC50 interpretation:
- Monotonicity of C(t) does not preserve conventional constant-exposure EC50 interpretation.
- The integrated effect is driven by the complete concentration-time path and the nonlinear
  clipping at zero growth. Report EC50 as a model input and report the exposure trajectory,
  integration interval, growth integral, and normalized inhibition separately.

Both models are screening and data-quality tools, not regulatory test execution or field actuation.
