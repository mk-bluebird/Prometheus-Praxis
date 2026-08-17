# PHX-QPU-RESIDUAL-RLS-20260817

Non-actuating domain (b) artifact set for canal water-quality sensor fusion and
adaptive residual-corridor calibration.

Files:
- cpp/canal_kalman_filter.cpp
- java/CanalKalmanFilter.java
- kotlin/BoundedRlsCorridor.kt
- lua/bounded_rls_corridor.lua
- sql/canal_sensor_residual_corridor.sql
- aln/qpudatashard_residual_corridor_20260817.aln2

Kalman update with an available scalar observation:
prediction:
  x_prior = x_previous
  P_prior = P_previous + Q

innovation:
  r = z - h(x_prior)

linear scalar measurement h(x) = Hx:
  S = H * P_prior * H + R
  K = P_prior * H / S
  x_posterior = x_prior + K * r
  P_posterior = (1 - K * H) * P_prior

Sensor dropout:
- Do not synthesize a residual or compute a gain.
- Propagate state and covariance only: x = x_prior, P = P_prior.
- A bounded expected covariance under random arrivals is not guaranteed by
  ordinary Kalman equations alone; it depends on system dynamics, detectability,
  stabilizability, noise, and a sufficient observation-arrival process.

Residual whiteness:
- For available, correctly modeled measurements, normalized innovations
  nu_t = r_t / sqrt(S_t) should have near-zero mean and near-zero autocorrelation
  at non-zero lags.
- Dropout intervals are excluded from lagged-whiteness pairs. A failed whiteness
  check identifies model, covariance, timing, calibration, or sensor-quality
  mismatch; it is not proof of the cause.

RLS corridor:
- This implementation uses a scalar, regularized RLS update with covariance
  clipping and a denominator floor:
  gain = P * phi / (lambda + phi * P * phi)
  theta_next = theta + gain * (y - phi * theta)
  P_next = clamp((P - gain * phi * P) / lambda, Pmin, Pmax)
- A numerical bound on gain follows from P <= Pmax and lambda > 0:
  |gain| <= Pmax * |phi| / lambda.
- That bound is not a proof of parameter convergence. Bounded estimation error
  needs stated assumptions, including bounded regressors, persistent excitation,
  bounded model/measurement error, a positive denominator, and an appropriate
  forgetting/covariance policy.
