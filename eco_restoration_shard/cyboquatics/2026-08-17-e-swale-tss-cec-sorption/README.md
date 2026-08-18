# PHX-SWALE-TSS-CEC-20260817

Non-actuating domain (e) artifact set for vegetated-swale TSS screening and
Langmuir sorption fitting for biochar/sand stormwater media.

Files:
- cpp/swale_tss_and_langmuir_fit.cpp
- sql/swale_tss_cec_sorption.sql
- aln/swale_tss_cec_sorption_20260817.aln2

TSS model:
  E = 1 - exp(-kTSS * L / v)

Definitions:
- E is the screened fractional TSS removal efficiency, constrained to [0, 1].
- kTSS is an empirical first-order removal coefficient in 1/time.
- L is effective swale flow-path length in m.
- v is measured or hydraulically estimated mean velocity in m/time.

The regression uses an interpretable log transform where 0 < E < 1:
  -ln(1 - E) * v / L = kTSS

The fitted coefficient is modeled as:
  kTSS = beta0 + betaVeg * vegetationDensity - betaSlope * slopeFraction
         - betaLoad * hydraulicLoading

This is a correlation model fitted from supplied observations. It must not be used
as a physical design claim outside the observed design range or without erosion,
hydraulic, soil, and vegetation review.

Langmuir model:
  q = Qmax * K * C / (1 + K * C)

The C++ fitter uses a bounded two-dimensional grid search against equilibrium
concentration C and measured media loading q. Qmax and K remain data-dependent;
the tool does not invent Phoenix-wide sorption parameters.

All outputs are planning and laboratory-data screens. They do not control water,
apply media, or authorize field construction.
