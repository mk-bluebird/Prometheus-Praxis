------------------------------ MODULE ActuationGateSynapse ------------------------------

EXTENDS Naturals, Reals, TLC

(*
  ActuationGateSynapse
  ---------------------
  TLA+ specification of the ACTUATION_GATE synapse pattern used for cyboquatic
  workloads. The gate decides whether physical actuation is allowed based on:

    - KER scalar: s = k * e - r
    - Lyapunov corridor: delta_v_t_step <= DeltaVMax
    - Joint KER–ΔVt corridor: delta_v_t_step <= Gamma * ker_s
    - Carbon-aware corridor: delta_v_t_step <= Delta * CarbonFactor(carbon_band)
    - Carbon band + KER constraints: no actuation in unsafe RED band states

  Safety property:
    actuation_allowed = TRUE implies all corridor checks are satisfied.
*)

CONSTANTS
  DeltaVMax,      \* maximum allowed ΔVt per step (e.g., 0.05)
  KerThreshold,   \* minimal ker_s required to consider actuation
  Gamma,          \* coupling coefficient for ΔVt <= Gamma * ker_s
  Delta           \* coupling coefficient for carbon-aware corridor

(*
  We model discrete carbon bands as simple strings. In TLC runs you can
  restrict them to {"GREEN","NEUTRAL","RED"} via CONSTANT CarbonBands.
*)
CONSTANT CarbonBands

VARIABLES
  ker_k, ker_e, ker_r, ker_s,
  delta_v_t_step, delta_v_t_cumulative,
  carbon_band,
  actuation_allowed

(***************************************************************************)
(* Helper definitions                                                      *)
(***************************************************************************)

(*
  KER bounds: knowledge, eco-impact, risk all in [0,1].
*)
KerBounds ==
  /\ ker_k \in Real
  /\ ker_e \in Real
  /\ ker_r \in Real
  /\ ker_k >= 0.0 /\ ker_k <= 1.0
  /\ ker_e >= 0.0 /\ ker_e <= 1.0
  /\ ker_r >= 0.0 /\ ker_r <= 1.0

(*
  KER scalar consistency: s = k * e - r
*)
KerScalarConsistency ==
  ker_s = ker_k * ker_e - ker_r

(*
  CarbonFactor maps bands to [0,1] corridor multipliers.
  Example TLC instantiation:
    CarbonBands = {"GREEN","NEUTRAL","RED"}
    GREEN   -> 1.0
    NEUTRAL -> 0.7
    RED     -> 0.4
*)
CarbonFactor(b) ==
  CASE b = "GREEN"   -> 1.0
       b = "NEUTRAL" -> 0.7
       b = "RED"     -> 0.4
       OTHER         -> 0.0

(*
  CarbonViolation encodes the band/ker constraints you enforce in SQLite:

  - RED band requires strong KER corridor: ker_s > 0.3, ker_e >= 0.8, ker_r <= 0.3.
  - For other bands, we consider CarbonViolation = FALSE here; band-specific
    tuning can be added if needed.
*)
CarbonViolation(b, s, e, r) ==
  IF b = "RED"
    THEN ~(s > 0.3 /\ e >= 0.8 /\ r <= 0.3)
    ELSE FALSE

(*
  Joint corridor predicate combining the three scalar checks:

    1. ΔVt <= DeltaVMax
    2. ΔVt <= Gamma * ker_s
    3. ΔVt <= Delta * CarbonFactor(carbon_band)

  This mirrors the code path where actuation is cut if any bound is violated.
*)
JointCorridorOK ==
  /\ delta_v_t_step <= DeltaVMax
  /\ delta_v_t_step <= Gamma * ker_s
  /\ delta_v_t_step <= Delta * CarbonFactor(carbon_band)

(*
  Overall corridor safety predicate: if actuation_allowed is TRUE, then:

    - KER scalar is above threshold.
    - Joint corridor holds.
    - No carbon band violation.
*)
CorridorSafe ==
  actuation_allowed =>
    /\ KerBounds
    /\ KerScalarConsistency
    /\ ker_s > KerThreshold
    /\ JointCorridorOK
    /\ ~CarbonViolation(carbon_band, ker_s, ker_e, ker_r)

(***************************************************************************)
(* Initial state                                                           *)
(***************************************************************************)

Init ==
  /\ KerBounds
  /\ KerScalarConsistency
  /\ ker_s > 0.0
  /\ delta_v_t_step = 0.0
  /\ delta_v_t_cumulative = 0.0
  /\ carbon_band \in CarbonBands
  /\ actuation_allowed = FALSE

(***************************************************************************)
(* Next-state relation                                                     *)
(***************************************************************************)

(*
  TelemetryUpdate:
    - KER parameters may change (e.g., module spec or eco-impact updates).
    - A new ΔVt step is observed.
    - Carbon band may change (grid carbon signals).
    - actuation_allowed' is determined by the corridor checks.
*)
TelemetryUpdate ==
  LET
    ker_k_new ==
      CHOOSE x \in Real :
        x >= 0.0 /\ x <= 1.0
    ker_e_new ==
      CHOOSE x \in Real :
        x >= 0.0 /\ x <= 1.0
    ker_r_new ==
      CHOOSE x \in Real :
        x >= 0.0 /\ x <= 1.0
    dvt_new ==
      CHOOSE x \in Real :
        x >= 0.0 /\ x <= 1.0
    band_new ==
      CHOOSE b \in CarbonBands :
        TRUE
  IN
  /\ ker_k' = ker_k_new
  /\ ker_e' = ker_e_new
  /\ ker_r' = ker_r_new
  /\ ker_s' = ker_k' * ker_e' - ker_r'
  /\ delta_v_t_step' = dvt_new
  /\ delta_v_t_cumulative' = delta_v_t_cumulative + delta_v_t_step'
  /\ carbon_band' = band_new
  /\ actuation_allowed' =
        IF /\ JointCorridorOK'
           /\ ker_s' > KerThreshold
           /\ ~CarbonViolation(carbon_band', ker_s', ker_e', ker_r')
        THEN TRUE
        ELSE FALSE

(*
  Note: JointCorridorOK' references primed variables; we define it inline here.
*)
JointCorridorOK' ==
  /\ delta_v_t_step' <= DeltaVMax
  /\ delta_v_t_step' <= Gamma * ker_s'
  /\ delta_v_t_step' <= Delta * CarbonFactor(carbon_band')

Next ==
  TelemetryUpdate

(***************************************************************************)
(* Temporal specification                                                  *)
(***************************************************************************)

Spec ==
  Init /\ [][Next]_<<ker_k, ker_e, ker_r, ker_s,
            delta_v_t_step, delta_v_t_cumulative,
            carbon_band, actuation_allowed>>

(*
  Safety theorem: under Spec, the CorridorSafe predicate holds at all times.
*)
THEOREM Spec => [](CorridorSafe)

=============================================================================
