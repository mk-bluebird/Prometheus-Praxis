# PHX-ENERGY-DELTAVT-20260817

Non-actuating energy and impulse screening tools for low-impact canal dredging and
native planting machinery.

Files:
- cpp/cyboquatic_energyreq.cpp
- java/DeltaVtIntegrator.java
- sql/cyboquatic_energy_deltavt.sql
- aln/cyboquatic_energy_deltavt_20260817.aln2

Energy model:
  mechanical_work_J = force_N * distance_m
  energyreqJ = mechanical_work_J / drivetrain_efficiency

Where a force is not aligned with travel, supply the force component along motion.
The model avoids pretending that generic Phoenix values determine real field energy:
soil condition, moisture, excavation geometry, implement drag, grade, load, route,
and machinery efficiency must be measured or validated for each operating plan.

Ground-pressure screen:
  ground_pressure_Pa = normal_load_N / contact_area_m2
  allowable_pressure_Pa = allowable_bearing_pressure_Pa * (1 - bank_sensitivity)

Operation is rejected when:
- measured ground pressure exceeds the sensitivity-adjusted allowable pressure;
- bank sensitivity is outside [0, 1];
- required input values are absent or non-positive; or
- energy demand exceeds the configured low-impact plan energy limit.

Delta-Vt:
  delta_vt = integral(a(t) dt)

The Java tool uses trapezoidal integration over ordered acceleration observations.
It treats the supplied 10000 limit as a hard governance/invariant ceiling, not as
an independently validated machinery safety threshold. A site engineer must set a
much lower equipment-specific operating limit based on equipment, grade, traction,
bank geometry, geotechnical conditions, and permitting constraints.
