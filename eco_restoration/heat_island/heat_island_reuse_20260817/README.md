# PHX-HEAT-ISLAND-REUSE-20260817

Non-actuating heat-island recovery and AI heat-reuse screening artifacts.

Files:
- cpp/surface_cooling_screen.cpp
- kotlin/ai_heat_reuse_screen.kt
- sql/heat_island_landcover_heat_reuse.sql

Surface-cooling screening equation:
  delta_T = (delta_absorbed_radiation - latent_heat_flux) / h

where:
  delta_absorbed_radiation = delta(1 - albedo) * net_radiation_w_m2

The program receives a signed change in absorbed radiation rather than inferring
the sign from ambiguous notation. For a reflective treatment, the expected value
is normally negative because less solar energy is absorbed. Vegetation cooling is
represented by a positive latent-heat-flux removal term. The calculated result is
a local screening indicator only; it is not a microclimate forecast.

Cost effectiveness:
  cooling_per_cost = max(0, -delta_T) / lifecycle_cost

The highest cooling_per_cost candidate is selected only if:
- expected local cooling is positive;
- the candidate has non-negative water and habitat suitability scores;
- all values are supplied with compatible area, time, and cost boundaries.

Heat reuse:
  Q_available_J = P_IT_W * PUE * t_s

This quantity estimates facility energy over the observation period, not all
recoverable thermal energy. The Kotlin program separately applies a supplied
recoverable-heat fraction and a sink-temperature guard.

Heat-sink policy:
- GREENHOUSE and SEED_GERMINATION: eligible only within site-defined temperature
  and demand limits.
- COMPOST: eligible only within site-defined compost temperature limits.
- CANAL_WATER: rejected by default because direct warming can harm freshwater
  habitat and water quality; no automated heat allocation is allowed.

All results require local measurement, thermal design, ecological review, water
budget review, and qualified approval before physical deployment.
