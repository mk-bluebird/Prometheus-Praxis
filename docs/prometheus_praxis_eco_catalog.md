# Prometheus‑Praxis Eco‑Module Catalog

This catalog maps existing Rust crate rolebands (SPINE, RESEARCH, ENGINE, MATERIAL, GOV, APP) to corresponding C++ eco‑restoration modules, highlighting gaps and opportunities for further C++ coverage.[78][59]

## 1. Rolebands and Rust Eco Crates

MCP/EcoNet uses `roleband` in `mcp_repo` to classify repositories:[78]

- **SPINE**: Core knowledge graph, KER semantics, Lyapunov residuals, governance overlays.
- **RESEARCH**: Experimental eco models, simulations, corridor design, PFAS fate studies.
- **ENGINE**: Execution engines and workloads (hydraulics, energy kernels, schedulers).
- **MATERIAL**: Material eco‑impact scoring, biodegradability indices, PFAS/material interactions.
- **GOV**: Governance particles, KER policies, ALN specs, DID bindings.
- **APP**: Application‑level tools, dashboards, CLIs, AI‑chat playgrounds.

Representative Rust eco crates (naming patterns):

- `eco_spine_ker`, `eco_spine_graph` (SPINE)
- `eco_pfas_corridor`, `eco_blast_pde`, `phoenix_hex_sim` (RESEARCH/ENGINE)
- `eco_materials`, `eco_iso_oecd`, `eco_pfasmaterial` (MATERIAL)
- `eco_gov_aln`, `eco_gov_ker`, `eco_hex_registry` (GOV)
- `eco_app_cli`, `eco_app_dashboard`, `econet_mcp_tools` (APP)

## 2. C++ Module Mapping

### SPINE (KER, Lyapunov, Hex Registry)

- Rust: `eco_spine_ker`, `eco_spine_graph`.
- C++:
  - `cpp/tools/ker_lyapunov_utils.cpp` (KER scoring, Lyapunov residuals).[59]
  - `cpp/tools/phoenix_hex_registry_client.cpp` (hex risk aggregation).[59]
  - `cpp/include/eco_restoration.hpp` (aggregated interfaces).[59]
- Gap: C++ KER history / governance verdict reader (could mirror `governance_verdict`).

### RESEARCH / ENGINE (PFAS, Blast‑Radius, Workloads)

- Rust: `eco_pfas_corridor`, `eco_blast_pde`, `phoenix_hex_sim`.
- C++:
  - `cpp/eco_restoration/pfas_fate_corridor.cpp` (PFAS fate corridor library).[59]
  - `cpp/simulation/phoenix_canal_blast_radius.cpp` (blast‑radius PDE approximation).[59]
  - `cpp/simulation/cyboquatic_workload_energy_sim.cpp` (workload energy and ΔVt).[59]
  - `cpp/simulation/multiplane_risk_harness.cpp` (Monte‑Carlo across planes).[59]
- Gap: More detailed hydraulic stress metrics (shear, vortex) and coupled PFAS mass‑load kernels.

### MATERIAL (Eco‑Impact Scoring)

- Rust: `eco_materials`, `eco_iso_oecd`.
- C++:
  - `cpp/eco_restoration/material_eco_impact.cpp` (eco‑impact engine).[59]
  - `sql/eco_net/eco_material_catalog_schema.sql` (material catalog).[66][72][75]
  - `kotlin/eco/MaterialImpactCli.kt` (catalog + C++ CLI bridge).[59]
- Gap: C++ material ingestion from lab datasets and direct linkage to canal placement policies.

### GOV (Governance, ALN, KER Particles)

- Rust: `eco_gov_aln`, `eco_gov_ker`, `eco_hex_registry`.
- C++ / ALN:
  - `aln/cyboquatic/eco_multilang_binding.aln2` (bindings non‑actuating, KER‑preserving).[59]
  - `aln/cyboquatic/qpudatashard_ker_pfas_refined.aln2` (PFAS KER corridor invariants).[59]
  - `cpp/tools/aln_conformance_checker.cpp` (C++ vs ALN corridor conformance).[59]
- Gap: C++ governance‑verdict reader and reporter, and ALN‑driven CI hooks.

### APP (CLIs, Dashboards, AI‑Chat Playground)

- Rust: `eco_app_cli`, `econet_mcp_tools`.
- C++ / JVM / Lua:
  - `cpp/tools/eco_restoration_cli.cpp` (eco CLI front‑end).[59]
  - `kotlin/cyboquatic/EcoRestorationDashboard.kt` (dashboard CLI).[11][59]
  - `kotlin/cyboquatic/HexNavigator.kt` (hex anchor navigator).[11][78]
  - `java/eco/KerReportGenerator.java`, `java/eco/EcoSchemaValidator.java`.[11][78]
  - `sql/eco_net/v_ai_chat_eco_playground.sql` (AI‑chat playground view).[78]
  - `lua/eco/eco_repl.lua`, `lua/cyboquatic/fog_cli.lua`.[59]
- Gap: Unified eco app that orchestrates across C++/Rust/SQL/Lua via MCP tools and eco_index.json.

## 3. Opportunities

- Add SPINE‑level C++ readers for governance chains (`governance_verdict`) and plane weights history.
- Extend ENGINE/RESEARCH C++ modules with more detailed hydraulic stress and PFAS mass discharge models.
- Enrich MATERIAL with C++ ingestion of ISO/OECD time‑series curves (not just summary percentages).
- Tighten GOV alignment via more ALN v2 particles for workloads, blast‑radius, and hex promotion.
- Build APP‑level MCP tools that expose C++ eco modules through `mcp_endpoint` entries and `v_cpp_eco_tools`.
