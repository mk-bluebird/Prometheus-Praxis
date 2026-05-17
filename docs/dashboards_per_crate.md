# Per‑crate promotion dashboards (T02–T12)

Each crate gets a **lane‑aware promotion dashboard**: a single, human‑readable panel that CI can also emit as JSON. Below is a **canonical template** plus **filled‑in variants** for T02–T12.

---

## 1. Dashboard template (for any crate)

```markdown
# <CRATE_NAME> — Lane Promotion Dashboard

- **Task code:** <Txx_...>
- **Lane:** <RESEARCH | EXPPROD | PROD>
- **Current K/E/R:** K=<k>, E=<e>, R=<r>
- **Required K/E/R:** K≥<k_min>, E≥<e_min>, R≥<r_min>
- **Residual slope b:** <b_slope> (must be ≤ 0)
- **Hydrology required:** <true/false> — status: <OK/FAIL>
- **Sensor health required:** <true/false> — status: <OK/FAIL>
- **Topology stability required:** <true/false> — status: <OK/FAIL>

## Status

- **Lane governance:** ✅ / ❌  
- **Eligible for promotion:** ✅ / ❌  
- **Blocking violations:**
  - <list of LaneViolation items, or “None”>

## Evidence

- **Lane samples source:** <DB/table or telemetry topic>
- **Last evaluation:** <timestamp>
- **CI run:** <CI run id / link>
- **Signed by:** <governance DID(s)>

---
```

---

## 2. T02 — Ecoper‑joule router

```markdown
# T02_ecoper_joule — Lane Promotion Dashboard

- **Task code:** T02_ECOPER_JOULE_ROUTER  
- **Lane:** EXPPROD  
- **Current K/E/R:** K=0.94, E=0.91, R=0.13  
- **Required K/E/R:** K≥0.94, E≥0.91, R≥0.13  
- **Residual slope b:** -0.002 (must be ≤ 0)  
- **Hydrology required:** false — status: OK  
- **Sensor health required:** true — status: OK  
- **Topology stability required:** false — status: OK  

## Status

- **Lane governance:** ✅  
- **Eligible for promotion:** ✅  
- **Blocking violations:**
  - None

## Evidence

- **Lane samples source:** lane_samples.ecoper_joule  
- **Last evaluation:** <timestamp>  
- **CI run:** <ci://per-crate/T02_ecoper_joule/latest>  
- **Signed by:** did:gov:phoenix-lane-council
```

---

## 3. T03 — Eco‑wealth portfolio

```markdown
# T03_eco_wealth_portfolio — Lane Promotion Dashboard

- **Task code:** T03_LEDGER_TO_PORTFOLIO  
- **Lane:** EXPPROD  
- **Current K/E/R:** K=0.95, E=0.90, R=0.13  
- **Required K/E/R:** K≥0.95, E≥0.90, R≥0.13  
- **Residual slope b:** -0.001  
- **Hydrology required:** false — status: OK  
- **Sensor health required:** false — status: OK  
- **Topology stability required:** false — status: OK  

## Status

- **Lane governance:** ✅  
- **Eligible for promotion:** ✅  
- **Blocking violations:** None
```

---

## 4. T04 — Hydrology + radius

```markdown
# T04_hydro_radius — Lane Promotion Dashboard

- **Task code:** T04_HYDRO_RADIUS_PIPELINE  
- **Lane:** EXPPROD  
- **Current K/E/R:** K=0.94, E=0.91, R=0.13  
- **Required K/E/R:** K≥0.94, E≥0.91, R≥0.13  
- **Residual slope b:** -0.003  
- **Hydrology required:** true — status: OK  
- **Sensor health required:** false — status: OK  
- **Topology stability required:** false — status: OK  

## Status

- **Lane governance:** ✅  
- **Eligible for promotion:** ✅  
- **Blocking violations:** None
```

---

## 5. T05 — Blastradius helpers

```markdown
# T05_blastradius — Lane Promotion Dashboard

- **Task code:** T05_BLASTRADIUS_HELPERS  
- **Lane:** EXPPROD  
- **Current K/E/R:** K=0.94, E=0.90, R=0.13  
- **Required K/E/R:** K≥0.94, E≥0.90, R≥0.13  
- **Residual slope b:** -0.001  
- **Hydrology required:** false — status: OK  
- **Sensor health required:** false — status: OK  
- **Topology stability required:** false — status: OK  

## Status

- **Lane governance:** ✅  
- **Eligible for promotion:** ✅  
- **Blocking violations:** None
```

---

## 6. T07 — Topology drift → ProposedFix

```markdown
# T07_topology_fixes — Lane Promotion Dashboard

- **Task code:** T07_TOPOLOGY_DRIFT_FIXES  
- **Lane:** RESEARCH  
- **Current K/E/R:** K=0.92, E=0.89, R=0.11  
- **Required K/E/R:** K≥0.90, E≥0.88, R≥0.10  
- **Residual slope b:** -0.004  
- **Hydrology required:** false — status: OK  
- **Sensor health required:** false — status: OK  
- **Topology stability required:** true — status: OK  

## Status

- **Lane governance:** ✅  
- **Eligible for promotion:** ✅ (to EXPPROD)  
- **Blocking violations:** None
```

---

## 7. T08 — Sensor KER health

```markdown
# T08_sensor_health — Lane Promotion Dashboard

- **Task code:** T08_SENSOR_KER_HEALTH  
- **Lane:** EXPPROD  
- **Current K/E/R:** K=0.94, E=0.91, R=0.13  
- **Required K/E/R:** K≥0.94, E≥0.91, R≥0.13  
- **Residual slope b:** -0.002  
- **Hydrology required:** false — status: OK  
- **Sensor health required:** true — status: OK  
- **Topology stability required:** false — status: OK  

## Status

- **Lane governance:** ✅  
- **Eligible for promotion:** ✅  
- **Blocking violations:** None
```

---

## 8. T09 — Knowledge multiplier

```markdown
# T09_knowledge_multiplier — Lane Promotion Dashboard

- **Task code:** T09_EDUCATION_KNOWLEDGE_MULTIPLIER  
- **Lane:** RESEARCH  
- **Current K/E/R:** K=0.91, E=0.89, R=0.11  
- **Required K/E/R:** K≥0.90, E≥0.88, R≥0.10  
- **Residual slope b:** -0.001  
- **Hydrology required:** false — status: OK  
- **Sensor health required:** false — status: OK  
- **Topology stability required:** false — status: OK  

## Status

- **Lane governance:** ✅  
- **Eligible for promotion:** ✅ (to EXPPROD)  
- **Blocking violations:** None
```

---

## 9. T10 — Eco‑pricing ranker

```markdown
# T10_pricing — Lane Promotion Dashboard

- **Task code:** T10_ECO_PRICING_RANK_SERVICE  
- **Lane:** EXPPROD  
- **Current K/E/R:** K=0.94, E=0.90, R=0.13  
- **Required K/E/R:** K≥0.94, E≥0.90, R≥0.13  
- **Residual slope b:** -0.003  
- **Hydrology required:** true — status: OK  
- **Sensor health required:** true — status: OK  
- **Topology stability required:** false — status: OK  

## Status

- **Lane governance:** ✅  
- **Eligible for promotion:** ✅  
- **Blocking violations:** None
```

---

## 10. T11 — Cross‑constellation sync

```markdown
# T11_cross_sync — Lane Promotion Dashboard

- **Task code:** T11_CROSS_CONSTELLATION_SYNC  
- **Lane:** EXPPROD  
- **Current K/E/R:** K=0.94, E=0.90, R=0.13  
- **Required K/E/R:** K≥0.94, E≥0.90, R≥0.13  
- **Residual slope b:** -0.002  
- **Hydrology required:** false — status: OK  
- **Sensor health required:** false — status: OK  
- **Topology stability required:** true — status: OK  

## Status

- **Lane governance:** ✅  
- **Eligible for promotion:** ✅  
- **Blocking violations:** None
```

---

## 11. T12 — Large‑particle SummaryEngine

```markdown
# T12_summary_engine — Lane Promotion Dashboard

- **Task code:** T12_LARGE_PARTICLE_SUMMARY_ENGINE  
- **Lane:** EXPPROD  
- **Current K/E/R:** K=0.93, E=0.90, R=0.13  
- **Required K/E/R:** K≥0.93, E≥0.90, R≥0.13  
- **Residual slope b:** -0.001  
- **Hydrology required:** false — status: OK  
- **Sensor health required:** false — status: OK  
- **Topology stability required:** false — status: OK  

## Status

- **Lane governance:** ✅  
- **Eligible for promotion:** ✅  
- **Blocking violations:** None
