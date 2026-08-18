# PHX-CANOPY-ET-SCHEDULE-20260817

Non-actuating heat-island recovery artifacts for canopy-cover screening and
evapotranspiration-limited irrigation planning.

Files:
- cpp/canopy_cover_threshold.cpp
- java/et_irrigation_scheduler.java
- sql/canopy_et_irrigation.sql

Canopy temperature model:
  T_s = (1 - f_v) * T_pavement + f_v * T_veg

Required canopy fraction to meet a target surface temperature:
  f_v_required = (T_pavement - T_target) / (T_pavement - T_veg)

The C++ tool:
- accepts pavement, vegetated-surface, and target temperatures;
- computes the unconstrained required fraction;
- reports IMPOSSIBLE when target temperature is lower than the modeled
  vegetated-surface temperature;
- reports ALREADY_MET when target is at or above pavement temperature;
- otherwise clamps the result to [0, 1] and reports required canopy coverage.

This linear mixture is a planning approximation. It does not model canopy
geometry, shade timing, wind, soil moisture, tree maturity, surface thermal
storage, radiation, or local pedestrian exposure.

Irrigation water budget:
  ETc_mm = Kc * ET0_mm
  plant_demand_L = ETc_mm * irrigated_area_m2
  usable_canal_supply_L = canal_allocation_L * conveyance_efficiency
  irrigation_request_L = min(plant_demand_L, usable_canal_supply_L)

The Java tool schedules only the available demand, applies a site-defined
soil-moisture deficit fraction, and reports unmet demand rather than using water
beyond the allocation. It excludes negative inputs and requires all rates and
efficiencies to be explicit.

These artifacts do not actuate water infrastructure, pump canal water, or
authorize diversion. Use only with local water-rights, ecological, irrigation,
and public-health review.
