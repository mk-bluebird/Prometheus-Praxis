# Research Rubric — AI Workloads for Earth Restoration

This rubric is for research agents working on Prometheus‑Praxis, eco_restoration_shard, and adaptive corridors. It defines how AI workloads must be studied and designed so they benefit earth‑restoration and improve eco‑impact and knowledge, under strict corridor and sovereignty constraints.

## 1. Start from corridor math (never from pure intuition)

- Always define corridor models before implementation:
  - Water corridors: Hflow, risk coordinates (R_vel, R_mob, R_geo), Lyapunov residual \(V_{\text{water}}\). [file:95]
  - Thermal corridors: Hheat, heat‑island indices (temp, albedo, green, heatflux), Lyapunov residual \(V_{\text{heat}}\). [web:148][web:151][web:157]
  - Air corridors: risk coordinates for particulate, gases, and biological loads, Lyapunov residual \(V_{\text{air}}\). [web:128][web:131]
- For each corridor:
  - Derive soft/hard bands, RoH ceiling (0.30), and Lyapunov functions or functionals using established ecological corridor and stability literature. [web:149][web:152][web:156]
  - Define KER deltas (knowledge, eco‑impact, risk‑of‑harm) that map restoration workloads into quantifiable improvements or degradations. [file:102]

## 2. Use non‑offsettable, cross‑corridor integration

- Treat water, thermal, and air corridors as coupled but non‑offsettable:
  - Water corridors (canals, cool corridors, cyboquatic flows) influence heat and air by changing temperatures, humidity, and contaminant transport. [web:125][web:151][web:157]
  - Thermal corridors (cool corridors, shading, reflective surfaces) change energy use and waste heat, affecting emissions and air quality. [web:123][web:130][web:148]
  - Air corridors (decontamination zones, filters, oxidizers) alter pollutant fields and can feed back into health and climate metrics. [web:128][web:131]
- Research rules:
  - Always analyze cross‑corridor interactions when designing workloads in shared urban zones (e.g., Phoenix). [web:148][web:151][web:157]
  - Never allow harmful effects in one corridor to be “paid for” by benefits in another (no trading water harm against heat relief, etc.). [file:95][file:102]
  - Use separate Lyapunov functions \(V_{\text{water}}, V_{\text{heat}}, V_{\text{air}}\), and require each to be non‑increasing or bounded under restoration workloads.

## 3. Tie restoration quotas to real and scalable baselines

- Always anchor quotas in real AI data‑center impacts:
  - Use published estimates of AI data‑center water, energy, and carbon footprints (e.g., hundreds of billions of liters of water annually, tens of millions of tons CO₂). [web:143][web:145][web:146][web:147][web:155][web:153]
  - Where possible, derive regional baselines (e.g., Phoenix or similar climate zones) for MW deployed, cooling water use, and waste heat contributions. [web:146][web:148][web:151]
- Define scalable quota functions:
  - Let \(Q_{\text{restoration}} = f(\text{MW}, \text{water use}, \text{heat flux}, \text{air emissions})\).
  - Require that for each unit of compute (MW, PFlop, or cluster), there is a matched or larger quota of:
    - Water treated or decontaminated (m³/day).
    - Air cleaned or pollutants removed (m³/day, log reductions).
    - Heat‑island intensity reduced (°C, or normalized Hheat and Vheat decreases). [web:128][web:131][web:151][web:157]
- Maintain two baselines:
  - **Q_real**: Quotas derived from actual or projected data‑center footprints in the target region.
  - **Q_future**: Parameterized quotas for hypothetical expansions and other cities, keeping the same corridor math and non‑offsettable constraints.

## 4. Implementation patterns must follow math, corridors, and governance

- Rust/ALN/MCP implementation:
  - Only implement guards and tools after corridor math and KER/Lyapunov definitions are clear.
  - Rust guards (e.g., `hflow_guard_cap_phx`, `heat_island_guard_phx`) must:
    - Compute scalar risk metrics, RoH bands, KER deltas, and Lyapunov residuals.
    - Return lane decisions (Allow/Derate/Stop) and reasons; never actuate hardware directly. [file:95][file:102]
  - ALN particles must:
    - Encode corridor definitions, workload objects, and non‑offsettable constraints for formal reasoning.
    - Bind to SQLite spines with HASHONLY evidence where needed. [cite:140]
  - MCP tools must:
    - Be annotated with `actuationallowed = false` for diagnostics and `ecosafetyrequired` flags for gated operations. [file:102]
    - Only surface corridor diagnostics and safety vectors to agents; actuation flows through separate, treaty‑bound channels.

## 5. Research tasks must improve KER and tighten corridors

- For every research action:
  - Require explicit KER evaluation:
    - **K (Knowledge)**: Does the work increase understanding of corridor dynamics, restoration strategies, or governance? [cite:137]
    - **E (Eco‑impact)**: Does it measurably improve water, air, or heat conditions or reduce harmful loads? [web:154]
    - **R (Risk‑of‑harm)**: Does it keep RoH ≤ 0.30 and respect neurorights and sovereignty? [file:95][cite:135]
  - Prohibit:
    - Any change that loosens corridors or raises Vt without transparent evidence and governance review.
    - Any identity classification or downgrade of human skills/rights; research must remain sovereignty‑first. [cite:135]
- Documentation requirements:
  - Each completed research artifact (Rust module, ALN shard, MCP tool, dataset) must:
    - Declare its corridor domains and bands.
    - Log KER deltas and Lyapunov impacts.
    - State whether it is diagnostic‑only or has actuation pathways (with clear governance bindings).

## 6. Phoenix as a reference urban lab (but not the only one)

- Phoenix corridors:
  - Use Phoenix cool corridors, canal systems, and heat‑vulnerability mapping as primary reference data for integrated water‑thermal‑air corridor design. [web:148][web:151][web:157]
  - Bind AI workloads there to:
    - CAP water corridors for cyboquatic restoration.
    - Cool corridors for thermal mitigation.
    - Air‑quality corridors around data centers and dense traffic.
- Generalization:
  - Ensure models and quotas are abstract enough to apply to other cities and biomes, but always re‑anchored to local corridor data and governance constraints.

---

Research agents should treat this rubric as a hard guidance layer:

- No work starts without corridor math and KER/Lyapunov definitions.
- No analysis ignores cross‑corridor interactions in shared zones.
- No restoration quota is considered valid unless tied to real AI footprints and scalable future units.
- No implementation bypasses Rust/ALN/MCP governance or non‑actuating constraints.

This keeps AI workloads aligned with earth‑restoration, not extraction, and ensures every new artifact in `crates/adaptive-corridors` measurably tightens safety and improves eco‑impact over time. [file:95][file:102][web:143][web:145][web:146][web:147][web:151][web:157]
