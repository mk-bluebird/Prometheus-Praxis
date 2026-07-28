<!-- File: docs/whitepaper-prometheus-praxis.md -->

# Prometheus‑Praxis: Non‑Actuating Governance Spine for EcoNet and Eco‑Fort

## 1. Abstract

Prometheus‑Praxis is the non‑actuating governance and execution spine for the EcoNet / Eco‑Fort constellation, implemented as a Rust‑first, ALN‑governed, SQLite‑backed monorepo band inside `Prometheus-Praxis` and the broader ecorestorationshard workspace.[file:18][file:21] Its sole mandate is to turn formally specified ecosafety grammar (ALN), KER particles, and Lyapunov residuals into verifiable metrics, decisions, and views that always tighten ecological safety corridors, never widen them, and never directly actuate physical machinery.[file:18][file:13]

Prometheus‑Praxis treats all physical capabilities (pumps, nanoswarms, drainage machinery, AI datacenters, cyboquatic systems) as sealed superpowers living behind strict governance walls; it only hosts **diagnostic** and governance kernels that compute KER, RoH, Lyapunov, lane status, and always‑improve scores under monotone‑safety constraints.[file:18][file:13] All actuation remains in separate ENGINE repositories and TEE enclaves, where ecosafety, neurorights, and corridor invariants are enforced before any change can touch the real world.[file:13][file:18]

The system is anchored by three mathematical primitives:  
- A KER triad \((K, E, R)\) with conservative composition algebra and lane thresholds, encoded in ALN shards such as `KERComposition2026v1` and consumed by Rust crates like `prometheus-praxis-ai` and Lyapunov guard libraries.[file:18][file:13]  
- A Lyapunov residual kernel \(V_{t+1} - V_t\) that must never increase beyond a narrow band for any allowed policy, encoded in ecosafety specs and enforced by Rust+Kani guards and SQLite triggers (no‑corridor, no‑build).[file:18][file:21]  
- A monotone always‑improve score that ranks nodes and windows by how strongly they improve or at least do not degrade under KER and Lyapunov constraints, implemented in non‑actuating Rust (`alwaysimprove.rs`) and surfaced via AI‑safe catalog views.[file:18]

Prometheus‑Praxis is also a sovereignty and neurorights engine: ALN shards like `AbsoluteDataSovereigntyPolicy2026v1.aln`, identity kernels, and `agentsafeconsentguard` tables ensure that all telemetry, KER evidence, and AI‑visible views are governed by explicit consent and neurorights corridors.[file:18][file:21] No raw neural data, identity‑sensitive health signals, or actuation parameters are ever exposed through AI‑safe catalogs; instead, only corridor‑bounded aggregates (e.g., daily KER, RoH, Tsafe, lane, consent flags) appear in diagnostic views.[file:18]

At the repository level, Prometheus‑Praxis standardizes a pattern: non‑actuating Rust crates (`prometheus-praxis`, `prometheus-praxis-ai`, `prometheus-praxis-lyapunov-guard`, hex anchor and provenance crates), ALN grammars for corridors and functions (`ppx.function.meta.v1.aln`, ecosafety specs), and SQLite spines for lanes, blastradius, cyboquatic progress, and AI‑safe catalogs.[file:18][file:21] Tooling in Lua, Python, Java, Kotlin, and C/CPP provides reporting, ledger verification, and CI checks, but all follow the same rule: read from governed views, never bypass ecosafety triggers, and never actuate.[file:9][file:18]

This whitepaper specifies the problem Prometheus‑Praxis addresses, the design goals and invariants it must preserve, its role within EcoNet / Eco‑Fort, the KER–Lyapunov mathematical spine, the governance and sovereignty layers, and concrete instantiations for Phoenix urban heat, drainage, cyboquatic machinery, and AI datacenter workloads.[file:18][file:13] It is written to be directly executable as architecture: every section corresponds to existing or planned ALN shards, Rust crates, SQL schemas, and tools in `github.com/mk-bluebird/Prometheus-Praxis`, with all assets hex‑anchored to the Bostrom identities `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` and related ALN DIDs.[file:18][file:21]

---

## 2. Problem Statement

### 2.1 Ecological and urban‑infrastructure gaps

- Phoenix and similar arid metros face coupled crises across:
  - Urban heat islands (UHI) driven by impervious surfaces, low canopy cover, and high HVAC loads.
  - Hydrology and drainage (MAR, canals, BOD/TSS/CEC decay) under climate‑stressed, aging infrastructure.
  - Materials and microplastics in cyboquatic networks and industrial flows.
  - Biodiversity corridors (pollinators, birds, aquatic life) fragmented by roads, canals, and development.[file:18][file:13]
- Existing monitoring systems are typically:
  - Fragmented across utilities, agencies, and vendors, each with bespoke metrics and dashboards.
  - Lacking a shared Lyapunov/KER spine to guarantee that any policy or control change cannot increase ecological “energy” \(V\) or violate Risk‑of‑Harm ceilings.
  - Poorly tied to identity, provenance, and eco‑wealth ledgers, making it hard to reward genuinely restorative actions or detect greenwashing.[file:18][file:21]

### 2.2 Governance and AI‑safety gaps

- Most smart‑city, IoT, and AI orchestration stacks:
  - Allow direct actuation (pumps, valves, nanoswarms, HVAC, data centers) from loosely governed control loops, with limited or no formal guarantees on safety or ecological impact.[file:13][file:18]
  - Do not encode “no corridor, no build”: there is no first‑class notion that if corridors, plane weights, or Lyapunov invariants are missing, deployment is automatically disallowed.[file:18][file:21]
  - Treat AI agents and dashboards as first‑class operators without enforcing strict separation between diagnostics and superpowers; AI patterns can accidentally become covert control channels.[file:18]

- AI‑centric ecosystems also:
  - Lack an explicit, machine‑checkable neurorights and sovereignty layer: raw biosignals, neural traces, and identity‑linked metrics can leak into models and logs without enforceable constraints.[file:18][file:21]
  - Have no standard way to encode host consent, telemetry families, and per‑field visibility into the same constitutional grammar that governs KER and corridor rules.[file:18]

### 2.3 Formal‑methods and compositionality gaps

- Without a shared KER and Lyapunov grammar:
  - It is impossible to prove that a given change (new controller, new workload, new nanoswarm pattern) respects invariants such as \(V_{t+1} \le V_t\) up to a small band, or keeps Risk‑of‑Harm within corridor bounds.[file:18][file:13]
  - Lane promotions (RESEARCH → PILOT → PRODUCTION) are ad‑hoc; there is no formal notion of “always‑improve” scoring, SafeStep gates, or monotone safety evolution across upgrades, fast‑track lanes, or emergency releases.[file:18][file:21]
- Evidence and telemetry aggregation is often:
  - Statistically sophisticated but governance‑weak; composition rules for risk and eco‑impact are implicit and can accidentally average down risk, violating “risk‑never‑underestimated” requirements.[file:13]
  - Not encoded in a frozen grammar (ALN) with machine‑checkable invariants, so pipelines can silently drift away from the intended safety semantics over time.[file:13][file:21]

### 2.4 Repository and ecosystem fragmentation

- Within the broader Eco‑Fort / EcoNet constellation, prior work produced:
  - Many high‑value ALN shards (ecosafety corridors, eco‑wealth, knowledge kernels, cyboquatic risk), Rust crates, SQL spines, and tools across multiple repos and languages.[file:18][file:21]
  - Rich cyboquatic progress shards (workload, drainage decay, blastradius, daily KER windows) and TEE patterns for KER computation and signing.[file:9][file:13]
- This constellation, before consolidation, suffered from:
  - Difficult cross‑navigation: contributors and AI agents struggled to locate canonical engines, ALN specs, or DB schemas for a given eco‑plane or domain.
  - Inconsistent enforcement: some shards had strong KER/Lyapunov triggers and invariants; others were still exploratory, with no single governance spine tying them together.[file:18][file:21]

### 2.5 What Prometheus‑Praxis must solve

- Provide a single, non‑actuating, well‑typed execution band that:
  - Centralizes KER, Lyapunov, lane, and always‑improve logic in Rust crates with Kani proofs and ALN binding, not in ad‑hoc SQL or application code.[file:18]
  - Encodes function meta, superpower boundaries, and governance flags in ALN (`ppx.function.meta.v1.aln`, ecosafety and identity specs), ensuring that any callable tool or diagnostic is classified and gated.[file:18]
  - Standardizes SQLite governance spines (lane status, blastradius, cyboquatic indexes, AI‑safe catalog) with triggers that enforce non‑increase of Lyapunov residuals and corridor compliance at write time.[file:18][file:21]
- Serve as the AI‑facing, eco‑restorative lens by:
  - Exposing only AI‑safe views (`vagentsafecatalog`, cyboquatic facades, workload and drainage summaries) that surface KER, RoH, Lyapunov, and lane diagnostics, but never raw biosignals or actuation fields.[file:18][file:9]
  - Binding every catalog entry, shard, and crate to Bostrom and ALN identities, with hex anchors and provenance tables, so contributions can be rewarded and audited without compromising sovereignty.[file:18][file:21]

Prometheus‑Praxis exists because neither traditional SCADA nor generic AI stacks can deliver these properties alone; a dedicated, mathematically grounded, non‑actuating spine is required to make eco‑restorative, AI‑assisted infrastructure both safe and provable at city scale.[file:18][file:13]

## 3. Research Objectives

Prometheus‑Praxis is designed as a **research engine** that makes eco‑restoration progress “just by researching it,” by turning every diagnostic computation and governance check into a measurable, DID‑anchored contribution to ecological health.[file:18][file:21] The core objectives are intentionally narrow and non‑actuating, so they can safely sit at the center of EcoNet and Eco‑Fort without ever driving hardware directly.

The primary objectives are:

- Encode ecosafety and governance as executable grammar.  
  - Formalize corridors, KER triads, Lyapunov kernels, plane weights, blast‑radius limits, and lane policies in ALN shards (e.g., ecosafety, Cyboquatic, EcoWealth, lane governance), so every Rust crate and SQL spine consumes a single, authoritative grammar.[file:18][file:21]  
  - Prove and enforce invariants such as “no corridor widening,” “Lyapunov residuals non‑increasing,” and “R is never reduced by composition,” using ALN invariants, Rust logic, Kani harnesses, and SQLite triggers.[file:18][file:21]

- Provide a non‑actuating governance spine for Cyboquatic and EcoNet machinery.  
  - Standardize SQLite schemas and views for workloads, blast‑radius, microplastic risk, drainage‑decay, and KER windows (e.g., `dbcyboquaticdailyprogress.sql`, `dbcyboquaticblastradiusindex.sql`, `dbcyboquaticmicroplasticriskindex.sql`).[file:18][file:9]  
  - Implement Rust crates (`prometheus-praxis`, `prometheus-praxis-ai`, `prometheus-praxis-lyapunov-guard`, cyboquatic spines) that only read from these views, compute diagnostics, and emit ALN/JSON shards; they never issue actuation commands.[file:18]

- Make KER, RoH, Lyapunov, and lane decisions first‑class, verifiable metrics.  
  - Surface internal rich views exposing full KER, RoH, Tsafe, Lyapunov residuals, lane status, corridor IDs, and treaty bindings for auditing, observability, and formal verification.[file:18]  
  - Provide thin, AI‑friendly facades that expose only the minimal summary (e.g., eco‑per‑joule, residual bands, safetopromote flags) needed for reasoning, without leaking actuation‑adjacent details or identity‑sensitive telemetry.[file:18]

- Implement an always‑improve kernel as verifiable, non‑actuating Rust.  
  - Centralize always‑improve scoring in Rust (`crates/prometheus-praxis-ai/src/alwaysimprove.rs`), consuming KER outputs and Lyapunov snapshots from existing governance crates and returning a bounded scalar score plus safetopromote flag.[file:18]  
  - Prove key properties with Kani, such as: if K and E are above lane minima, R is below lane max, RoH is under ceiling, and Lyapunov delta is within band, then the kernel never incorrectly blocks promotion; conversely, if Lyapunov increases or risk ceilings are breached, safetopromote is never true.[file:18]

- Build an AI‑safe catalog and pattern layer that strictly enforces superpower and consent boundaries.  
  - Define an `agentsafecatalog` / `agentsafediagnostics` schema and `vagentsafecatalog` view that list only non‑actuating tools, nodes, and windows, along with KER, RoH, Lyapunov, and always‑improve diagnostics, but no actuation fields or raw biosignals.[file:18]  
  - Gate AI‑visible telemetry families (KER, RoH, Lyapunov, EcoHealth aggregates) through `agentsafeconsentguard`, so neurorights and sovereign consent envelopes determine what any agent can see, and nothing can bypass these ALN‑backed decisions.[file:18]

- Turn cross‑repo eco‑machinery into a single, DID‑anchored research surface.  
  - Bind all code files, SQL schemas, and shard definitions to Bostrom DIDs and hex anchors (e.g., via hexanchor crates and Phoenix hex registry tables), so every governance computation is attributable to the steward DID and ALN authority.[file:18][file:21]  
  - Use Lua, Java, Kotlin, and C/CPP harnesses only as diagnostic clients (e.g., microplastic risk reports, blast‑radius simulations, data‑center KER monitors), never as actuators, and always backed by ALN specs and SQLite views.[file:9][file:18]

These objectives define Prometheus‑Praxis as a verifiable, non‑actuating governance kernel that can safely coordinate Cyboquatic, EcoNet, and Eco‑Fort work while keeping all superpowers sealed and all ecological safety guarantees explicit and machine‑checkable.[file:18][file:21]

---

## 4. System Architecture Overview

The Prometheus‑Praxis architecture is a layered, mono‑repo spine that connects ALN grammar, Rust governance crates, SQLite spines, and multi‑language diagnostic tools into a single, DID‑anchored ecosystem.[file:18][file:21] Every layer is designed to be non‑actuating, corridor‑tightening, and formally auditable.

- Constitutional grammar layer (ALN).  
  - Core ecosafety, identity, and governance are defined in ALN shards such as `alnPrometheusPraxisCore.v1.aln`, ecosafety risk vectors, lane governance topology, EcoWealth contracts, Cyboquatic ecosafety and microplastic risk, blast‑radius governance, and plane weights.[file:18][file:21]  
  - ALN also specifies function metadata (`ppx.function.meta.v1.aln`), role bands (`prometheus-role-bands.v1.aln`), shard layouts, AIsafe catalogs, and telemetry kernels, giving Rust, SQL, and tooling a shared, frozen grammar to adhere to.[file:18][file:21]

- Non‑actuating Rust crate band.  
  - Governance and observability crates (`prometheus-praxis`, `prometheus-praxis-ai`, `prometheus-praxis-lyapunov-guard`, KER composition and residual crates, hex anchor and provenance crates) implement execution‑layer logic over KER, RoH, Lyapunov, lanes, eco‑wealth, and blast‑radius without ever issuing actuator commands.[file:18]  
  - Crates are Rust 2024, `rust-version = "1.85"`, `!forbid(unsafe_code)` where shown, and use Kani 0.67 for proofs over Lyapunov guards and always‑improve scoring, turning safety invariants into machine‑checked properties.[file:18]

- SQLite governance spines and views.  
  - Workspaces under `workspacedb` host canonical schemas for cyboquatic machinery, ecosafety windows, EcoNet indexes, blastradius, microplastic risk, and AI‑safe catalogs; triggers enforce “no corridor, no build” and Lyapunov non‑increase.[file:18][file:9]  
  - A two‑tier view architecture exposes internal rich views (full KER, RoH, Tsafe, Lyapunov, lane, treaties) for CI and auditors, and thin, AI‑friendly facades (e.g., `vcyboquaticmicroplasticriskfacade`, `vcyboworkloadnodewindow`, `vagentsafecatalog`) for chat agents and dashboards.[file:18][file:9]

- Diagnostic toolchains (Lua, Python, Java, Kotlin, C/CPP).  
  - Lua: governance tools validate ALN catalogs, generate shard layout diagrams, verify Merkle ledgers, sandbox agent functions, compute cyboquatic microplastic risk scores, and emit GitHub‑markdown reports for stewards.[file:9][file:18]  
  - Python: diagnostics scripts traverse ALN specs, summarize repository changes, enforce docstring and ALN‑reference quality, and run maintenance sessions, all using native tooling without new dependencies.[file:18]  
  - Java/Kotlin: JVM agents monitor AI datacenter KER, corridor blast‑radius simulations, and cyboquatic drainage summaries, reading from SQLite/JSON outputs and posting human‑readable HTML/JSON reports; they never alter lanes or actuators.[file:9][file:18]  
  - C/CPP: numeric engines in `srccpp` compute non‑actuating workload, drainage‑decay, and blast‑radius metrics for Cyboquatic and hydrology planes, accessed via Rust FFI and never bound directly to devices.[file:18][file:21]

- AI‑safe catalog and consent gates.  
  - The `dbdbagentsafecatalog.sql` spine and `vagentsafecatalog` view define exactly which tools, nodes, and windows are visible to AI agents, along with KER, RoH, Lyapunov, always‑improve, and stability flags, but no actuation or raw biosignals.[file:18]  
  - An `agentsafeconsentguard` table encodes per‑catalog entry, per‑telemetry‑family permissions, ensuring neurorights and sovereign consent engines decide which KER, RoH, Lyapunov, or health aggregates can be surfaced, and preventing any AI path from bypassing these ALN‑defined constraints.[file:18]

- Provenance, hex anchoring, and cross‑repo wiring.  
  - Hex anchor and provenance crates bind Rust, SQL, ALN, and tool files to Phoenix hex registries and Bostrom DIDs, so every governance artifact (e.g., KER shards, blastradius ledgers, eco‑wealth statements) is cryptographically attributed and auditable.[file:18][file:21]  
  - The shard layout ALN (`prometheus-shard-layout.v1.aln`) plus PlantUML generators keep the global shard and trust‑band topology explicit in `ARCHITECTURE.md`, making cross‑shard dependencies inspectable for humans and CI alike.[file:18]

In combination, these layers form a single, Eco‑Fort‑aligned governance lattice: ALN grammars define what is allowed, Rust crates implement verifiable kernels, SQLite enforces invariants and stores evidence, tools provide human‑ and AI‑readable reports, and the AI‑safe catalog plus consent guards ensure that no agent ever escapes the non‑actuating, corridor‑tightening envelope of Prometheus‑Praxis.[file:18][file:21]

## 5. Mathematical Spine: KER–Lyapunov Framework

Prometheus‑Praxis encodes risk as a small set of mathematically sharp primitives, so every governance decision can be traced to explicit coordinates, vectors, and residuals rather than opaque scores.[file:18]

- RiskCoord and RiskVector.  
  - A RiskCoord is a bounded scalar \(r_p \in [0,1]\) for a specific plane \(p\) (e.g., CARBON, HEAT, WATER, BIODIVERSITY, PSYCHRISK), representing normalized risk of harm along that dimension.[file:18]  
  - A RiskVector is the tuple of all active coordinates \(R = (r_{\text{carbon}}, r_{\text{heat}}, r_{\text{water}}, \dots)\) for a node or window, accompanied by a KER triad \((K,E,R_\text{agg})\) where \(R_\text{agg}\) is a plane‑weighted aggregation of the individual coordinates.[file:18]

- LyapunovWeights and Residual.  
  - The Lyapunov function \(V_t\) is a scalar potential defined per node or shard that encodes the “distance” from an ecologically restorative equilibrium, with per‑plane weights drawn from ALN PlaneWeights shards (e.g., Tree‑of‑Life plane weights for biodiversity vs. hydrology).[file:18]  
  - The Lyapunov residual over a window is \( \Delta V = V_{t+1} - V_t \), with constraints that \(\Delta V \le 0\) in restorative or allowed states, and stricter bands (\(\Delta V \le \varepsilon\)) for always‑improve windows; these bands are part of ecosafety ALN specs and enforced via Rust guards and SQL triggers.[file:18]

- SafeStepGate and corridor grammar.  
  - The SafeStepGate is a function that evaluates whether a proposed step (or a realized window) is admissible given K, E, R, RoH, and Lyapunov residual; a step is safe only if K and E exceed lane‑specific minima, R and RoH stay below lane ceilings, and \( \Delta V \) remains within its non‑increase band.[file:18]  
  - Corridor grammar is expressed in ALN ecosafety and corridor shards as SAFE, GOLD, HARD bands for each corridor variable (e.g., \(\Delta V\), maxcarbonradius, maxbiodivradius, RoH), with weights and MANDATORY flags; the combination defines admissible corridors in the KER–Lyapunov space and is consumed directly by Rust crates and SQLite invariants.[file:18]

- NonOffsettablePlanes and Tree‑of‑Life extensions.  
  - Non‑offsettable planes (e.g., PSYCHRISK, critical AQUATIC BIODIVERSITY, neurorights‑linked NEUROBIOME) are marked in ALN so that improvements in other planes cannot compensate for violations; in practice, any violation in such a plane forces a Stop or Derate verdict regardless of K or E gains elsewhere.[file:18]  
  - Tree‑of‑Life (ToL) PlaneWeights shards encode higher weights for biodiversity and long‑horizon regenerative dynamics, and the Lyapunov function is extended with “biodiversity‑boosted” residuals where genuine restoration (e.g., corridor species return, soil biota recovery) can tighten corridors and lower \(V\) even if short‑term energy metrics are flat.[file:18]

- Windowed KER and always‑improve metrics.  
  - Observability views and KER kernels compute windowed KER vectors, RoH, and Lyapunov deltas per node or shard window (e.g., vcyboworkloadnodewindow, cyboquatic microplastic risk windows), so the system reasons over finite windows rather than individual events.[file:18][file:9]  
  - The always‑improve kernel in `crates/prometheus-praxis-ai/src/alwaysimprove.rs` takes lane, KER outputs, RoH snapshot, and Lyapunov residual snapshots and returns an AlwaysImproveScore: a bounded scalar \(s \in [0,1]\) plus a safetopromote flag, under rules such as “RoH below global ceiling, \(|\Delta V|\) below max band, K/E above lane minima, R below lane max; otherwise \(s=0\), safetopromote=false.”[file:18]

These primitives make the KER–Lyapunov spine a compact but expressive grammar for risk and restoration, and they are implemented in non‑actuating Rust and SQL so they can be formally verified and audited.[file:18]

---

## 6. Governance Spine and Superpower Boundaries

Prometheus‑Praxis uses the KER–Lyapunov spine to drive governance while treating all physical capabilities as sealed superpowers that are never exposed as tools.[file:18] Governance crates, ALN shards, and DB schemas together define how verdicts are computed, how lanes are managed, and how SafeStepGate is enforced.

- Superpower model and PraxisGovernanceKernel.  
  - Superpowers such as Heat/Water/Tree macro‑infrastructure, nanoswarm routing, psychrisk engines, and AI data‑center actuation live behind sealed, DID‑bound capability objects in other stacks; Prometheus‑Praxis never exports functions that can directly drive them.[file:18]  
  - The PraxisGovernanceKernel and KER engines are pure Rust/ALN components that take observed KER, RoH, lane, and Lyapunov evidence and return Allow/Derate/Stop (and safetopromote) verdicts; they emit qpudatashards and lane verdicts but never send actuator commands.[file:18]

- Governance artifacts: lanes, weights, status, and verdicts.  
  - Lane thresholds and plane weights are defined in PlaneWeights and lane governance ALN shards, setting K/E minima, R and RoH ceilings, and per‑plane weights for each lane (RESEARCH, PILOT, PRODUCTION), including Tree‑of‑Life biases for biodiversity and non‑offsettable planes.[file:18]  
  - SQLite tables such as `lanestatusshard`, `virtalaneverdict`, and `vlatestlanestatus` store per‑shard lane assignments, historical verdicts, and latest status, and are read by governance crates and AI‑safe catalogs to determine admissible operations and AI visibility.[file:18]

- SafeStepGate in practice.  
  - Every proposed state change (e.g., lane promotion, new workload profile, corridor adjustment) is evaluated by SafeStepGate, which checks KER and RoH against lane thresholds and corridor grammar, and enforces Lyapunov non‑increase bands; if any condition fails, the change is rejected or downgraded before it can influence any actuation queue.[file:18]  
  - CI guards, DB triggers (e.g., no‑corridor, no‑build; Lyapunov non‑increase), and Kani proofs for KER/Lyapunov guards together ensure SafeStepGate is consistently applied at code, schema, and runtime layers, preventing accidental downgrades or corridor widening.[file:18]

- Superpower boundaries in AI‑facing surfaces.  
  - The AI‑safe catalog schema (`dbdbagentsafecatalog.sql`) and `vagentsafecatalog` view list only non‑actuating objects (views, patterns, FFI functions, node windows) annotated with KER, RoH, Lyapunov, always‑improve, and stability flags, and explicitly exclude any actuation fields or raw biosignals.[file:18]  
  - `agentsafeconsentguard` rows enforce neurorights and consent envelopes per telemetry family (KER, RoH, LYAP, Eco‑Health), and superpower‑adjacent entries are only visible when ecosafetyrequired is set and governance tokens are present, never as general‑purpose chat tools.[file:18]

- SafeStepGate and lane status integration.  
  - The output of SafeStepGate is materialized into lane verdict tables and diagnostic views (e.g., `laneadmissibleok`, `safetopromoteok` flags in microplastic risk and workload windows), so agents and operators can see which nodes and shards are Always‑Improve and eligible for promotion.[file:18][file:9]  
  - LaneGuard logic in governance crates requires that safetopromote, carbonnegativeok, restorationok, and Lyapunov/ToL constraints all hold before updating `lanestatusshard` to a higher lane; the reverse (downgrades) is allowed whenever safety is threatened, ensuring monotone tightening of protection over time.[file:18]

This governance spine turns the mathematical KER–Lyapunov framework into concrete, DID‑anchored decisions, while superpower boundaries and SafeStepGate ensure that neither AI agents nor human operators can bypass ecosafety corridors or neurorights by going around Prometheus‑Praxis.[file:18][file:9]

## 7. Data sovereignty, neurorights, and identity

### 7.1 Sovereign‑consent architecture

- Data surfaces are split into:
  - Internal rich governance views (full K, E, R, RoH, Tsafe, Vt, lane, corridor, treaty IDs).
  - Thin AI‑safe facades that only expose non‑actuating diagnostics needed for reasoning. [file:18]

- The AI‑safe catalog is materialised as:
  - `db/db_agent_safe_catalog.sql` with:
    - `agentsafecatalog` (core registry of AI‑visible objects).
    - `agentsafediagnostics` (always‑improve and KER diagnostics per catalog entry).
    - `agentsafeconsentguard` (per‑catalog, per‑telemetry‑family gate). [file:18]
  - The AI‑facing view:
    - `CREATE VIEW IF NOT EXISTS vagentsafecatalog AS ...` joining catalog, diagnostics, and consent guard, and filtering:
      - Only KER/ROH/LYAP families with `allowed = 1`.
      - Superpower‑adjacent entries only when `ecosafetyrequired != 0`. [file:18]

- Neurorights corridors are enforced by:
  - A hard exclusion list for:
    - Raw EEG, high‑frequency neural waveforms, fine‑grained biosignal traces tied to identity, and unaggregated lab values. [file:18]
  - A positive‑list for neurally/biologically sensitive metrics only in aggregated, DP‑safe form (e.g., monthly microplastic burden index, daily detox stress, hourly LOW/MED/HIGH cognitive load bands, Eco‑phi DP vectors) and only when a positive `TelemetryVerdict` is present. [file:18]

- Consent and sovereignty are encoded structurally via:
  - `agentsafeconsentguard`:
    - `catalogid`, `telemetryfamily` (`KER`, `ROH`, `LYAP`, `ECOHEALTHAGG`, etc.), `allowed`, `reason`, `updatedat`.
    - Populated by sovereign‑consent engines so any AI‑safe view must join through it to surface those telemetry families. [file:18]
  - Views such as `vagentsafecatalog` explicitly JOIN three consent rows for `KER`, `ROH`, `LYAP` before surfacing KER fields, so neurorights and host envelopes directly control visibility. [file:18]

- Superpower boundaries are enforced by catalog rules:
  - Objects classified as superpower‑adjacent (e.g., HeatWaterTree, nanoswarm routing, Neurobiome Mesh) are:
    - Present in `agentsafecatalog` only as diagnostic surfaces.
    - Always flagged `superpoweradjacent = 1`.
    - Only visible to AI when `ecosafetyrequired != 0` (i.e., there is an explicit governance token or gate). [file:18]
  - Actuation parameters, controller gains, or plans are never present in any AI‑facing view; only read‑only diagnostics such as RoH, Tsafe, Vt, guard verdicts, corridor IDs, and treaty IDs are allowed. [file:18]

- “No corridor, no build” remains a hard invariant:
  - Core ecosafety schemas and triggers reject writes that push risk coordinates beyond corridor bands or increase Lyapunov residuals.
  - AI patterns are forced to query through vetted views (e.g., `vcyboworkloadnodewindow`, `vmachineblastradius`), not raw staging tables, so sovereign‑consent and corridor enforcement are never bypassed. [file:18]

- Knowledge factor / eco‑impact / risk:
  - Knowledge factor K: High, because the architecture reuses existing KER/Lyapunov tables and extends them with explicit AI‑safe catalog and consent guards, clarifying what agents can see. [file:18]
  - Eco‑impact value E: High, because excluding raw neural and identity‑sensitive telemetry while allowing aggregated, DP‑safe metrics reduces exploitation risk while enabling restorative reasoning. [file:18]
  - Risk‑of‑harm R: Moderate, mainly from mis‑classification of telemetry families or misconfigured consent rows, mitigated by CI checks ensuring no raw neural families are marked `allowed = 1`. [file:18]

### 7.2 Contribution ledgers, ecocredit semantics, and ALN policy shards

- Contribution and workload evidence:
  - Cyboquatic machinery energy/carbon/restoration evidence is captured in:
    - `cyboquaticworkloadledger` (in `db/db_cyboquatic_blastradius_spine.sql`): per‑workload `ereqj`, `esurplusj`, `rcarbon`, `rbiodiv`, `vtbefore`, `vtafter`, `decision`, `lane`, `region`, `evidencehex`, and `signingdid`. [file:18]
  - Blast‑radius effects are captured in:
    - `blastradiuslink`: `sourcetype`, `sourceid`, `targettype`, `targetid`, `impacttype`, `impactscore`, `vtsensitivity`, with unique constraints per (source, target, impacttype). [file:18]

- Ecocredit semantics:
  - Higher‑level views (e.g., `vmachineblastradius`, `vcyboworkloadnodewindow`) provide per‑machine/per‑node/per‑window summaries of:
    - Energy usage, carbon risk, Lyapunov deltas, acceptance rates, and ecopenalty metrics, usable as a basis for ecocredit or eco‑penalty allocations per region or lane. [file:18]
  - Planned `ecopenalty` metrics combine:
    - Plane weights, impact scores, and normalized risk factors to rank worst offenders; these can drive corridor‑level ecocredit rules (e.g., more credits for nodes with low ecopenalty and `deltavt < 0`). [file:18]

- ALN policy shards:
  - Ecological blast‑radius shards:
    - `qpudatashards/particles/CyboquaticBlastRadiusShard2026v1.aln`:
      - Declares fields like `maxcarbonradius`, `maxbiodivradius`, `vtradiussum`, lane, region, and corridor‑level CORRIDORVAR thresholds for `deltavt` and `maxcarbonradius` with SAFE/GOLD/HARD bands. [file:18]
      - Binds to Bostrom DID via `ALN_SPECHASH_HEX` and DID anchor. [file:18]
  - Microplastic risk policy:
    - `db/db_cyboquatic_microplastic_risk_index.sql`:
      - Table `cyboquaticmicroplasticriskindex` includes `microplasticloadmgm3`, `ecoperjoule`, `restorationscore`, KER dimensions, Lyapunov residuals, RoH, and governance flags (`carbonnegativeok`, `restorationok`, `laneadmissibleok`, `safetopromoteok`). [file:18]
      - A trigger `trg_microplastic_ker_invariants` ensures:
        - `kerscore ≈ k + e − r`.
        - Non‑negative microplastic load.
        - `vtafter <= vtbefore`. [file:18]
    - Facade view `vcyboquaticmicroplasticriskfacade` exposes only non‑actuating fields to AI. [file:18]

- Contribution ledgers as ecocredit backbone:
  - Each workload, shard blast‑radius, and microplastic risk row forms a ledger entry that can be transformed via ALN shards (e.g., `CyboquaticBlastRadiusShard2026v1`, microplastic ALN) into ecocredit transactions anchored to a DID and corridor. [file:18]
  - This enables future `ecowealth`‑like tables and lanes where rewards are allocated based on sustained `alwaysimprove` flags and positive KER evolution per node/region. [file:18]

- Knowledge factor / eco‑impact / risk:
  - Knowledge factor K: High, because schemas and ALN specs provide a consistent numerical language for blast radius, workloads, and microplastics aligned with KER. [file:18]
  - Eco‑impact value E: High, because the structure directly enables ecocredit scoring that favours carbon‑negative, restorative patterns per machine/node window. [file:18]
  - Risk‑of‑harm R: Moderate, mostly from mis‑weighted ecocredit logic or misconfigured plane weights, mitigated by formal Kani harnesses on scoring kernels. [file:18]

### 7.3 DID and ALN anchors for identity and rewards

- Bostrom identity anchors:
  - ALN shards explicitly embed:
    - `Anchor bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` or similar DID tag to bind specifications and shards to the brain‑bound Bostrom identity. [file:18]
  - Examples:
    - `NeuromorphicCorridorAlign2026v1.aln` under `Prometheus-Praxis/specs` anchors neuromorphic material–corridor rules to the DID and EcoFort grammar. [file:18]
    - `CyboquaticBlastRadiusShard2026v1.aln` likewise carries a hex spechash bound to the DID. [file:18]

- EcoFort grammar and corridor mapping:
  - ALN specs define:
    - Typed `materialprofile` or `plot` particles.
    - `corridor` variants with power density, contact rules, and plane binding (e.g., `LOWENERGYBIOCOMPATIBLE`, `HIGHENERGYCONTAINED`). [file:18]
    - Alignment blocks (`align NeuromorphicMaterialProfile -> EcosafetyCorridor as NeuromorphicCorridorMap`) formally map profiles to corridors with `risk` labels and eco‑notes. [file:18]
  - Rust and Kotlin codegen sections turn these ALN mappings into:
    - Rust enums in crates like `neuromorphiccorridoralign` with serde derives.
    - Kotlin sealed hierarchies under `org.prometheuspraxis.neuromorphic`. [file:18]

- Authorship and reward tracking:
  - Contribution DIDs:
    - Tables such as `cyboquaticworkloadledger` include `signingdid` to record which governance identity signed a decision. [file:18]
    - Microplastic risk index rows can carry `didroot` corresponding to the ALN spec’s DID anchor. [file:18]
  - Hex‑stamping:
    - Repo index migrations (e.g., `db/db_repo_index_restoration_cyboquatic.sql`) register ALN and SQL files with `repofile` entries, enabling `DefinitionRegistry` and hex anchor manifests like `PHX_HEX_ANCHORS.md` to track authorship and version. [file:18]
  - Eco‑rewards:
    - KER and always‑improve flags in `agentsafediagnostics` and eco‑ledger tables can be aggregated into eco‑reward or ecowealth tables tied to contributor DID, corridor, and lane. [file:18]

- Knowledge factor / eco‑impact / risk:
  - Knowledge factor K: High, because DID and ALN anchoring provide a verifiable chain from spec to code to ledger, improving provenance. [file:18]
  - Eco‑impact value E: High, because connecting KER and always‑improve flags to DID‑anchored shards allows reward systems that explicitly favour restorative work. [file:18]
  - Risk‑of‑harm R: Low–moderate, mainly from mis‑binding of shards to wrong DIDs or corridor tags, mitigated by CI checks on `repoindex` bindings and ALN spec hashes. [file:18]

---

## 8. Repository architecture and planes

### 8.1 Monorepo structure and Cargo workspace

- Authoritative monorepo:
  - Target: `github.com/mk-bluebird/Prometheus-Praxis`, formerly a constellation of Doctor0Evil eco repositories, now the single source of truth for EcoNet and Prometheus‑Praxis work. [file:18]
  - Eco‑restoration focused subtree: `ecorestoration_shard` (now folded into the mono‑repo), holding SQLite schemas, ALN shards, and Cyboquatic machinery spines. [file:18]

- Cargo workspace layout:
  - Workspace root `Cargo.toml` lists crates such as:
    - `crates/prometheuspraxis` – core governance and execution kernel. [file:18]
    - `crates/prometheuspraxisai` – AI‑facing, non‑actuating ecosafety/always‑improve scoring crate, with FFI and CPP engine adapters. [file:18]
    - `crates/prometheus-praxis-cyboquatic` – Cyboquatic coupling layer onto EcoNet governance. [file:18]
    - `crates/prometheus-praxis-lyapunov-guard` – Lyapunov guard/invariant crate with Kani harnesses. [file:18]
    - `crates/cyboquatic-ecosafety-core`, `crates/cyboquatic-core`, `crates/econet-governance-spine` – ecosafety, recognition/index, and governance spine crates. [file:18]
  - Workspace‑level dependency pinning (e.g., `serde = 1.0.203`, `serde_json = 1.0.120`) ensures all crates use consistent versions required for formal verification and diagnostic tooling. [file:18]

- Cross‑language adapters:
  - C/CPP adapters:
    - Under `prometheuspraxisai` (e.g., `src/engine.cpp`, FFI shims in `src/lib.rs`) and a dedicated `fog-heuristic-drainage` directory for a C microservice feeding a Rust hydrology plane via Unix sockets. [file:18]
  - Kotlin/Java:
    - `ai-datacenter-node-agent` Java code monitors `AiDatacenterNode2026v1` metrics and alerts via Discord, governed by Rust/ALN invariants fetched via a Unix socket bridge. [file:18]
  - Lua tools:
    - Located in `tools/` (e.g., `regionrepresentationshardvisualizer.lua`, `sunflowerbeecorridorledgerverifier.lua`, `cyboquaticmicroplasticrisk.lua`) providing CI‑friendly SVGs, ledger checks, and diagnostic summaries. [file:18]

- Knowledge factor / eco‑impact / risk:
  - Knowledge factor K: High, because the workspace structure is explicitly documented in repo notes, allowing precise crate‑role reasoning. [file:18]
  - Eco‑impact value E: High, because consolidation into a single mono‑repo reduces drift and improves the ability to enforce eco‑invariants via shared CI and ALN. [file:18]
  - Risk‑of‑harm R: Low, primarily from mis‑wiring crate dependencies, mitigated by `DefinitionRegistry` and CI checks over workspace members. [file:18]

### 8.2 Planes and shards: diagnostic, governance, observability, agent

- Planes and their main artefacts:

  - Governance plane:
    - Implemented primarily by:
      - `crates/prometheuspraxis` (macroscale execution kernel returning `Allow/Derate/Stop`). [file:18]
      - `crates/econet-governance-spine` (typed access to views like `vresidualkernel`, `vplaneweights`, `vshardblast`, `vlaneadmissibility`). [file:18]
    - Relies on SQLite schemas:
      - `db/db_ecosafety_workload_window.sql`, `db/db_econet_cyboquatic_index_2026v1.sql`, `db/db_blastradius_spine.sql`, and related ecosafety corridors. [file:18]

  - Diagnostic plane:
    - Backed by SQLite shards and rich internal views:
      - `cyboquaticworkloadledger`, `blastradiuslink`, cyboquatic machinery registries, microplastic risk index, and derived diagnostics. [file:18]
      - Views like `vmachineblastradius`, `vcyboworkloadnodewindow`, `vcyboquaticecoperjoule`, `vcyboquaticrestore`. [file:18]
    - Exposed via non‑actuating Rust crates:
      - `crates/cyboquatic-blastradius-spine` FFI returning JSON snapshots for tooling without exposing device APIs. [file:18]

  - Observability plane:
    - Rust crates (e.g., `prometheus-praxis-lyapunov-guard`, `cyboquatic-core` with `metrics` feature) emit:
      - KER, RoH, Tsafe, Vt metrics as gauges/counters/histograms. [file:18]
      - Snapshot APIs that compose these into JSON for dashboards and AI facades. [file:18]
    - Lua visualisers (e.g., region coverage SVG, bee corridor Merkle checks) deliver CI/README‑grade observability artefacts. [file:18]

  - Agent plane:
    - SQL plus ALN:
      - `agentsqlpattern` table defines curated, governance‑approved SQL templates with lane scopes, capability levels, and risk ceilings. [file:18]
      - `vagentsafecatalog` view and its ALN mirror `econet.agent.function.catalog.v1.aln` define which views/FFI/patterns an AI may use, with KER and eco‑scores per object. [file:18]
    - Rust/ALN:
      - `prometheuspraxisai` provides `alwaysimprove` scoring kernels; outputs flow into `agentsafediagnostics` to rank tools and patterns. [file:18]
    - Shell/Lua tooling:
      - AI‑oriented scripts fetch `sqltext` from `agentsqlpattern` and map `objectid` from `vagentsafecatalog` to tools, ensuring agents only invoke pre‑vetted queries. [file:18]

- Plane–artefact mapping table:

  | Plane        | SQLite / ALN artefact                                                                 | Rust crate(s)                                           | AI/Tool surface                                            |
  |-------------|----------------------------------------------------------------------------------------|---------------------------------------------------------|-----------------------------------------------------------|
  | Governance  | `db_ecosafety_workload_window.sql`, ecosafety corridor ALN shards                      | `prometheuspraxis`, `econet-governance-spine`           | Lane guards, AlwaysImprove verdicts                       |
  | Diagnostic  | `db_cyboquatic_blastradius_spine.sql`, `db_cyboquatic_machinery_spine.sql`, microplastic index | `cyboquatic-blastradius-spine`, `cyboquatic-ecosafety-core` | `vmachineblastradius`, `vcyboworkloadnodewindow`         |
  | Observability | KER/Vt schemas, plane weights ALN                                                    | `prometheus-praxis-lyapunov-guard`, `cyboquatic-core`   | JSON snapshots, Prometheus‑style metrics                  |
  | Agent       | `agentsafecatalog`, `agentsafediagnostics`, `agentsqlpattern`, `vagentsafecatalog`, `econet.agent.function.catalog.v1.aln` | `prometheuspraxisai` and EcoNet index crates           | Tool catalogs, safe SQL patterns, FFI handles             | [file:18]

- Knowledge factor / eco‑impact / risk:
  - Knowledge factor K: High, because the plane decomposition aligns explicitly with documented invariants and existing schemas/crates. [file:18]
  - Eco‑impact value E: High, because agents operate strictly on diagnostic/observability surfaces, making it easier to measure and improve eco‑impact without risking actuation. [file:18]
  - Risk‑of‑harm R: Low–moderate, mainly from mis‑classification of an object’s plane (e.g., accidentally registering an actuation‑adjacent tool as diagnostic), mitigated by `DefinitionRegistry`, `roleband`, `nonactuating_only` flags, and CI checks over `vagentsafecatalog`. [file:18]

 ## 9. Non‑actuating Rust and ALN conventions

### 9.1 Rust constraints and invariants

- Core constraints for all Prometheus‑Praxis and EcoNet crates:
  - `edition = "2024"` and `rust-version = "1.85"` must be set in `Cargo.toml` (workspace and member crates). [file:18]
  - `#![forbid(unsafe_code)]` at crate root: all governance, ecosafety, and AI‑facing crates are strictly safe Rust; any low‑level bindings live in dedicated, tightly scoped adapter crates. [file:18]
  - Clippy is run in “deny” mode for warnings on these crates (e.g., `-D warnings` in CI), so code must be warning‑free to merge. [file:18]

- Kani requirements:
  - `kani-verifier = "0.67"` is a mandatory dev‑dependency for all safety‑critical kernels (e.g., `prometheuspraxisai`, `prometheuspraxisker`, `prometheus-praxis-lyapunov-guard`). [file:18]
  - At least three classes of properties are proven:
    - Memory safety (no panics, no overflows, no UB) on core scoring and guard functions. [file:18]
    - Lane invariants: if K, E, R are within lane thresholds and RoH and Lyapunov respect ceilings, governance must not return `Stop`. [file:18]
    - Always‑improve monotonicity: outputs never lower safety floors, and `safetopromote` cannot be `true` when RoH, Vt, or corridor thresholds are violated. [file:18]

- Non‑actuating spine:
  - All Rust crates in the governance/diagnostic band (e.g., `econet-governance-spine`, `cyboquatic-blastradius-spine`, `prometheuspraxisai`) are read‑only over SQLite and ALN; they:
    - Read logs, KER windows, Lyapunov residuals, blast‑radius, ecoper‑joule, etc. [file:18]
    - Compute scores, flags, or KER‑derived summaries. [file:18]
    - Emit JSON, ALN shards, or write back evidence rows only (never actuation commands). [file:18]
  - Actuation (Perknos‑Nexus, nanoswarm routing, macro‑health logistics, etc.) is always outside this repo and never exposed as a Rust function callable from AI‑facing crates. [file:18]

- Always‑improve kernel pattern:
  - Implemented in `crates/prometheuspraxisai/src/alwaysimprove.rs` with:
    - `AlwaysImproveScore { score, safetopromote }` as the core result type. [file:18]
    - `AlwaysImproveConfig { vref, maxdeltav, wk, we, wr }` for weighting K/E/R residuals and Lyapunov deltas. [file:18]
  - Main function:
    - `pub fn compute_always_improve_score(lane: ActionLane, ker: KerOutput, roh: RohSnapshot, lyap: LyapunovResidualSnapshot, cfg: AlwaysImproveConfig, roh_ceiling_global, kmin_*, emin_*, rmax_*) -> AlwaysImproveScore` with:
      - Hard early exits when RoH exceeds ceiling or Lyapunov delta exceeds `maxdeltav`. [file:18]
      - Lane‑specific K/E/R minima/maxima enforced before `safetopromote` may be `true`. [file:18]

- Rust configuration conventions:
  - Governance crates expose config via:
    - Workspace metadata or ALN (e.g., `workspace.metadata.ker.residuals`) instead of ad‑hoc constants, so Kani harnesses can reason about the same thresholds. [file:18]
  - All AI‑facing FFI layers (C/CPP, Kotlin, Lua) are thin veneers over these Rust functions; they must:
    - Never introduce actuation logic.
    - Only serialize/deserialize evidence, KER summaries, and `AlwaysImproveScore`. [file:18]

### 9.2 ALN patterns, function meta, and governance shards

- ALN function metadata:
  - Function‑level ALN shards (e.g., `ppx.function.meta.v1.aln`) encode:
    - `functionid`, `domain` (`ecosafety`, `cyboquatic`, `governance`), `lane` (`RESEARCH`, `PILOT`, `PRODUCTION`). [file:18]
    - `nonactuating = 1` for all AI‑exposed functions. [file:18]
    - `ecosafetyrequired` flag when a function is near superpowers or corridors requiring extra gates. [file:18]
  - These meta shards drive:
    - `agentsafecatalog.ecosafetyrequired` and `superpoweradjacent` fields. [file:18]

- Ecosafety policies:
  - ALN shards such as:
    - `ecosafety.nonactuatingworkload.v1.aln` describe workloads that are guaranteed non‑actuating and their expected metrics. [file:18]
    - `ecosafety.riskvector.v2.aln` defines risk vector components (e.g., `rcarbon`, `rbiodiv`, topology risks) and how they map into corridors. [file:18]
  - Policies enforce:
    - Bounding of K/E/R, RoH, and Lyapunov residuals in \([0,1]\).
    - Explicit corridor assignments (SAFE/GOLD/HARD bands) for each dimension. [file:18]

- Task‑list ALN (`PrometheusPraxisCodingTaskList2026v1.aln`):
  - Encodes coding tasks and governance work items as first‑class ALN particles:
    - `taskid`, `category` (`SQL`, `RustKernel`, `ALNSpec`, `CIGuard`), `lanetarget`, `kerimpact`, `rohimpact`, `ecoscore`, `riskflag`. [file:18]
  - Allows:
    - Prioritisation of tasks by knowledge factor and eco‑impact.
    - CI to enforce that high‑risk tasks (e.g., touching KER kernels) must carry Kani property proofs before merging. [file:18]

- Governance flag shards:
  - Shards that encode governance state and flags, for example:
    - Lane rules (`vlanepromotionhistory`, `lanestatusshard`): how shards move from `RESEARCH` → `EXPPROD` → `PROD`. [file:18]
    - KER upgrade guards (`KerUpgradeGuard` ALN) capturing `K_new ≥ K_old`, `E_new ≥ E_old`, `R_new ≤ R_old`. [file:18]
    - Always‑improve meta (`alwaysimprove.policy.2026v1.aln`) tying score thresholds to lane promotions and eco‑rewards. [file:18]
  - These are referenced by:
    - Rust guard crates (`econet-governance-spine`, `prometheuspraxisai`) and CI harnesses that validate shards and workloads before lane changes. [file:18]

---

## 10. SQL and SQLite governance spines

### 10.1 Core governance schemas

- Plane weights:
  - Plane‑weight shards and tables (e.g., `PlaneWeightsShard2026v1` and its SQL materialization) encode:
    - Planes such as `ENERGY`, `CARBON`, `BIODIVERSITY`, `HYDRAULIC`, each with weights and risk coefficients. [file:18]
  - Used to:
    - Compute plane‑weighted blast‑radius \(B_\text{weighted} = \sum_p w_p \cdot \text{impactscore}_p\). [file:18]
    - Derive ecopenalty metrics per machine or shard. [file:18]

- Blast‑radius:
  - `blastradiuslink` table (in `db/db_cyboquatic_blastradius_spine.sql`) with:
    - `sourcetype` (e.g., `SHARD`, `MACHINE`), `sourceid`.
    - `targettype` (e.g., `NODE`, `REGION`), `targetid`.
    - `impacttype` (`CARBON`, `BIODIVERSITY`, etc.), `impactscore`, `vtsensitivity`. [file:18]
  - Diagnostic views:
    - `vshardblastradius` summarises blast‑radius per shard. [file:18]
    - `vmachineblastradius` summarises per machine/node with governance filters:
      - Excludes stale lanes (`expiresutc < now`).
      - Drops non‑offsettable planes when any corridor violation exists. [file:18]

- Event windows:
  - Conceptualised via:
    - Tables such as `windowgdbmetrics`, `evolutionepoch`, or a dedicated `workloadwindow` table. [file:18]
    - Deterministic window assignment function (e.g., `windowid = floor((timestamp - t0)/windowsize)` per region), encoded in SQL and/or Rust. [file:18]
  - Key view:
    - `vcyboworkloadnodewindow` with columns:
      - `nodeid`, `region`, `lane`, `windowstartutc`, `windowendutc`.
      - `energyjtotal`, `carbonriskavg`, `vtbeforeavg`, `vtafteravg`, `deltavt`.
      - `acceptedcount`, `rejectedcount`, `reroutedcount`.
      - Flags like `carbonnegativeok`, `restorationok`, `alwaysimproveok`. [file:18]

- Lane status and KER / Lyapunov windows:
  - Lane views:
    - `lanestatusshard`, `vlaneadmissibility`, `vlanepromotionhistory`:
      - Encode current lane per shard, admissibility verdicts, TTL/staleness rules, and historical promotions. [file:18]
  - KER and Lyapunov:
    - Windows of K/E/R and Vt are held in:
      - Residual snapshots and KER tables accessed via `vresidualkernel` and related views. [file:18]
    - Used by:
      - CI guards (`KerUpgradeGuard`, LaneGuard, `Mt6883Guard`) to enforce KER monotonicity and no corridor/no build. [file:18]

### 10.2 AI‑safe catalog and tool surfacing

- Core tables (from `db/db_agentsafecatalog.sql`):

  - `agentsafecatalog`:
    - Fields:
      - `catalogid` (stable ID, e.g., `ASC-000001`).
      - `kind` (`TOOL`, `NODE`, `WINDOW`, `VIEW`, `SQLPATTERN`).
      - `name`, `description`, `domain` (ecosafety/cyboquatic/governance).
      - `lane` (`RESEARCH`, `PILOT`, `PRODUCTION`).
      - `corridorid`, `smartchainid`.
      - `ecosafetyrequired` (mirrors ALN function meta).
      - `superpoweradjacent` (1 if near sealed superpowers).
      - `consentstreamid`, `mcptoolname`.
      - `createdat`, `updatedat`. [file:18]

  - `agentsafediagnostics`:
    - Fields:
      - `catalogid` (FK to `agentsafecatalog`).
      - `kscore`, `escore`, `rscore` (normalized K/E/R 0..1).
      - `rohscalar`, `vcurrent`, `vnext`, `lyapdelta`.
      - `alwaysimprove` (0..1), `safetopromote` (0/1).
      - `stableflag` (0/1), `marginclass` (`safe`, `tight`, `violated`).
      - `lane`, `corridorid`, `updatedat`. [file:18]
    - Populated only by Rust jobs (e.g., `prometheuspraxisai` batch): AI agents never write to this table. [file:18]

  - `agentsafeconsentguard`:
    - Fields:
      - `catalogid` (FK).
      - `telemetryfamily` (`KER`, `ROH`, `LYAP`, `ECOHEALTHAGG`, etc.).
      - `allowed` (0/1).
      - `reason`, `updatedat`. [file:18]
    - Populated by sovereign‑consent engines; any row with `allowed = 0` blocks those telemetry families from AI views. [file:18]

- View `vagentsafecatalog`:

  - Combines:
    - `agentsafecatalog` (identity, lane, domain, superpower adjacency).
    - `agentsafediagnostics` (KER/Lyapunov/always‑improve metrics).
    - `agentsafeconsentguard` (per‑family consent). [file:18]
  - Filters:
    - Only entries where required telemetry families (`KER`, `ROH`, `LYAP`) are `allowed = 1`. [file:18]
    - Excludes raw biosignal families and neurorights‑sensitive telemetry (EEG, high‑frequency neural/biosignal traces). [file:18]
    - Ensures any `superpoweradjacent = 1` entry is visible only when `ecosafetyrequired != 0` and lane is diagnostic (e.g., `RESEARCH`). [file:18]
  - AI tooling usage:
    - Treats each row as a safe tool/object:
      - `objectid` = `VIEW/dbname/viewname` or `FFI/crate/symbol` or `SQLPATTERN/patternid`. [file:18]
      - Includes KER and always‑improve fields so tools can filter to `safetopromote = 1`, `alwaysimprove ≥ threshold`, or low `rohscalar`. [file:18]

- Relation to other governance views:
  - `vcyboworkloadnodewindow` and `vmachineblastradius` are referenced by:
    - SQL patterns in `agentsqlpattern` (e.g., “list nodes that improved Vt and reduced carbon last week”). [file:18]
    - AI‑safe catalog entries whose diagnostics are derived from these views and then written into `agentsafediagnostics`. [file:18]
  - This keeps AI agents:
    - Operating purely on vetted, lane‑filtered snapshots.
    - Never directly touching raw ledger tables or pre‑trigger staging data. [file:18]

## 11. Always‑Improve kernel and scoring

### 11.1 Rust scoring kernel

- Kernel location and role:
  - Implemented in `crates/prometheuspraxisai/src/alwaysimprove.rs` as a non‑actuating scoring kernel for diagnostic shards and node windows. [file:18]
  - Consumes existing KER, RoH, and Lyapunov snapshots from governance crates (`prometheuspraxisker`, `prometheuspraxisgovernance`) rather than recomputing them. [file:18]

- Core types:
  - `AlwaysImproveScore`:
    - Fields: `score` (bounded scalar in \([0,1]\)), `safetopromote: bool`. [file:18]
    - Interpreted as “how strong is the evidence that this object/window is improving or at least not degrading under KER/Lyapunov.” [file:18]
  - `AlwaysImproveConfig`:
    - Fields: `vref`, `maxdeltav`, `wk`, `we`, `wr`. [file:18]
    - Encodes Lyapunov reference value, maximum allowed Lyapunov increase, and weights for K, E, and R residuals. [file:18]
    - Default values are tied to workspace metadata (`workspace.metadata.ker.residuals`) so changes propagate via ALN/metadata rather than hard‑coded constants. [file:18]

- Residuals and scoring:
  - Helper `ker_residuals_for_lane`:
    - Takes `ActionLane`, `KerOutput`, and lane thresholds (`kmin_*`, `emin_*`, `rmax_*`) and returns residuals \((r_K, r_E, r_R)\) where:
      - For K,E: residual is \(K_\text{target} - K\), clamped at 0 if \(K ≥ K_\text{target}\). [file:18]
      - For R: residual is \(R - R_\text{target}\), clamped at 0 if \(R ≤ R_\text{target}\). [file:18]
  - Helper `lyapunov_delta`:
    - Computes \(ΔV = V_\text{next} - V_\text{current}\) from `LyapunovResidualSnapshot`, with inputs clamped into \([0,1]\). [file:18]
  - Main function `compute_always_improve_score`:
    - Inputs:
      - `lane: ActionLane`, `ker: KerOutput`, `roh: RohSnapshot`, `lyap: LyapunovResidualSnapshot`.
      - `cfg: AlwaysImproveConfig`.
      - Global and lane thresholds: `roh_ceiling_global`, `kmin_*`, `emin_*`, `rmax_*`. [file:18]
    - Steps:
      - Clamp RoH; if `rohscalar > roh_ceiling_global`, return `score = 0`, `safetopromote = false`. [file:18]
      - Compute `deltav = lyapunov_delta(lyap)`; if `deltav > maxdeltav`, return `score = 0`, `safetopromote = false`. [file:18]
      - Compute residuals `(rK, rE, rR)` via `ker_residuals_for_lane`. [file:18]
      - Combine: `combined = wk*rK + we*rE + wr*rR`, `score = clamp01(1 − combined)`. [file:18]
      - Set `safetopromote = score ≥ 0.7` (or ALN‑driven threshold) and only if lane K/E minima and R maxima are satisfied. [file:18]

- Kani harnesses and monotonicity:
  - Kani proof modules (e.g., `kaniharnesses`) assert:
    - If `K ≥ K_min(lane)`, `E ≥ E_min(lane)`, `R ≤ R_max(lane)`, `RoH ≤ roh_ceiling`, and `ΔV ≤ maxdeltav`, then `safetopromote` is never false. [file:18]
    - If `ΔV > maxdeltav` or `carbonrisk > 0.13` or `RoH > roh_ceiling`, `safetopromote` is always false. [file:18]
  - This formalises monotone evolution:
    - Safety floors cannot be lowered, and always‑improve cannot “bless” a shard that violates KER/RoH/Lyapunov constraints. [file:18]

### 11.2 Integration into diagnostics views

- Diagnostics table surfacing:
  - `agentsafediagnostics` table (in `db/db_agentsafecatalog.sql`) holds:
    - `kscore`, `escore`, `rscore` (normalized K/E/R).
    - `rohscalar`, `vcurrent`, `vnext`, `lyapdelta`.
    - `alwaysimprove`, `safetopromote`.
    - `stableflag` (0/1) and `marginclass` (`safe`, `tight`, `violated`). [file:18]
  - Populated by Rust jobs:
    - Batch tools read from rich views (e.g., `vrichcyboshardstate`, `vcyboworkloadnodewindow`), call `compute_always_improve_score`, then update/insert into `agentsafediagnostics`. [file:18]

- AI‑safe catalog view:
  - `vagentsafecatalog` joins:
    - `agentsafecatalog` (what the object is).
    - `agentsafediagnostics` (how safe/improving it is).
    - `agentsafeconsentguard` (which telemetry families are allowed). [file:18]
  - Fields surfaced to agents:
    - `alwaysimprove`, `safetopromote`, `stableflag`, `marginclass`, along with K/E/R and Lyapunov scalars. [file:18]
  - Usage patterns:
    - Agents filter to `safetopromote = 1` and `marginclass = 'safe'` when searching for exemplar shards or nodes. [file:18]
    - Governance CI uses the same table to require `safetopromote = 1` and `carbonnegativeok` and `restorationok` before lane upgrade to `PROD`. [file:18]

---

## 12. Function registry and CI enforcement

### 12.1 Immutable function contracts (ppx.function.meta.v1.aln)

- Canonical registry:
  - `ppx.function.meta.v1.aln` is the authoritative ALN registry for callable functions across the Prometheus‑Praxis constellation. [file:18]
  - Each entry includes:
    - `functionid` (stable identifier).
    - `domain` (`ecosafety`, `cyboquatic`, `governance`, etc.).
    - `lane` (`RESEARCH`, `PILOT`, `PRODUCTION`). [file:18]
    - `corridorid` / corridors touched.
    - Capitals or planes touched (e.g., carbon, biodiversity, hydraulic). [file:18]
    - `actuationflag` or `actuationcapability` (must be `NONE` for AI‑safe functions). [file:18]
    - `superpoweradjacent` and `ecosafetyrequired` for functions near sealed kernels. [file:18]

- Binding to runtime:
  - Fields are mirrored into:
    - `agentsafecatalog` (`ecosafetyrequired`, `superpoweradjacent`). [file:18]
    - `agentsqlpattern` (`roleband`, `lanescope`, `actuationflag`). [file:18]
  - Ensures:
    - Any FFI symbol, SQL pattern, or CLI command exposed to AI has a corresponding immutable function contract in ALN. [file:18]

### 12.2 CI guardrails and enforcement

- `functionmetacicheck.py`:
  - Python CI tool that:
    - Scans the repo for governance artefacts (`.aln`, `.sql`, `.rs`) in configured directories. [file:18]
    - Ensures each callable function or tool has a `functionid` row in `ppx.function.meta.v1.aln`. [file:18]
    - Verifies consistency between ALN contracts and:
      - `agentsafecatalog` entries (kind, domain, lane, actuation capability).
      - `agentsqlpattern` entries (actuation flag must be 0 for AI‑safe patterns). [file:18]
    - Fails CI with a clear list of unregistered or inconsistent functions (“no‑registry, no‑build”). [file:18]

- Non‑actuation CI:
  - Guards that ensure AI‑visible surfaces remain non‑actuating:
    - Check `actuationflag = 0` for all entries referenced in `vagentsafecatalog` and `econet.agentfunctioncatalog.v1.aln`. [file:18]
    - Confirm no function with `actuationcapability != NONE` is tagged as `AICHAT` or included in AI‑safe catalogs. [file:18]
    - Validate that any `superpoweradjacent = 1` entry in `agentsafecatalog` has `ecosafetyrequired != 0`, meaning extra governance gating. [file:18]

- Governance‑flag CI:
  - Additional scripts enforce:
    - Governance flags in ALN (e.g., KER upgrade guards, lane promotion rules) match lane behaviour in SQL views like `vlaneadmissibility`, `vlanepromotionhistory`. [file:18]
    - `safetopromote`, `carbonnegativeok`, and `restorationok` flags are respected before CI allows lane upgrades or PROD tagging for shards. [file:18]
  - Together these ensure:
    - Functions cannot silently change domains, lanes, or actuation capabilities.
    - All callable surfaces used by AI or automation are explicitly declared, non‑actuating, corridor‑bounded, and backed by ALN contracts and Rust/Kani invariants. [file:18]

## 13. Eco‑Credit kernel (output‑only reward semantics)

### 13.1 EcoCredit kernel and minting rules

- EcoCredit type:
  - Represented as a ledger row and/or ALN particle bound to a steward, shard, and region, with fields like `ecocreditid`, `stewardid`, `shardid`, `region`, `amount`, `vwindow`, `rcarbon`, and evidence hex bound to your Bostrom DID. [file:18]
  - Lives in the governance spine alongside EcoWealth/EcoUnit tables and is produced by non‑actuating Rust kernels over diagnostic views (workloads, blast‑radius, KER, restoration metrics). [file:18]

- Inputs for minting:
  - Lyapunov residuals:
    - Use windowed metrics such as `meanvtbefore`, `meanvtafter`, and `deltavt` from `vcyboworkloadnodewindow` or analogous Phoenix views. [file:18]
    - Require \(ΔV = \text{meanvtafter} - \text{meanvtbefore} ≤ 0\) over the reward window. [file:18]
  - Carbon risk:
    - Use `meanrcarbon` (0..1) or per‑workload `rcarbon`, with a hard ceiling (e.g., `meanrcarbon ≤ 0.13`) as already used for always‑improve gating. [file:18]
  - Workload energy:
    - Use energy and surplus metrics (`totalreqj`, `totalsurplusj`, `Eeff = totalsurplusj / totalreqj`) from workload window views. [file:18]

- Minting formula (example pattern consistent with your grammar):
  - Compute an eco‑credit score per shard/window:
    - \(S_\text{eco} = f(E_\text{eff}) - g(r_\text{carbon}) - h(R_\text{planes})\) where:
      - \(E_\text{eff}\) is energy efficiency from workload windows.
      - \(r_\text{carbon}\) is mean carbon risk.
      - \(R_\text{planes}\) is a combined blast‑radius risk from plane weights and `blastradiuslink`. [file:18]
  - Only mint if:
    - \(ΔV ≤ 0\), `meanrcarbon` under corridor, and no non‑offsettable plane exceeds its corridor band. [file:18]
  - Map score to credits:
    - `credits = max(0, floor(Scale * S_eco))`, with per‑lane caps enforced in Rust and encoded in ALN policy shards (e.g., `ecocredit.policy.phoenix.v1.aln`). [file:18]

- Kernel properties:
  - Implemented as a pure Rust function in a governance crate (e.g., `crates/prometheuspraxis-ecocredit`), which:
    - Reads from views like `vcyboworkloadnodewindow`, `vmachineblastradius`, `vresidualkernel`. [file:18]
    - Computes `EcoCreditMintReport` objects with `amount`, `window`, `evidencehex`, `safetomint` flag. [file:18]
    - Optionally writes an output‑only `ecocreditledger` table but never touches machinery, controllers, or plane weights. [file:18]
  - Kani harnesses:
    - Prove that credits are zero whenever any hard constraint (Lyapunov, carbon ceiling, corridor) is violated. [file:18]
    - Prove non‑negativity and monotonicity: if inputs improve (`ΔV` more negative, lower `rcarbon`), `S_eco` cannot decrease. [file:18]

### 13.2 Non‑feedback constraint

- No offsetting of physical risk:
  - EcoCredits are strictly informational rewards; they:
    - Do not appear in any controller input tables.
    - Do not reduce `R` coordinates, RoH ceilings, or blast‑radius metrics. [file:18]
  - KER, plane weights, and RiskCoords stay entirely governed by physical evidence tables (blastradius, workloads, ecocorridorvar) and Kani‑verified Rust kernels. [file:18]

- No influence on plane weights or controllers:
  - EcoCredits:
    - Are not part of `planeweights`, `ecocorridorvar`, or any `Mt6883Guard` or LaneGuard input. [file:18]
    - Cannot lower safety floors or widen corridors; CI checks enforce that no query, trigger, or guard uses `ecocreditledger` when computing K/E/R, RoH, or Lyapunov. [file:18]
  - CI guard examples:
    - Static analysis (SQL/AST and Rust) verifies that:
      - Any function or view that writes to controller‑adjacent tables never reads from `ecocredit*` tables. [file:18]
      - Lane promotion and KER update logic reference only physical metrics and governance ALN, not reward totals. [file:18]

- Output‑only semantics:
  - EcoCredits:
    - Feed into EcoWealth/EcoUnit accounting and human/DAO‑level decisions, not real‑time control loops. [file:18]
    - Are materialized as ALN shards and ledger rows for reporting, incentives, and long‑horizon planning, while the non‑actuating spine continues to veto unsafe actions irrespective of rewards. [file:18]

---

## 14. Phoenix infrastructure instantiations

### 14.1 UHI hex shards (phoenix.uhi.hex.risk.v1)

- Shard purpose:
  - `phoenix.uhi.hex.risk.v1` encodes hex‑level Urban Heat Island risk for Phoenix, harmonized with your corridor grammar and Tree‑of‑Life residual. [file:18]
  - Each shard row corresponds to a hex cell with temperature, canopy, albedo, and anthropogenic heat features folded into risk coordinates. [file:18]

- Fields and risk coordinates:
  - Core fields (example as aligned with your riskvector grammar):
    - `hexid` (spatial index cell).
    - `region = 'Phoenix-AZ'`.
    - `rT` (thermal risk, e.g., normalized excess surface temperature or nighttime minima above baseline).
    - `rC` (carbon/combustion risk, e.g., traffic density, fossil load).
    - `rA` (air quality/atmospheric risk, including PM and ozone, mapped into 0..1). [file:18]
    - Optional: `canopyfrac`, `imperviousfrac`, `popdensity`, evidence and signing hex. [file:18]
  - Corridor binding:
    - Each `rT`, `rC`, `rA` is declared as a `RISKCOORD` in an ALN shard (e.g., `phoenix.uhi.hex.risk.v1.aln`) with SAFE/GOLD/HARD bands and weights. [file:18]

- Integration into Tree‑of‑Life residual:
  - Tree‑of‑Life residual (ToL) aggregates:
    - Hydrological, ecological, thermal, and biosignal risk traces into a scalar or vector residual per region. [file:18]
  - For Phoenix:
    - UHI risk is folded in via a weighted term:
      - \(R_\text{UHI} = w_T r_T + w_C r_C + w_A r_A\) aggregated over hexes (mean or population‑weighted). [file:18]
    - ToL residual includes `R_UHI` as a component alongside aquifer stress, restoration metrics, and Cyboquatic blast‑radius effects. [file:18]
  - Usage:
    - Guides where Cyboquatic cooling, shading, or water recovery machinery should be prioritized without directly controlling machines. [file:18]

### 14.2 MAR corridors (phoenix.mar.corridor.v1)

- Corridor purpose:
  - `phoenix.mar.corridor.v1` defines Managed Aquifer Recharge corridors around Phoenix, including drawdown limits, recharge targets, and filtration capacities. [file:18]
  - Encoded as corridor variables and region corridors consistent with `ecocorridorvar` and existing corridor grammar. [file:18]

- Metrics and grammar:
  - Core quantities per corridor or aquifer reach:
    - `drawdownm` (allowed drawdown from baseline in meters).
    - `rechargerate` (m³/day target or minimum recharge).
    - `filtrationcapacity` (m³/day of water that can be safely processed through soil/engineered media).
    - `salinity`, `contaminantload` metrics for water quality constraints. [file:18]
  - ALN corridor grammar:
    - `CORRIDORVAR drawdownm`, `SAFE/GOLD/HARD` bands and weights.
    - `CORRIDORVAR rechargeratio` (actual/target), encouraging `≥ 1`.
    - `CORRIDORVAR filtrationloadratio` (actual/allowed), constrained to ≤ 1. [file:18]
  - These are referenced by:
    - Non‑actuating Rust kernels that compute MAR residuals and flags (`marok`, `drawdownok`, `filtrationok`), which feed into lane guards and EcoCredit kernels but never actuate pumps. [file:18]

### 14.3 Phased deployment: spine hardening, Phoenix scale‑out, sovereignty schedule

- Phase 1 – Spine hardening:
  - Objectives:
    - Ensure SQLite schema coverage for Phoenix UHI/MAR tables and views in `ExpectedSchema`, including UHI hex risk, aquifer metrics, and corridor views. [file:18]
    - Extend `SchemaVerifier` and CI guards to:
      - Enforce presence of Phoenix UHI/MAR tables and their registration in `repofile` and `definitionregistry`. [file:18]
      - Require evidence windows (e.g., recent UHI measurements and MAR telemetry) for any PROD‑lane Phoenix machinery shard (“no evidence, no build”). [file:18]
  - Outputs:
    - Stable, non‑actuating views that expose Phoenix risk metrics to always‑improve, EcoCredit, and Tree‑of‑Life kernels. [file:18]

- Phase 2 – Phoenix scale‑out:
  - Objectives:
    - Bind UHI hex and MAR corridor shards to your Bostrom DID via restoration identity binding and hex stamping. [file:18]
    - Extend `vagentsafecatalog` and `econet.agentfunctioncatalog.v1.aln` to include Phoenix‑specific diagnostics tools (e.g., “list hexes with highest UHI risk given safe MAR capacity”). [file:18]
    - Integrate Cyboquatic machinery diagnostics (from blastradius/workload spines) with Phoenix UHI/MAR views to support planning queries like:
      - “Which nodes reduce UHI and recharge aquifers within hard corridors?”. [file:18]
  - Outputs:
    - High‑yield, AI‑safe tools for Phoenix operators and researchers, still strictly non‑actuating, informed by KER and corridor invariants. [file:18]

- Phase 3 – Multi‑decade sovereignty schedule:
  - Objectives:
    - Define a long‑horizon governance schedule in ALN (e.g., `phoenix.sovereignty.schedule.v1.aln`) that:
      - Encodes milestones for corridor tightening (no widening, only stricter caps).
      - Sets representation floors and education multipliers for affected communities via EcoWealth/EcoUnit coupling. [file:18]
      - Establishes policies for when and how data and EcoCredits evolve, ensuring monotone evolution of protection. [file:18]
    - Bind all Phoenix governance shards to your Bostrom DID so that:
      - The city‑scale governance history is auditable across decades.
      - Future kernel changes must satisfy “no corridor widening, no RoH ceiling increases” with Kani proofs. [file:18]
  - Outputs:
    - A durable eco‑sovereignty framework where Phoenix UHI/MAR and Cyboquatic interventions remain evidence‑first, non‑actuating, and aligned with Tree‑of‑Life restoration across multi‑decade horizons. [file:18]

## 15. Cyboquatic and industrial eco‑machinery surfaces

### 15.1 Cyboquatic workloads (canals, MAR, pumps, soft robots)

- Core Cyboquatic workload spine:
  - SQLite tables `cyboquaticworkloadledger`, `cybomachineryworkload`, and `cybomachineryblastradius` record:
    - Per‑workload energy (`ereqj`, `eusedj`, `esurplusj`), carbon (`rcarbon`, `carbonkg`), biodiversity risk (`rbiodiv`, `pollutantmasskg`), Lyapunov residuals (`vtbefore`, `vtafter`), RoH (`rohscalar`), and governance decisions (`ACCEPT`, `DERATE`, `REJECT`, `REROUTE`). [file:18]
    - Blast‑radius links from machines and shards to nodes, regions, aquifers, materials, and river reaches with `impacttype` and `impactscore`. [file:18]
  - These tables are strictly non‑actuating, forming the evidence base for canals, MAR systems, pumps, and soft robots operating in Cyboquatic corridors. [file:18]

- Blastradius diagnostics:
  - Views `vshardblastradius`, `vmachineblastradius`, and `cybomachineryblastradius` expose:
    - Per‑machine or per‑shard max blast‑radius by plane (`maxnoderadius`, `maxregionradius`, `maxenergyradius`, `maxcarbonradius`, `maxbiodivradius`) and aggregate Vt footprint (`vtradiussum`). [file:18]
    - Region and lane, so auditors can see which machines in Phoenix canals or MAR corridors contribute the most hydraulic/carbon/biodiversity risk. [file:18]
  - These diagnostics are used by Rust governance crates and AI‑safe tools, not controllers. [file:18]

- Workload windows:
  - View `vcyboworkloadnodewindow` aggregates workloads per `nodeid, region` into windows with:
    - `totalreqj`, `totalsurplusj`, `meanvtbefore`, `meanvtafter`, `deltavt`, `meanrcarbon`, `meanrbiodiv`, counts of `accepts`, `rejects`, `reroutes`, and `acceptfraction`. [file:18]
  - Future extensions add:
    - Deterministic window IDs and `alwaysimproveok` flags (Lyapunov non‑increase, carbon within corridor) to support always‑improve scoring and EcoCredit minting for MAR, canal, and pump workloads. [file:18]

- KER windows and corridors:
  - Governance spine views (e.g., `vresidualkernel`, corridor views) and new machinery‑specific views link:
    - Machine workloads and blast‑radius metrics to K/E/R residuals and RoH ceilings per lane and corridor. [file:18]
  - For canals, MAR pumps, and soft robots:
    - KER windows ensure upgrades or new variants respect Lyapunov and corridor constraints before promotion to `EXPPROD` or `PROD`. [file:18]

### 15.2 Industrial governance surfaces (magnet separation, wastewater, conveyance)

- Machinery registry and blast‑radius:
  - `cybomachinery` registry table defines:
    - `machineid`, `kind` (`PUMP`, `SCREEN`, `BLOWER`, `UFTRAIN`, `SOILWASH`, `MBR`, `ESS`, magnet separators, etc.), `region`, `lane`, `ecosafetyshard`, `rohlanemax`. [file:18]
  - `cybomachineryblastradius` links:
    - Each machine to `NODE`, `REGION`, `MATERIAL`, `AQUIFER`, or `RIVERREACH` with `impacttype` (`HYDRAULIC`, `ENERGY`, `CARBON`, `BIODIVERSITY`, `MATERIAL`, `DATAQUALITY`) and `impactscore`, `vtsensitivity`. [file:18]

- Workload ledgers for industrial equipment:
  - `cybomachineryworkload` table captures:
    - Per‑run energy (`ereqj`, `eusedj`), waste/water channels (`channel` = `energy`, `carbon`, `water`, `waste`, `biota`), pollutant mass removed (`pollutantmasskg`), Lyapunov before/after, `rohscalar`, `decision`, `lane`, and evidence/signing fields. [file:18]
  - Covers:
    - Wastewater pumps and screens, magnet separation modules, conveyance systems, and other eco‑machinery as telemetry‑only surfaces. [file:18]

- KER/RoH corridor exposure:
  - Views tying machinery to KER/RoH:
    - Internal rich views (e.g., `vrichcyboassetwindow`) join machinery identity, eco corridor bindings, workload windows, and blast‑radius links. [file:18]
    - Facade views (e.g., `vcyboassetfacade`) expose:
      - `assetid`, `assetkind`, `regioncode`, `maxcarbonradius`, `meanvtdelta`, `ecoperj`, `acceptfraction`, making KER/RoH effects visible for industrial modules. [file:18]
  - All surfaces remain:
    - Telemetry‑only (no duty cycles, valve positions, torque commands), while still exposing enough information to govern magnet trains, pumps, and waste conveyance through KER/RoH corridors. [file:18]

---

## 16. Non‑actuating C++ and FFI adapters

### 16.1 Telemetry headers (wastewater and magnet)

- Wastewater pump telemetry header:
  - A C++ header `wastewaterpumptelemetry.hpp` defines POD structs for:
    - Instantaneous and aggregated telemetry: flow rate, head, power, vibration bands, temperature, suction/discharge pressures. [file:18]
    - Governance metrics: KER triads (`kerk`, `kere`, `kerr`), `rohscalar`, lane, and corridor IDs associated with the pump. [file:18]
  - Intended use:
    - Pump controllers emit these structs into log streams or IPC channels; Rust governance crates ingest them via FFI or file parsing, but never send control commands back. [file:18]

- Magnet telemetry structs:
  - A similar C++ header for magnet separation (e.g., `magnet_telemetry.hpp`) declares:
    - Telemetry for magnetic field strength, throughput, particle load, reject stream quality, energy usage. [file:18]
    - Governance overlays: KER triads, RoH ceilings, and blast‑radius IDs for effluent and solids streams. [file:18]
  - Both headers:
    - Exclude any actuator commands, setpoints, or control parameters, and are strictly for telemetry export into the non‑actuating spine. [file:18]

### 16.2 FFI patterns (Rust cdylib, JSON, C‑ABI)

- Rust `cdylib` snapshots:
  - Non‑actuating crates (e.g., `cyboquaticblastradiusspine`) build as `cdylib` and expose:
    - Read‑only functions to fetch JSON snapshots of blast‑radius and workload windows, such as:
      - `cybospine_list_shard_blastradius_json(db_path: *const c_char) -> *mut c_char`
      - `cybospine_summarize_workload_node_region_json(db_path: *const c_char, nodeid: *const c_char, region: *const c_char) -> *mut c_char` [file:18]
  - Internal implementation:
    - Uses `CyboSpine::open`, queries views like `vcyboworkloadnodewindow` and `vmachineblastradius`, serializes `ShardBlastRadius` or `WorkloadNodeWindow` structs via `serde_json`. [file:18]

- JSON payloads and C‑ABI adapters:
  - C‑ABI functions:
    - Use `extern "C"` and raw C strings for maximum compatibility; they return heap‑allocated JSON strings for callers (Lua, C, Kotlin via JNI) to parse into telemetry dashboards or accountability logs. [file:18]
  - Accountability log pattern:
    - Loaders or adapters call these functions periodically and append returned JSON to append‑only logs (files or structured logging backends), preserving:
      - Machine IDs, windows, KER triads, RoH ceilings, blast‑radius summaries, and decisions over time. [file:18]

- Non‑actuation guarantees:
  - FFI exports:
    - Never expose actuating functions; no symbols to set pump speeds, magnet torques, or valve positions exist in these `cdylib`s. [file:18]
    - Are registered in `agentsafecatalog` and `econet.agentfunctioncatalog.v1.aln` with `actuationcapability = NONE`, making them AI‑safe diagnostic tools only. [file:18]
  - CI and catalog checks:
    - Ensure every exported C‑ABI function is:
      - Bound to a non‑actuating ALN function meta entry.
      - Present in AI‑safe catalogs only if it reads from read‑only governance views and never from actuator configuration or control channels. [file:18]
     
    ## 17. AI‑facing views, patterns, and tool catalogs

### 17.1 Diagnostic views

- Agent‑safe catalog view:
  - `vagentsafecatalog` is the primary AI‑facing catalog over `agentsafecatalog`, `agentsafediagnostics`, and `agentsafeconsentguard`. [file:18]
  - It exposes, per catalog entry:
    - Identity and governance: `catalogid`, `kind`, `name`, `description`, `domain`, `lane`, `corridorid`, `smartchainid`, `ecosafetyrequired`, `superpoweradjacent`. [file:18]
    - Diagnostics: `kscore`, `escore`, `rscore`, `rohscalar`, `vcurrent`, `vnext`, `lyapdelta`, `alwaysimprove`, `safetopromote`, `stableflag`, `marginclass`, `diagnosticsupdatedat`. [file:18]
  - It filters to entries where consent allows KER, RoH, Lyapunov telemetry and hides superpower‑adjacent entries unless `ecosafetyrequired != 0`. [file:18]

- Blastradius node‑window views:
  - Internal views (e.g., `vrichcyboshardstate`, `vrichcyboassetwindow`) and facades (`vmachineblastradius`) provide:
    - Per node/machine: region, lane, KER scalars, normalized RoH, `vtmax` or `meandeltavt`, blast‑radius by plane (carbon, biodiversity, etc.), and corridor bindings. [file:18]
  - These internal views prioritise diagnostic completeness and are consumed by Rust kernels and CI; AI‑facing facades expose only the scalar invariants and IDs needed for safe reasoning. [file:18]

- Workload windows with K, E, R, RoH, Vt, safestep flags:
  - `vcyboworkloadnodewindow` is the canonical node/window surface, exposing:
    - `nodeid`, `region`, `lane`, `windowstartutc`, `windowendutc`. [file:18]
    - Aggregates: `totalreqj`, `totalsurplusj`, `meanvtbefore`, `meanvtafter`, `deltavt`, `meanrcarbon`, `meanrbiodiv`, `acceptfraction`. [file:18]
  - Rust always‑improve kernel computes:
    - `alwaysimprove`, `safetopromote`, and stability/margin flags that are then written into `agentsafediagnostics` and surfaced via `vagentsafecatalog` to AI agents. [file:18]

### 17.2 SQL pattern index (agentsqlpattern)

- Pattern table:
  - `agentsqlpattern` stores:
    - `patternid`, `patternname`, `description`, `sqltext` (parameterized), `lanescope`, `roleband`, `riskceilingnote`, `actuationflag` (must be 0 for AI‑safe), and `blastradiusscore`. [file:18]
  - Patterns are DefinitionRegistry‑bound and versioned under monotone evolution, so they can only get stricter, not more permissive. [file:18]

- Parameter typing and AI capability levels:
  - Patterns carry:
    - Typed parameter hints (region, lane, nodeid, corridorid) and permitted value ranges or enumerations in ALN or metadata. [file:18]
  - AI capability levels:
    - Pattern metadata and repo manifest ALN (`econet.repomanifest.ai.v1.aln`) define `aicapabilitylevel` (e.g., `NONE`, `READONLYDOC`, `READONLYSPINE`, `PROTOAGENT`) and which patterns each level may use. [file:18]

- Pattern risk scoring:
  - A simple risk score `patternrisk` is computed from:
    - Planes and corridors touched and plane risk weights; only patterns with `patternrisk` below an AI threshold are exposed for AI use. [file:18]
  - Optional usage log table (`agentsqlpatternusage`) tracks anonymised pattern runs to tune or retire risky patterns. [file:18]

### 17.3 Tool catalog and MCP bindings

- Agent function catalog:
  - `econet.agentfunctioncatalog.v1.aln` mirrors `vagentsafecatalog` and `agentsqlpattern`, with entries for each tool:
    - `functionid`, `summary`, `inputschema`, `outputschema`, `backingvieworbin`, `lanescope`, `riskbands`, `actuationcapability` (must be `NONE` for AI‑safe). [file:18]
  - Provides a typed tool list that LLM platforms can load once as their function schema. [file:18]

- MCP tool bindings:
  - For each MCP tool (Lua or other agent server):
    - There must be a backing entry in `agentsqlpattern` and `econet.agentfunctioncatalog.v1.aln`, and a row in `vagentsafecatalog` for the underlying view/FFI. [file:18]
  - MCP servers:
    - Resolve tool calls exclusively via `vagentsafecatalog`; if the function is not in the catalog or fails consent checks, the tool is unavailable. [file:18]

- CI enforcement:
  - CI checks ensure:
    - Every AI‑exposed tool has consistent entries across SQL, ALN, and function catalog. [file:18]
    - No tool with `actuationcapability != NONE` or superpower internals appears in AI‑safe catalogs. [file:18]

---

## 18. Data sovereignty, consent, and health/biosignal surfaces

### 18.1 Sovereign consent engines and TelemetryVerdict

- Sovereign consent engine role:
  - `sovereign-consent-engines` crate enforces:
    - Host data envelopes, neurorights, and “Prometheus‑Praxis only, no commercial recipients, no raw neural export” constraints. [file:18]
  - It is the authoritative decision‑maker for which telemetry families (KER, RoH, Lyapunov, eco‑health, BCI) may appear in AI‑facing views. [file:18]

- TelemetryVerdict and stream‑level gating:
  - Consent decisions are expressed per catalog entry and telemetry family in `agentsafeconsentguard`:
    - `catalogid`, `telemetryfamily` (e.g., `KER`, `ROH`, `LYAP`, `ECOHEALTHAGG`, `BCIAGG`), `allowed` (0/1), `reason`, `updatedat`. [file:18]
  - `vagentsafecatalog`:
    - Joins `agentsafecatalog`, `agentsafediagnostics`, and `agentsafeconsentguard` and only returns rows where `allowed = 1` for required families (KER, RoH, Lyapunov). [file:18]
  - Any health/BCI/eco‑health fields:
    - Must additionally pass TelemetryVerdict in the consent engine before they can appear in AI‑safe views or function catalogs. [file:18]

### 18.2 Exclusions and allowed aggregations

- Explicit exclusions:
  - Per Prometheus‑Praxis neurorights and host envelopes:
    - Raw EEG and high‑frequency neural waveforms are forbidden in AI‑safe catalogs and views. [file:18]
    - Unaggregated lab values and fine‑grained biosignal traces tied to an identifiable host are also excluded. [file:18]
  - Catalog rules:
    - Any field classified as raw neural or identity‑linked biosignal is blocked at schema and view level from `vagentsafecatalog`. [file:18]

- Allowed aggregated, consented, DP‑safe metrics:
  - Only aggregated, consented, and, where appropriate, differentially private (DP‑safe) health metrics may be surfaced, for example:
    - Monthly normalized microplastic burden index. [file:18]
    - Daily detox stress scalar. [file:18]
    - Hourly BCI cognitive load in bands (`LOW`, `MED`, `HIGH`). [file:18]
    - Eco‑phi vectors in DP‑safe vector form. [file:18]
  - All such fields:
    - Must be explicitly permitted in host ALN envelopes (`host.data.contribution.envelope.v1`) and carry a positive TelemetryVerdict through `agentsafeconsentguard`. [file:18]

- Alignment with sovereignty and Prometheus‑Praxis:
  - This design:
    - Keeps AI‑exposed surfaces diagnostic‑only (KER, RoH, Lyapunov, stability flags) with no actuation or raw biosignals. [file:18]
    - Ensures AI agents only see what sovereign consent and neurorights envelopes permit, matching the “data‑as‑labor, neurorights‑first, no rollback” narrative. [file:18]

## 19. Eco‑Impact, Knowledge Factor, and Risk Scoring

### 19.1 KER scoring for views, functions, and planes

- KER components:
  - The governance spine exposes knowledge \(K\), eco‑impact \(E\), and residual risk \(R\) for shards, views, and machinery via `shardinstance`, `knowledgeecoscore`, and corridor bindings. [file:18]
  - Internal rich views (e.g., `vrichcyboshardstate`, `vrichcyboassetwindow`) provide:
    - `kmetric`, `emetric`, `rmetric`, Lyapunov residuals (`vtmax`, `meanvtbefore`, `meanvtafter`), and normalized RoH coordinates. [file:18]

- KER‑aware diagnostic surfaces:
  - A cross‑cutting `agentobjectscore` table (proposed) holds:
    - `objectid` (view/FFI/function), `kscore`, `escore`, `rscore`, `rohscalar`, `lyapdelta`, and boolean flags like `safestepok`, `alwaysimproveok`. [file:18]
  - `vagentsafecatalog` joins these scores so AI agents see:
    - KER values and stability flags per tool or surface, not raw logs. [file:18]

- How KER drives tool ranking:
  - Tool ranking logic uses:
    - High `kscore` and `escore` with low `rscore` to prioritise tools; `rscore` is derived from corridor risk vectors and plane weights. [file:18]
  - Repo‑level risk can be estimated as:
    - \(R_{\text{repo}, i} = \max_{s \in \text{shards}_i} R_s\), exposed as `repormax` in `veconetrepomanifestagent` to favour low‑risk repos for AI experimentation. [file:18]

### 19.2 Eco‑penalty, eco‑credits, eco‑wealth, and blast‑radius metrics

- Blast‑radius‑normalized impact and eco‑penalty:
  - `vshardblastradius` and `vmachineblastradius` expose:
    - `maxcarbonradius`, `maxbiodivradius`, `vtradiussum` per shard or machine. [file:18]
  - A derived eco‑penalty per machine \(P_{\text{eco}, j}\) can be computed as:
    - \(P_{\text{eco}, j} = \sum_p w_p \cdot \text{impactscore}_{p,j} \cdot R_{\text{corridor}, p}\), where \(w_p\) are plane weights and \(R_{\text{corridor}, p}\) is the corridor risk factor. [file:18]

- Always‑improve score and KER constraints:
  - Always‑improve scoring uses:
    - Inputs from `vcyboworkloadnodewindow` and `vshardblastradius`. [file:18]
  - Define:
    - \(E_{\text{eff}, s} = \frac{\text{totalsurplusJ}}{\text{totalreqJ}}\), \(C_{\text{risks}} = r_{\text{carbon}}\), \(B_{\text{risks}} = r_{\text{biodiv}}\), \(V_t = \Delta V_t\). [file:18]
  - Always‑improve score:
    - \(S_{\text{AI}, s} = E_{\text{eff}, s} - C_{\text{risks}} - B_{\text{risks}} - V_t\), subject to:
      - \(C_{\text{risks}} \le 0.13\), \(V_t \le 0\), and no violation of non‑offsettable planes. [file:18]

- Eco‑credits and eco‑wealth kernels:
  - EcoWealth logic links:
    - EcoWealth increase to improvements in \(S_{\text{AI}, s}\) while KER constraints and lane admissibility hold; EcoWealth decays if \(S_{\text{AI}, s}\) stagnates or worsens. [file:18]
  - Eco‑credits are minted or adjusted when:
    - Windows satisfy `carbonnegativeok`, `restorationok`, and always‑improve constraints, using `vcyboworkloadnodewindow` and restoration views like `vcyboquaticecoperjoule` and `vcyboquaticrestore`. [file:18]

---

## 20. Roadmap and Task Shards

### 20.1 Task ALN: canonical coding backlog

- PrometheusPraxisCodingTaskList2026v1.aln:
  - A task ALN shard (proposed) serves as:
    - The canonical backlog for EcoNet spine wiring, ecosafety pipelines, AI‑safe catalogs, and cross‑domain integration. [file:18]
  - Each task particle includes fields such as:
    - `taskid`, `summary`, `repo`, `lane`, `kerk_target`, `kere_target`, `kerr_max`, `roh_ceiling`, `vt_target`, `aicapabilitylevel`, `superpoweradjacent`, and `acceptancecriteria`. [file:18]

- Governance flags and sovereignty shards:
  - Tasks can reference:
    - Sovereignty and consent shards (e.g., host envelopes, data contribution specs) and MCP dev‑tunnel specifications. [file:18]
  - Each task carries:
    - Flags like `nonactuatingrequired`, `requiresconsentengine`, `multicorridor`, ensuring alignment with Prometheus‑Praxis superpower boundaries and neurorights. [file:18]

### 20.2 Execution planes and encoding roadmap items

- Execution planes in the roadmap:
  - Roadmap items span:
    - Ecosafety pipelines (richer RoH/KER/Lyapunov surfaces), MCP integration, and cross‑domain coupling (e.g., materials → Cyboquatics → EcoNet rewards). [file:18]
  - Each task is tagged with:
    - `executionplane` (e.g., `SPINE_SQL`, `RUST_KER`, `MCP_AGENT`, `SOVEREIGNTY`), mapping to the part of the stack it modifies. [file:18]

- KER targets and acceptance criteria:
  - For each coding task:
    - KER targets specify expected changes in diagnostic surfaces (e.g., new `carbonnegativeok` and `restorationok` flags; exposure of `safestepok` in AI‑safe views). [file:18]
  - Acceptance criteria include:
    - Concrete invariants such as “no new tool in `econet.agentfunctioncatalog.v1.aln` has `actuationcapability != NONE`”, “all new views covered by ExpectedSchema and CI schema verifier”, and “Kani harness passes for new always‑improve formulas”. [file:18]

- Roadmap as machine‑readable governance:
  - Encoding the roadmap in ALN:
    - Lets CI and agents treat tasks as governance‑bound objects with explicit KER targets instead of informal tickets. [file:18]
  - This supports:
    - Automated checks that a task is completed only when its KER, RoH, Lyapunov, and sovereignty acceptance criteria are met, keeping evolution monotone and ecosafety‑aligned. [file:18]

## 21. Security, Monotone Evolution, and Attack Surfaces

### 21.1 No‑downgrade doctrine and monotone KER/RoH

- Under‑attack karma and no‑rollback:
  - Prometheus‑Praxis enforces a monotone evolution doctrine: protection levels, RoH floors, and KER thresholds can only tighten over time, never loosen, even under attack or misconfiguration. [file:18]
  - “Under‑attack karma” and OTA guardrails ensure any branch or migration that attempts to lower protections fails proofs and CI, preventing hostile downgrades of ecosafety or sovereignty constraints. [file:18]

- Immutable plane weights and non‑offsettable planes:
  - Certain planes (e.g., psychosocial, biodiversity, neurorights) are marked non‑offsettable and have immutable or strictly monotone plane weights stored in ALN shards such as `PlaneWeightsShard2026v1`. [file:18]
  - Governance rules forbid trading improvements in offsettable planes (e.g., energy) against regressions in non‑offsettable planes; scoring functions and CI guards must respect these weights. [file:18]

- Monotone KER/RoH ceilings:
  - KER invariants require:
    - \(K_{\text{new}} \ge K_{\text{old}}\), \(E_{\text{new}} \ge E_{\text{old}}\), \(R_{\text{new}} \le R_{\text{old}}\) for any shard upgrade, enforced by `KerUpgradeGuard`. [file:18]
  - RoH ceilings and Lyapunov residuals are encoded as corridor variables; lane upgrades to `EXPPROD`/`PROD` require RoH under ceiling and non‑increasing Lyapunov according to `LaneGuard` and Kani‑verified kernels. [file:18]

### 21.2 Attack surface reduction

- Non‑actuating crates and sealed superpowers:
  - Governance and diagnostic crates (EcoNet spine, Cyboquatic spines, always‑improve kernels) are strictly non‑actuating: they read logs, compute scores, emit JSON/ALN, and never send control signals. [file:18]
  - Actuation kernels (Perkunos‑Nexus, MT6883 stacks, nanoswarm routing, HeatWaterTree, etc.) are sealed superpowers, not exposed in AI‑safe catalogs; only their diagnostic projections (KER, RoH, Vt, treaty IDs) appear in views and JSON. [file:18]

- ALN‑backed policies and AI‑safe catalog:
  - An AI‑safe catalog enforced via `vagentsafecatalog` and `econet.agentfunctioncatalog.v1.aln`:
    - Lists only non‑actuating views/FFI functions, with `actuationcapability = NONE` and bounded blast‑radius classifications. [file:18]
  - ALN shards define:
    - Non‑Actuating Workload contracts, risk vectors, plane weights, and function catalog entries, giving a typed, cryptographically anchored policy layer for tools and metrics. [file:18]

- CI guards and hidden surface controls:
  - `SchemaVerifier` and DefinitionRegistry CI:
    - Enforce that all governance artifacts (`.sql`, `.rs`, `.aln`) are registered (`no‑registry, no‑build`), preventing shadow schemas or hidden APIs. [file:18]
  - Additional guards:
    - `no‑blast‑radius, no‑build` for machinery shards without blast‑radius links; energy/carbon evidence guards for production workloads; and Cyboquatic proof harnesses that recompute metrics and fail CI on discrepancies. [file:18]
  - Unsafe IO constraints:
    - AIfacing functions are limited to reading from governance DBs and emitting logs/shards; they are forbidden from touching actuator queues, raw device IO, or unvetted network sinks. [file:18]

---

## 22. Implementation Patterns and Developer Workflow

### 22.1 Coding workflow: Rust, ALN, and CI conventions

- Rust & ALN conventions:
  - All governance and diagnostic logic lives in Rust crates with:
    - Edition 2024, `rust-version = "1.85"`, dual MIT/Apache‑2.0 licensing, explicit `#![forbid(unsafe_code)]` except at FFI boundaries. [file:18]
  - Data contracts and governance grammars are defined in ALN shards:
    - For metrics, KER corridors, plane weights, non‑actuating workloads, risk vectors, and agent function catalogs. [file:18]

- Non‑actuation rules:
  - Crates in the spine (EcoNet governance, Cyboquatic blastradius spines, always‑improve kernels):
    - Are non‑actuating by design; they access SQLite via `rusqlite`, compute scores, and offer read‑only FFI and JSON APIs. [file:18]
  - Any crate that might be used by AI chat:
    - Must be tagged non‑actuating in repo manifests and AI catalogs, and is forbidden from linking to actuator‑side stacks. [file:18]

- Python‑only CI helpers, no tool installation:
  - CI helpers may use Python:
    - To run schema verifiers, DefinitionRegistry scans, and proof harness orchestration, but not to implement core safety logic. [file:18]
  - No ad‑hoc tool installation:
    - CI is designed to run with in‑repo code and SQLite files only; external dependencies are minimized to prevent hidden surfaces or unverified tooling. [file:18]

### 22.2 Example single‑session developer task pattern

- Typical single‑session task flow:
  - A developer picks a task from `PrometheusPraxisCodingTaskList2026v1.aln`:
    - Each task specifies target repo, lane, execution plane (e.g., `SPINE_SQL`, `RUST_KER`, `MCP_AGENT`), and KER targets/acceptance criteria. [file:18]
  - Within one session, they:
    - Wire kernels, extend schemas, and update ALN shards while preserving invariants and keeping evolution monotone. [file:18]

- Example: adding an always‑improve view and Rust kernel wiring:
  - Steps:
    - Extend schema: add or complete `vcyboquaticecoperjoule` and `vcyboquaticrestore` to compute `carbonnegativeok` and `restorationok`. [file:18]
    - Implement Rust scoring: finalize `calculate_always_improve_score` and Kani harnesses enforcing KER, RoH, and Lyapunov invariants. [file:18]
    - Update ALN: define or revise ALN shards describing new metrics and task acceptance criteria. [file:18]
    - Wire AI‑safe catalog: expose new boolean flags and diagnostics via `vagentsafecatalog` and `econet.agentfunctioncatalog.v1.aln`, ensuring `actuationcapability = NONE`. [file:18]
    - Run CI: verify `ExpectedSchema`, CI guards, and proof harnesses all pass before merging. [file:18]

- Invariant‑preserving incremental expansion:
  - Each session:
    - May only add new diagnostics, tighten filters, or add stricter KER/RoH constraints; it may not remove guards or loosen ceilings. [file:18]
  - This workflow:
    - Ensures that functionality and AI‑chat surfaces expand over time, but security, ecosafety, and sovereignty constraints evolve monotonically in the protective direction. [file:18]

## 23. Conclusion and Future Extensions

### 23.1 Summary of guarantees

- Safer eco‑machinery:
  - Prometheus‑Praxis enforces a strict non‑actuating governance spine: all SQLite schemas, Rust crates, and FFI surfaces are diagnostic only and never control hardware. [file:18]
  - Core invariants (KER bands, RoH ceilings, Lyapunov non‑increase) are embedded in rich views and always‑improve kernels, then proven with tools like Kani to prevent unsafe promotions of Cyboquatic machinery. [file:18]

- Sovereignty and neurorights:
  - An AI‑safe catalog and sovereign‑consent engine gate every AI‑facing surface, permanently excluding raw EEG, high‑frequency biosignals, and identity‑linked lab traces. [file:18]
  - Only aggregated, DP‑safe, consented eco‑health metrics (e.g., microplastic burden, detox stress, BCI load bands) may appear, and only when TelemetryVerdict is positive. [file:18]

- Mathematically governed evolution:
  - KER monotonicity, RoH ceilings, and Lyapunov residual constraints are encoded as hard gates in lane guards and always‑improve scoring; Kani harnesses check that kernels never violate these invariants. [file:18]
  - The no‑corridor, no‑build rule and CI schema verifiers prevent unbounded machinery or un‑evidenced workloads from entering production, keeping evolution monotone toward safer, more restorative configurations. [file:18]

### 23.2 Future work

- Large‑particle summary engines:
  - Build “large‑particle” summary engines that:
    - Aggregate shard‑level data (blastradius, ecoperjoule, restoration metrics) into higher‑order ALN particles for regions, corridors, and Tree‑of‑Life branches. [file:18]
  - These engines would:
    - Provide succinct, corridor‑level diagnostics and eco‑wealth summaries safe for AI‑chat and human stewards. [file:18]

- AI‑chat lane auditors:
  - Implement AI‑chat lane auditors as non‑actuating Rust tools that:
    - Recompute KER, RoH, Lyapunov, carbonnegativeok, and restorationok for shards and compare to lane flags and EcoWealth state. [file:18]
  - CI would:
    - Treat these auditors as proof harnesses, failing when lane states or rewards diverge from recomputed invariants. [file:18]

- CrossSync into external registries:
  - Design CrossSync modules that:
    - Export curated, ALN‑backed, blastradius‑normalized metrics into external sustainability registries and public ledgers without exposing sensitive telemetry. [file:18]
  - CrossSync must:
    - Preserve Bostrom DID bindings and ALN spechash hex‑stamping so external consumers can verify provenance and invariants. [file:18]

- Extended Tree‑of‑Life integrations:
  - Extend Tree‑of‑Life (ToL) grammars:
    - To represent biodiversity risk, species‑level restoration, and eco‑substrate feedback loops as Lyapunov channels and corridor variables. [file:18]
  - Future work links:
    - Machinery blastradius and eco‑metrics to ToL residuals, ensuring that improvements in pumps and substrates map directly into lower ecological risk across taxa and habitats. [file:18]

---

## 24. Appendix A: Formal Definitions and Key Equations

### 24.1 Core Lyapunov and residual definitions

- Lyapunov potential \(V\):
  - For a system state \(x_t\), the Lyapunov potential \(V(x_t)\) is a scalar measuring distance from an eco‑safe equilibrium; lower values are safer. [file:18]
  - Residuals are recorded per workload in ledgers as:
    - \(V_{\text{before}} = V(x_t)\), \(V_{\text{after}} = V(x_{t+1})\). [file:18]

- Residual delta and window residual:
  - Per workload:
    - \(V_t = V_{\text{after}} - V_{\text{before}}\). [file:18]
  - Over a window at node \(n\):
    - \(\overline{V}_n = \frac{1}{N} \sum_{i=1}^{N} \left(V_{\text{after},i} - V_{\text{before},i}\right)\), constrained so \(\overline{V}_n \le 0\). [file:18]

- KER window invariants:
  - For each shard or asset and lane:
    - \(K \ge K_{\min}^{\text{lane}}\), \(E \ge E_{\min}^{\text{lane}}\), \(R \le R_{\max}^{\text{lane}}\). [file:18]
  - Lane guards require:
    - RoH under its ceiling and \(\overline{V} \le 0\) for promotion. [file:18]

### 24.2 Always‑improve score and harmful/beneficial normalization

- Eco‑efficiency and risk components:
  - From `vcyboworkloadnodewindow` and blastradius views:
    - \(E_{\text{eff}, s} = \frac{\text{totalsurplusJ}}{\text{totalreqJ}}\). [file:18]
    - \(C_{\text{risks}} = r_{\text{carbon}}\) (mean carbon risk coordinate). [file:18]
    - \(B_{\text{risks}} = r_{\text{biodiv}}\) (mean biodiversity risk coordinate). [file:18]
    - \(V_t = \overline{V} = \text{meanvtafter} - \text{meanvtbefore}\). [file:18]

- Always‑improve score with constraints:
  - The canonical always‑improve score per shard \(s\) is:
    - \(S_{\text{AI}, s} = E_{\text{eff}, s} - C_{\text{risks}} - B_{\text{risks}} - V_t\), subject to:
      - \(C_{\text{risks}} \le 0.13\). [file:18]
      - \(V_t \le 0\). [file:18]
      - No violations on non‑offsettable planes according to plane weights. [file:18]
  - A shard is safetopromote only if:
    - KER, RoH, Lyapunov constraints hold and \(S_{\text{AI}, s}\) is non‑decreasing across windows. [file:18]

- Harmful/beneficial normalization:
  - Normalized risk coordinates for plane \(p\):
    - \(r_p \in [0, 1]\) with corridor variables specifying SAFE, GOLD, and HARD thresholds (e.g., for `maxcarbonradius`, `maxbiodivradius`, `deltavt`). [file:18]
  - Normalization ensures:
    - Harmful contributions push \(r_p\) toward 1, beneficial actions move \(r_p\) toward 0, with non‑offsettable planes weighted so they cannot be compensated by gains elsewhere. [file:18]

### 24.3 Tree‑of‑Life residuals and biodiversity risk coordinates

- Tree‑of‑Life residuals:
  - ToL residual \(V_{\text{ToL}}\) generalizes Lyapunov to ecological state:
    - Aggregating across species, trophic levels, and habitats as weighted sums or more complex potentials. [file:18]
  - A future ToL channel would:
    - Track \(V_{\text{ToL,before}}\), \(V_{\text{ToL,after}}\), and \(\Delta V_{\text{ToL}}\) per workload or design choice. [file:18]

- Biodiversity risk coordinates:
  - Biodiversity risk \(r_{\text{biodiv}}\) is:
    - A normalized coordinate (\(0 \ldots 1\)) derived from ToL impacts, species loss risk, and habitat fragmentation metrics. [file:18]
  - Corridor variables for `maxbiodivradius`:
    - Define SAFE, GOLD, HARD bands and weights so even small increases near HARD thresholds heavily penalize always‑improve scores. [file:18]

### 24.4 Notation and symbol table

- Planes and lanes:
  - Planes:
    - HYDRAULIC, ENERGY, CARBON, BIODIVERSITY, MATERIAL, DATAQUALITY, plus future Tree‑of‑Life planes. [file:18]
  - Lanes:
    - RESEARCH, EXPPROD, PROD; each lane has KER/RoH thresholds and Lyapunov requirements. [file:18]

- Corridors and hex shards:
  - Corridors:
    - Bounded regions in risk space defined by corridor variables (SAFE, GOLD, HARD, weights, mandatory flags). [file:18]
  - Hex shards:
    - ALN particles (e.g., `CyboquaticBlastRadiusShard2026v1`) hex‑stamped via `ALNSPECHASHHEX` and bound to Bostrom DIDs for provenance. [file:18]

- Core symbols:
  - \(K\): Knowledge factor (confidence, model fidelity, data richness). [file:18]
  - \(E\): Eco‑impact or eco‑benefit score (e.g., energy‑per‑joule, restoration metrics). [file:18]
  - \(R\): Residual risk‑of‑harm scalar per shard or asset. [file:18]
  - RoH: Risk‑of‑Harm coordinate, often normalized and corridor‑bound. [file:18]
  - \(V\): Lyapunov potential; \(V_t\) or \(\Delta V\) denotes residual change. [file:18]
  - \(S_{\text{AI}}\): Always‑improve score combining eco‑efficiency, risks, and Lyapunov residual. [file:18]
  - ToL / \(V_{\text{ToL}}\): Tree‑of‑Life potential, a future eco‑Lyapunov channel over biodiversity and life‑system health. [file:18]

## 25. Appendix B: Schema and Shard Index

### 25.1 SQLite schema index (governance core vs AI-safe facades)

The schema below treats internal tables as **evidence kernels** and views as read-only, AI-safe projections. [file:18]

#### Core governance and telemetry tables

| Table name                          | Target domain           | Core columns (subset)                                                                                      | Primary invariant / role                                                                                       |
|-------------------------------------|-------------------------|------------------------------------------------------------------------------------------------------------|-----------------------------------------------------------------------------------------------------------------|
| `corridordefinition`                | Corridor grammar        | `corridorid`, `corridorname`, `lanescope`, `createdat`, `updatedat`                                       | Defines SAFE/GOLD/HARD band containers per lane; binds corridor IDs used across risk/KER snapshots. [file:18]  |
| `ecocorridorvar`                    | Corridor variables      | `varname`, `plane`, `safe_max`, `gold_max`, `hard_max`, `weight`, `non_offsettable_flag`                  | Encodes per-plane corridor bounds and non‑offsettable flags for KER and RoH coordination. [file:18]            |
| `knowledgeecoscore`                 | KER state               | `shardid`, `kmetric`, `emetric`, `rmetric`, `rohscalar`, `tsafe`, `updatedat`                             | Tracks K/E/R and normalized RoH per shard; lane guards reference these metrics for promotions. [file:18]       |
| `blastradiuslink`                   | Impact topology         | `sourcetype`, `sourceid`, `targettype`, `targetid`, `impacttype`, `impactscore`, `vtsensitivity`          | Defines blast‑radius graph edges and Vt sensitivity across machines, shards, nodes, and regions. [file:18]     |
| `cyboquaticworkloadledger`          | Cyboquatic workloads    | `workload_id`, `nodeid`, `lane`, `regioncode`, `ereqj`, `esurplusj`, `rcarbon`, `rbiodiv`, `vtbefore`, `vtafter`, `decision`, `signingdid` | Evidence ledger for cyboquatic jobs enforcing ΔV and risk bounds via triggers/guards. [file:18]                |
| `cybomachinery`                     | Machine registry        | `machineid`, `kind`, `region`, `lane`, `ecosafetyshard`, `rohlanemax`                                     | Identifies telemetry‑only industrial units and ties them to ecosafety shards and RoH lane ceilings. [file:18]   |
| `cybomachineryworkload`             | Industrial workloads    | `workload_id`, `machineid`, `channel`, `ereqj`, `eusedj`, `pollutantmasskg`, `vtbefore`, `vtafter`, `rohscalar`, `decision` | Workload evidence for industrial processes, no control fields. [file:18]                                       |
| `cybomachineryblastradius`          | Industrial impact       | `machineid`, `targettype`, `targetid`, `impacttype`, `impactscore`, `vtsensitivity`                       | Normalized blast‑radius per machine across ecological planes. [file:18]                                        |
| `lanestatusshard`                   | Lane governance         | `shardid`, `lane`, `assignedat`, `expiresutc`, `safetopromoteok`                                          | Tracks current lane and promotion readiness, enforcing TTL and monotone evolution. [file:18]                   |
| `workloadledger`                    | General workloads       | `workload_id`, `nodeid`, `lane`, `vtbefore`, `vtafter`, `decision`, `ts_utc`                              | Canonical Lyapunov residual ledger for non‑cyboquatic workloads. [file:18]                                     |
| `cyboquaticmicroplasticriskindex`   | Microplastic state      | `nodeid`, `microplasticloadmgm3`, `ecoperjoule`, `restorationscore`, `kmetric`, `emetric`, `rmetric`, `vtbefore`, `vtafter`, `rohscalar` | KER‑aware microplastic risk index; triggers enforce ΔV≤0 and corridor compliance. [file:18]                    |
| `agentsafecatalog`                  | AI function registry    | `catalogid`, `kind`, `name`, `description`, `domain`, `lane`, `corridorid`, `ecosafetyrequired`, `superpoweradjacent` | Master index of AI‑visible tools; forbids superpower exposure. [file:18]                                       |
| `agentsafediagnostics`              | AI diagnostics          | `catalogid`, `kscore`, `escore`, `rscore`, `rohscalar`, `vcurrent`, `vnext`, `lyapdelta`, `alwaysimprove`, `safetopromote`, `marginclass` | Stores offline S_AI and stability scores pushed by Rust batch jobs. [file:18]                                  |
| `agentsafeconsentguard`             | Sovereign consent       | `catalogid`, `telemetryfamily`, `allowed`, `reason`, `updatedat`                                          | Per‑tool consent gate for KER, RoH, Lyapunov, and eco‑health metrics. [file:18]                                |
| `agentsqlpattern`                   | SQL template index      | `patternid`, `patternname`, `description`, `sqltext`, `lanescope`, `roleband`, `actuationflag`, `blastradiusscore` | Pre‑vetted parameterized read‑only SQL templates for LLM agents. [file:18]                                     |

Internal triggers like `trg_microplastic_ker_invariants` conceptually enforce: [file:18]

```sql
CREATE TRIGGER trg_microplastic_ker_invariants
BEFORE INSERT OR UPDATE ON cyboquaticmicroplasticriskindex
FOR EACH ROW
BEGIN
  -- enforce Lyapunov non-increase
  SELECT
    CASE
      WHEN NEW.vtafter - NEW.vtbefore > 0.0 THEN
        RAISE(ABORT, 'Lyapunov residual violation: ΔV must be <= 0')
    END;

  -- enforce KER corridor bounds (example)
  SELECT
    CASE
      WHEN NEW.kmetric < 0.0 OR NEW.emetric < 0.0 OR NEW.rmetric > 1.0 THEN
        RAISE(ABORT, 'KER corridor violation')
    END;
END;
```

This keeps all Lyapunov/KER invariants at the storage engine boundary. [file:18]

#### Diagnostic and AI-safe views

```sql
CREATE VIEW IF NOT EXISTS vagentsafecatalog AS
SELECT 
    c.catalogid,
    c.kind,
    c.name,
    c.description,
    c.domain,
    c.lane,
    c.corridorid,
    c.smartchainid,
    c.ecosafetyrequired,
    c.superpoweradjacent,
    d.kscore,
    d.escore,
    d.rscore,
    d.rohscalar,
    d.vcurrent,
    d.vnext,
    d.lyapdelta,
    d.alwaysimprove,
    d.safetopromote,
    d.stableflag,
    d.marginclass,
    d.updatedat AS diagnosticsupdatedat
FROM agentsafecatalog c
JOIN agentsafediagnostics d ON c.catalogid = d.catalogid
JOIN agentsafeconsentguard g_ker
  ON c.catalogid = g_ker.catalogid AND g_ker.telemetryfamily = 'KER'
JOIN agentsafeconsentguard g_roh
  ON c.catalogid = g_roh.catalogid AND g_roh.telemetryfamily = 'ROH'
JOIN agentsafeconsentguard g_lyap
  ON c.catalogid = g_lyap.catalogid AND g_lyap.telemetryfamily = 'LYAP'
WHERE g_ker.allowed = 1 
  AND g_roh.allowed = 1 
  AND g_lyap.allowed = 1
  AND (c.superpoweradjacent = 0 OR c.ecosafetyrequired != 0);
```
[file:18]

| View name                           | Exposure tier           | Underlying entities                                          | Primary functional target                                                                          |
|-------------------------------------|-------------------------|--------------------------------------------------------------|----------------------------------------------------------------------------------------------------|
| `vplaneweights`                     | Internal governance     | `ecocorridorvar`                                            | Surfaces plane weights and corridor bounds to Rust guard kernels. [file:18]                       |
| `vresidualkernel`                   | Internal governance     | `knowledgeecoscore`, `shardinstance`                        | Aggregates per‑shard KER + residuals as a single diagnostic vector. [file:18]                     |
| `vshardblastradius`                 | Internal governance     | `blastradiuslink`                                           | Computes per‑shard blast radii across CARBON/BIODIVERSITY/ENERGY. [file:18]                       |
| `vmachineblastradius`              | Internal governance     | `blastradiuslink`, `cybomachinery`                         | Machine‑level blast footprints with expiry and lane filtering. [file:18]                          |
| `vcyboworkloadnodewindow`          | Internal governance     | `cyboquaticworkloadledger`                                 | Windowed averages for energy, surplus, residuals, and risk. [file:18]                             |
| `vcyboworkloadnodewindow_residual` | Internal diagnostics    | `vcyboworkloadnodewindow`                                  | Adds `meanvtafter - meanvtbefore` as `meandeltavt`. [file:18]                                     |
| `vlaneadmissibility`                | Internal governance     | `shardinstance`, `knowledgeecoscore`, `ecocorridorvar`     | Computes `kerk_ok`, `kere_ok`, `kerr_ok` and eco flags for lane changes. [file:18]               |
| `vlanepromotionhistory`            | Internal governance     | `lane_change_log`                                          | Historical lane transitions to detect downgrades. [file:18]                                       |
| `vlatestlanestatus`                | Internal governance     | `lanestatusshard`                                          | Active, non‑expired lane per shard. [file:18]                                                     |
| `virtalaneverdict`                 | Internal governance     | `lanestatusshard`, `vlaneadmissibility`                    | Computes “virtual” lane feasibility based on live corridors. [file:18]                            |
| `vcyboquaticmicroplasticriskfacade`| AI‑facing facade        | `cyboquaticmicroplasticriskindex`                          | Read‑only microplastic and eco‑per‑joule facade with S_AI metrics. [file:18]                      |
| `vagentsafecatalog`                | AI‑facing facade        | `agentsafecatalog`, `agentsafediagnostics`, `agentsafeconsentguard` | Consolidated, non‑actuating AI tool surface w/ KER and consent gating. [file:18]                 |

These views are the only surfaces AI tools and external agents see; all writes and invariants live in the underlying tables and triggers. [file:18]

---

### 25.2 ALN shard catalog

ALN shards act as immutable specs for topology, corridors, policies, and function catalogs, all hex‑stamped and DID‑anchored. [file:18]

| ALN shard identifier                         | Domain / scope            | Identity anchor                                  | Core objectives                                                                                         |
|---------------------------------------------|---------------------------|--------------------------------------------------|----------------------------------------------------------------------------------------------------------|
| `alnPrometheusPraxisCore.v1.aln`            | System topology           | `bostrom18sd2ujv24...`                          | Global execution boundaries, core invariants, plane vocabulary. [file:18]                               |
| `ppx.function.meta.v1.aln`                  | Function metadata         | `bostrom18sd2ujv24...`                          | Canonical function registry (domain, lane, non‑actuating flag). [file:18]                               |
| `PrometheusPraxisCodingTaskList2026v1.aln`  | Task backlog              | `bostrom18sd2ujv24...`                          | Machine‑readable dev tasks with KER targets and acceptance gates. [file:18]                             |
| `econet.agentfunctioncatalog.v1.aln`        | Agent tooling             | `bostrom18sd2ujv24...`                          | ALN mirror of `vagentsafecatalog` for LLM/machine tool discovery. [file:18]                             |
| `AbsoluteDataSovereigntyPolicy2026v1.aln`   | Sovereignty policy        | `bostrom18sd2ujv24...`                          | Zero raw biosignal export, host ownership, telemetry family rules. [file:18]                            |
| `NeurorightsCorridor.aln`                   | Neurorights               | `bostrom18sd2ujv24...`                          | Differential privacy bounds and exclusion lists for neural metrics. [file:18]                           |
| `PlaneWeightsShard2026v1.aln`               | Plane weights             | `bostrom18sd2ujv24...`                          | Plane weights, corridor limits, non‑offsettable flags for safety planes. [file:18]                      |
| `ecosafety.nonactuatingworkload.v1.aln`     | Ecosafety policy          | `bostrom18sd2ujv24...`                          | Declares the traits a workload must satisfy to be “non‑actuating.” [file:18]                            |
| `ecosafety.riskvector.v2.aln`               | Risk modeling             | `bostrom18sd2ujv24...`                          | Defines risk vector components (`rcarbon`, `rbiodiv`) and corridor mappings. [file:18]                  |
| `CyboquaticBlastRadiusShard2026v1.aln`      | Cyboquatic safety         | `bostrom18sd2ujv24...`                          | Spatial and plane blast boundaries for cyboquatic nodes. [file:18]                                      |
| `phoenix.uhi.hex.risk.v1.aln`               | Urban heat (Phoenix)      | `bostrom18sd2ujv24...`                          | Risk coordinates for urban heat hexes (thermal, carbon, atmospheric). [file:18]                         |
| `phoenix.mar.corridor.v1.aln`               | Aquifer governance        | `bostrom18sd2ujv24...`                          | Drawdown/recharge/filtration corridors for Phoenix MAR systems. [file:18]                               |
| `NeuromorphicCorridorAlign2026v1.aln`       | Material alignment        | `bostrom18sd2ujv24...`                          | Maps neuromorphic substrates to environmental compatibility corridors. [file:18]                        |
| `prometheus-role-bands.v1.aln`              | Access control            | `bostrom18sd2ujv24...`                          | Role band hierarchy (SPINE, ENGINE, MATERIAL, GOV, APP) vs lanes. [file:18]                             |
| `prometheus-shard-layout.v1.aln`            | Shard topology            | `bostrom18sd2ujv24...`                          | Parent/child shard relationships and trust‑band layouts. [file:18]                                      |

Every shard concludes with `ALN.SPECHASHHEX` and DID fields, giving cryptographic provenance and ensuring that governance changes are versioned, auditable, and lane‑aware. [file:18]

---

## 26. Appendix C: Crate, Module, and FFI Inventory

### 26.1 Rust crates (non‑actuating governance spine)

All crates use Rust 2024, `rust-version = "1.85"`, `#![forbid(unsafe_code)]` (except minimal FFI boundaries), and `kani-verifier = "0.67"` for formal proofs. [file:18]

| Crate name                         | Workspace path                       | Primary technical responsibility                                                                                 | Proof / verification target                                                                                     |
|------------------------------------|--------------------------------------|------------------------------------------------------------------------------------------------------------------|------------------------------------------------------------------------------------------------------------------|
| `prometheuspraxis`                | `crates/prometheuspraxis`           | Macro‑kernel: evaluates system‑wide KER/RoH/Vt and emits `Allow/Derate/Stop` for abstract workloads. [file:18]  | Proves `Stop` is monotonically returned whenever a hard corridor is breached. [file:18]                          |
| `prometheuspraxisai`              | `crates/prometheuspraxisai`         | S_AI always‑improve scoring, non‑offsettable plane guards, C/FFI shims for analytics engines. [file:18]         | Proves `safetopromote = false` when ΔV>0 or RoH exceeds ceiling. [file:18]                                      |
| `prometheus-praxis-lyapunov-guard`| `crates/prometheus-praxis-lyapunov-guard` | Lyapunov residual types and invariants; computes windowed ΔV and consistency checks. [file:18]                  | Verifies that `meanvtafter > meanvtbefore` cannot be flagged as stable. [file:18]                                |
| `econet-governance-spine`         | `crates/econet-governance-spine`    | Governance spine: typed SQLite accessors, `SchemaVerifier`, `KerUpgradeGuard`, `LaneGuard`, `Mt6883Guard`. [file:18] | Proves KER monotonicity across lane changes and schema integrity vs `ExpectedSchema`. [file:18]                  |
| `cyboquatic-blastradius-spine`    | `crates/cyboquatic-blastradius-spine` | Read‑only diagnostics over cyboquatic blastradius/workloads, JSON FFI for external tools. [file:18]             | Guarantees no actuation APIs are exposed; only evidence and diagnostics. [file:18]                               |
| `prometheus-praxis-cyboquatic`    | `crates/prometheus-praxis-cyboquatic` | Bridges Cyboquatic DB structures into EcoNet governance and KER kernels. [file:18]                              | Validates schema alignment and DID bindings for cyboquatic windows. [file:18]                                   |
| `sovereign-consent-engines`       | `crates/sovereign-consent-engines`  | Evaluates host envelopes to produce `TelemetryVerdict` for KER/RoH/LYAP/ECOHEALTH telemetry. [file:18]          | Proves no raw neural/biosignal fields appear in outputs when not authorized. [file:18]                          |
| `econet-index`                    | `crates/econet-index`               | Manages DB migrations, `ExpectedSchema` verification, and DefinitionRegistry indexing. [file:18]               | Asserts all required tables/views are present and no drift occurs. [file:18]                                     |
| `cyboquatic-ecosafety-core`       | `crates/cyboquatic-ecosafety-core`  | Risk vector primitives, microplastic metrics, eco‑per‑joule calculations. [file:18]                             | Ensures risk vectors stay in [0,1] and ecoperjoule math is numerically safe. [file:18]                           |
| `cyboquatic-core`                 | `crates/cyboquatic-core`            | Indexing, telemetry aggregation, and metrics export helpers. [file:18]                                          | Verifies memory safety and JSON serialization correctness. [file:18]                                             |

#### Always-improve kernel snippet (crate: `prometheuspraxisai`)

```rust
pub fn compute_always_improve_score(
    lane: ActionLane,
    ker: KerOutput,
    roh: RohSnapshot,
    lyap: LyapunovResidualSnapshot,
    cfg: AlwaysImproveConfig,
    roh_ceiling_global: Decimal,
    kmin: Decimal,
    emin: Decimal,
    rmax: Decimal,
) -> AlwaysImproveScore {
    if roh.rohscalar > roh_ceiling_global {
        return AlwaysImproveScore {
            score: Decimal::ZERO,
            safetopromote: false,
        };
    }

    let deltav = lyap.v_next - lyap.v_current;
    if deltav > cfg.maxdeltav {
        return AlwaysImproveScore {
            score: Decimal::ZERO,
            safetopromote: false,
        };
    }

    let r_k = if ker.kmetric >= kmin { Decimal::ZERO } else { kmin - ker.kmetric };
    let r_e = if ker.emetric >= emin { Decimal::ZERO } else { emin - ker.emetric };
    let r_r = if ker.rmetric <= rmax { Decimal::ZERO } else { ker.rmetric - rmax };

    let combined_penalty = (cfg.wk * r_k) + (cfg.we * r_e) + (cfg.wr * r_r);
    let raw_score = Decimal::ONE - combined_penalty;
    let score = raw_score.clamp(Decimal::ZERO, Decimal::ONE);

    let threshold_ok = ker.kmetric >= kmin && ker.emetric >= emin && ker.rmetric <= rmax;
    let safetopromote = score >= Decimal::new(70, 2) && threshold_ok;

    AlwaysImproveScore { score, safetopromote }
}
```
[file:18]

This keeps lane promotion bounded by KER thresholds, RoH ceilings, and ΔV constraints, independent of domain (water, heat, etc.). [file:18]

---

### 26.2 C++ telemetry headers and FFI entrypoints

C++ and C‑ABI interfaces are telemetry‑only; they ingest evidence and expose diagnostics but never expose actuator control fields. [file:18]

#### Example: wastewater pump telemetry header

```cpp
#ifndef WASTEWATER_PUMP_TELEMETRY_HPP
#define WASTEWATER_PUMP_TELEMETRY_HPP

#include <cstdint>

namespace prometheus::telemetry {

struct alignas(8) WastewaterPumpMetrics {
    uint64_t timestamp_utc;
    int64_t  machine_id;
    double   flow_rate_m3h;
    double   total_head_m;
    double   power_consumption_kw;
    double   vibration_rms_mms;
    double   temperature_celsius;
    double   suction_pressure_bar;
    double   discharge_pressure_bar;

    double   ker_k_metric;
    double   ker_e_metric;
    double   ker_r_metric;
    double   roh_scalar;
    uint32_t current_lane_code;
    uint64_t corridor_id;
};

extern "C" {
    int32_t ppx_ingest_pump_telemetry(const WastewaterPumpMetrics* metrics);
}

} // namespace prometheus::telemetry

#endif // WASTEWATER_PUMP_TELEMETRY_HPP
```
[file:18]

This header only defines observable state; no API exists for writing back control parameters to the pump. [file:18]

#### Representative FFI symbols (cdylib exports)

| Symbol / header                               | ABI / language | Domain                     | Payload / behavior                                                                                  | Sovereignty / governance gating                                                                    |
|-----------------------------------------------|----------------|----------------------------|-----------------------------------------------------------------------------------------------------|----------------------------------------------------------------------------------------------------|
| `wastewaterpumptelemetry.hpp`                | C++17          | Pumps & screens            | `WastewaterPumpMetrics` struct and `ppx_ingest_pump_telemetry` ingestion function. [file:18]       | Rejects telemetry if RoH or corridors are violated; write‑only into evidence ledgers. [file:18]    |
| `magnet_telemetry.hpp`                       | C++17          | Magnet separators          | `MagnetModuleMetrics` describing field intensity and throughput. [file:18]                          | Exposes only diagnostics; coil currents and actuator controls are out‑of‑scope. [file:18]         |
| `cybospine_list_shard_blastradius_json`      | C‑ABI          | Shard diagnostics          | Inputs: `db_path: *const c_char`; output: JSON string of `ShardBlastRadius`. [file:18]             | Only lists blast radii for shards present in AI‑safe catalog; no actuation keys. [file:18]        |
| `cybospine_summarize_workload_node_region_json` | C‑ABI      | Workload windows           | Inputs: `db_path`, `nodeid`, `region`; output: JSON `WorkloadNodeWindow`. [file:18]                | Returns aggregated windows, not raw events; respects consent & lane filters. [file:18]            |
| `ppx_accountability_log_append`              | C‑ABI          | Deployment accountability  | Inputs: `shard_id`, `ker_snapshot_json`, `signing_did`; output: status code. [file:18]             | Append‑only log; enforces valid DID and lane admissibility before accepting entries. [file:18]    |

#### Telemetry pipeline (unidirectional evidence flow)

- Step 1:
  - Hardware emits telemetry into C++ metric structs (e.g., `WastewaterPumpMetrics`). [file:18]
- Step 2:
  - C‑ABI ingress (`ppx_ingest_pump_telemetry`) forwards metrics into Rust governance crates (`econet-governance-spine`, `prometheuspraxisai`). [file:18]
- Step 3:
  - Governance logic evaluates KER, RoH, Lyapunov invariants and writes into SQLite ledgers (`cyboquaticworkloadledger`, `cybomachineryworkload`). [file:18]
- Step 4:
  - Batch jobs update `agentsafediagnostics`, recomputing S_AI, stability, and safetopromote flags. [file:18]
- Step 5:
  - AI‑safe views (`vagentsafecatalog`, `vcyboquaticmicroplasticriskfacade`) expose read‑only diagnostics to LLM tools and MCP servers. [file:18]

At no point do AI‑facing or FFI layers expose actuator interfaces; all control stacks remain sealed in separate, non‑exposed kernels. [file:18]
  - Cross‑language harnesses:
    - Lua, Android/Kotlin, and C clients:
      - Consume the same JSON FFI surfaces to visualize EcoWealth, blast radius, workload windows, and lane states, while leaving all actuation in external, sealed stacks. [file:18]
