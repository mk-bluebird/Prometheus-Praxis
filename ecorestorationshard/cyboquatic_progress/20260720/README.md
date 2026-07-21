<!-- filename: ecorestorationshard/cyboquatic_progress/20260720/README.md
     destination: ecorestorationshard/cyboquatic_progress/20260720/README.md
     repo-target: https://github.com/mk-bluebird/Prometheus-Praxis -->

# Cyboquatic Progress 2026-07-20 – Surcharge Impact Radius (Domain g, Non-Weaponized)

- This shard models **hydraulic surcharge impact zones** for Phoenix canal segments, not explosives or weapons, and is strictly non-actuating and non-offensive.[file:2][file:13]  
- The term “blast-radius” is interpreted here as a civil-engineering radius for **embankment or canal surcharge failures** (flooding footprint), aligned with Phoenix HYDRO and CYBOQUATIC governance planes.[file:2][file:13]  

## Files in this shard

- `sql/cyboquatic_dailyprogress_20260720.sql`  
  - Defines `dailyprogress`, `blastradius_surcharge`, `fog_node_parameters`, and `canal_node_parameters` with K,E,R ∈ [0,1] checks and Lyapunov residual fields, all scoped to HYDRO_SURCHARGE use.[file:2][file:13]  
- `cpp/blastradius_model_20260720.cpp`  
  - Provides a non-actuating C++ diagnostic model for surcharge-dependent breach probability and local impact radius, using bounded logistic/tanh forms, with no energy or weapons parameters.[file:13]  
- `java/BlastRadiusTelemetry20260720.java`  
  - Inserts diagnostic `blastradius_surcharge` rows into `dbcyboquaticdailyprogress.sqlite` for evidence-only, read-only governance workflows.[file:2]  
- `kotlin/BlastRadiusView20260720.kt`  
  - Reads joined K,E,R,Vt and surcharge impact rows for a given day and segment to support dashboards and AI agents.[file:2][file:13]  
- `lua/blastradius_cli_20260720.lua`  
  - Lightweight CLI over SQLite for listing surcharge impact envelopes; useful for low-power nodes and CI checks.[file:2]  
- `aln/BlastRadiusGovernance20260720.aln`  
  - Governance particle binding this shard to DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`, with explicit K,E,R triad, Lyapunov hint, and a hard `no_weaponization` invariant restricting scope to `HYDRO_SURCHARGE` and `NONACTUATING_HYDRO_SAFETY_ONLY` classification.[file:13][file:22]  

## Governance and non-weaponization guarantees

- All artifacts in this directory are:
  - Non-actuating (diagnostic, planning, and governance-only), with no actuator drivers or fieldbus integrations.[file:2][file:13]  
  - Bound to HYDRO and CYBOQUATIC planes for canal surcharge safety; they lack any variables required to compute weapons yields (no TNT equivalents, pressures, or over-pressure laws).[file:13][file:22]  
- The ALN invariant `blast_radius_governance_safe_nonweaponized` ensures:
  - K,E,R remain in the unit interval,  
  - Vt stays in a contractive-safe band,  
  - `scope_label == "HYDRO_SURCHARGE"` and `classification == "NONACTUATING_HYDRO_SAFETY_ONLY"`,  
  making any attempt to reinterpret this shard as a weapons model formally invalid under the EcoNet/EcoFort governance spine.[file:13][file:22]  

- Phoenix Hex Anchors and `Eco-Fort/db/phoenixhexregistry.sql` must register this shard under a CYBOQUATIC / BLASTRADIUS_SURCHARGE domain with NONACTUATING scope; CI must reject any build that tries to bind these artifacts into offensive, weapons, or actuating contexts.[file:2][file:12]
