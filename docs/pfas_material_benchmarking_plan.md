# PFAS and Biodegradable Materials Benchmarking Plan

## 1. Objectives and Inputs

Objectives:

- Compare PFAS corridor safety across materials and canal segments.
- Quantify eco‑impact trade‑offs between materials that differ in PFAS sorption, cold‑survival, and biodegradability.
- Produce comparative tables that governance and engineering teams can use in eco decision‑making.

Inputs:

- **PFAS data:** Canal PFAS mass, sorbed fraction, temperature history, cold‑survival parameters per segment and hex.[94]
- **Material data:** OECD 301/ISO 14851/14855 biodegradation curves, ultimate biodegradation fractions, time‑to‑threshold, toxicity bands.[94]
- **Engines:**
  - C++ PFAS corridor engine: discrete‑time state update over mass, sorbed fraction, and cold‑survival factor with a Lyapunov corridor.[94]
  - C++ material eco‑impact engine: maps biodegradability, toxicity, and placement context into an eco‑impact scalar \(e_{\text{eco\_impact}}\).[94]

## 2. Benchmarking Workflow with C++ Engines

1. **Data ingestion:**
   - Parse PFAS telemetry per canal segment into a C++ struct (mass_kg, sorbed_fraction, cold_survival_factor, temperature profile).
   - Parse material test results into structs capturing biodegradation curves and toxicity metrics.

2. **PFAS corridor simulation:**
   - For each (segment, material) combination, simulate PFAS fate for a fixed horizon (e.g. 10–20 years) using the PFAS corridor engine:
     - Update state via discrete transition matrix capturing degradation and sorption with cold‑survival slowdown.[94]
     - Compute a PFAS risk coordinate \(r_{\text{pfas}}(t)\) and its Lyapunov contribution.
   - Record:
     - Time‑averaged PFAS mass and risk coordinate.
     - Corridor violations (PFAS mass exceeding safegoldhard bands).
     - Empirical KER triads and \(s_t\) for PFAS‑focused windows.

3. **Material eco‑impact scoring:**
   - Map OECD/ISO curves to a ready‑biodegradability scalar \(b_{\text{ready}} \in [0,1]\) using normalized ultimate degradation and time‑to‑threshold.[94]
   - Feed \(b_{\text{ready}}\), toxicity, and placement context into the material eco‑impact engine to compute \(e_{\text{eco\_impact}}\).
   - Compute risk coordinates for materials (e.g., \(r_{\text{materials}}\), \(r_{\text{toxicity}}\)) and their Lyapunov contributions.

4. **Combined scenario evaluation:**
   - For each scenario (segment, material, PFAS profile):
     - Compute overall risk vector including hydraulics, PFAS, materials.
     - Compute Lyapunov residual \(V_t\) and window KER values.
     - Evaluate whether KER thresholds and Lyapunov decrement conditions are satisfied.

## 3. Comparative Table Generation

With C++ engines producing per‑scenario outputs, generate CSV tables in `cpp/tools/eco_serialization.cpp` that include:

- **Scenario identifiers:** canal_segment_id, hex_id, material_id.
- **PFAS results:** final PFAS mass_kg, max \(r_{\text{pfas}}\), number of corridor breaches, PFAS KER triad, PFAS \(s_t\).
- **Material results:** \(b_{\text{ready}}\), toxicity index, \(e_{\text{eco\_impact}}\), material risk coordinates.
- **Combined eco metrics:** overall KER triad, Lyapunov residual summary, whether non‑offsettable PFAS/material planes stayed within gold bands.

Example header:

```text
segment_id,hex_id,material_id,final_pfas_mass_kg,max_r_pfas,pfas_K,pfas_E,pfas_R,material_b_ready,material_toxicity_index,e_eco_impact,overall_K,overall_E,overall_R,V_max,ker_s_positive_fraction
```

Governance usage:

- Sort scenarios by eco‑impact and risk to identify materials that both reduce PFAS mass and improve biodegradability without violating non‑offsettable planes.
- Use KER triads and Lyapunov residuals to define placement corridors for materials in Phoenix canals and related infrastructure.
- Archive tables in SQLite (`pfas_material_benchmark`) with evidence hex stamps and DID bindings to support ALN‑linked governance and future CI replays.[95][94]
