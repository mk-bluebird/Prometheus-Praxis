# VFD-Adapted Pump Power and Hex-Scale LST Forecasting

## Cyboquatic Pump Power with VFD Efficiency

- `cpp/simulation/cyboquatic_pump_power_vfd.cpp` refines the pump power model:
  - Base formula: `P = rho * g * Q * H / eta(Q)`, with `eta(Q)` given by a variable frequency drive (VFD) efficiency curve as a function of `Q/Q_rated`.
  - The VFD efficiency curve is modelled quadratically around a peak operating ratio, with bounds `[eta_min, 1]`.
- The code derives the partial derivative:
  - `energyreqJ(Q) = P(Q) * dt`.
  - `dP/dQ = A * [1/eta(Q) - (Q / eta(Q)^2) * d eta/dQ]`, where `A = rho g H`.
  - `d(energyreqJ)/dQ = dt * dP/dQ`.
- This derivative is computed explicitly in `PumpState.dEnergyreq_dFlow`, enabling real-time carbon-optimising control: controllers can modulate flow `Q` to minimise energy use and eco-impact while respecting hydraulic constraints.

## Hex-Scale LST Seq2Seq Forecasting (Java DL4J)

- `java/src/main/java/org/cyboquatic/forecast/HexLstSeq2Seq.java` defines a DL4J-based Seq2Seq-style recurrent model for forecasting hex-scale LST:
  - Input features per time step:
    - Telemetry: `energyreqJ`, `deltaVt_m_s`, `ker_e`, `ker_r`, PFAS, DO.
    - Meteorology: air temperature, humidity, wind speed, cloud cover.
    - Static: day-of-year (sin/cos), soil moisture, canopy cover.
  - Output: 24-hour-ahead LST (or residual) for each hex cell.
- Loss function:
  - Asymmetric loss `L = α * under^2 + β * over^2` penalises over-cooling (`y_pred > y_true`) more strongly, discouraging energy waste while maintaining adequate cooling.
- Sequence length:
  - For hourly data, a minimum sequence length of 48–72 time steps (2–3 days) is recommended to capture diurnal cycles and short-term dynamics necessary for reliable 24-hour forecasts.
  - The helper `recommendedSequenceLength(stepSeconds)` encodes this rule-of-thumb.

Technical justification: The VFD-adapted pump model provides closed-form gradients for energy requirement with respect to flow, enabling gradient-based, carbon-aware control in cyboquatic machinery. The Seq2Seq LST forecaster uses telemetry and meteorological features without invoking any digital-twin constructs, with an asymmetric loss that explicitly penalises over-cooling, improving eco-restoration efficiency in Phoenix hex cells.
