# Prometheus-Praxis C++ Tools

This directory hosts Prometheus-Praxis C++ command-line tools and synapse bridges used for eco-restoration analytics, evaluation, and cross-language integration.

---

## 1. `ppx_eval_report_cli.cpp` — Phoenix Evaluation CLI

A self-contained C++17 program that:

- Builds Phoenix-bound instances of the advection kernel, MAR workload architecture, and streaming pipeline models.
- Prints a comparative ASCII matrix of components vs. seven scoring dimensions.
- Evaluates the integrated Phoenix stack for deployment eligibility and prints the result.
- Exports the same data as JSON (`eval_report.json`) and ALN (`eval_report.aln`) for downstream governance and audit.

### Scoring dimensions

The seven dimensions are:

- `KnowledgeFactor`
- `EcoImpact`
- `RiskOfHarm`
- `Robustness`
- `Sovereignty`
- `EnergyEfficiency`
- `GovernanceAlignment`

Each dimension is scored on a normalized range `[0.0, 1.0]` using conservative Phoenix thresholds.

### Compilation and running

```bash
g++ -std=c++17 -O2 -o ppx_eval_report_cli ppx_eval_report_cli.cpp
./ppx_eval_report_cli
```

Outputs:

- `eval_report.json` – machine-readable record of component profiles and integrated eligibility.
- `eval_report.aln` – ALN-format envelope suitable for Prometheus-Praxis governance shards.

---

## 2. `eco_synapse_cpp_bridge.cpp` — Eco Synapse C++ Bridge

C++ analytics bridge for KER-style eco scoring:

- Exposes a simple non-actuating function `extern "C" double eco_compute_simple_score(double k, double e, double r)` for optional JNI/shared-library use.
- Provides a CLI that prints a CSV header and single row:

  ```text
  K,E,R,s
  <K>,<E>,<R>,<s>
  ```

### Wiring

- **CLI + CSV**  
  Java and Kotlin tools run `eco_synapse_cpp_bridge` as a subprocess and parse its CSV output to obtain KER scalars for dashboards, governance tools, or AI-chat backends.

- **JNI / Shared Library (optional)**  
  The file can be compiled into a shared library, for example:

  ```bash
  g++ -std=c++17 -O2 -fPIC -shared -o libeco_synapse.so eco_synapse_cpp_bridge.cpp
  ```

  Java/Kotlin then loads `libeco_synapse.so` via `System.loadLibrary("eco_synapse")` and calls `eco_compute_simple_score` through a JNI binding.

- **MCP / EcoNet integration**  
  Registered in `mcp_file` / `mcp_tool` as a C++ `COMMAND` tool with endpoint type `CLI`, so AI agents can discover and invoke it in RESEARCH lanes.

---

## 3. Other C++ tools in `cpp/tools/`

This directory also contains supporting non-actuating CLIs, including:

- `eco_serialization.cpp` — writes eco metrics to JSONL and CSV for data lakes and Rust/C++ consumers.
- `eco_scenario_library_spec.cpp` — defines and parses INI-like scenario files for hex anchors, canal nodes, PFAS settings, and workloads.
- `non_rust_interop_catalog.cpp` — prints proven interop patterns between C++, Kotlin, Java, Lua, and ALN.
- `energy_carbon_metrics.cpp` — computes energy-efficiency and carbon-impact metrics (energyreqJ, energytailwindJ, carbon_savings_kg).
- `eco_extension_roadmap.cpp` — prints a roadmap of future soil health, biodiversity, and multiplane KER modules.

All tools:

- Use only the C++ standard library.
- Are non-actuating (analytics and reporting only).
- Are suitable for wiring into SQLite schemas, MCP tooling, Java/Kotlin/Lua dashboards, and AI-chat orchestration.

---

## 4. Production readiness and AI-chat usability

- Each CLI prints structured, parseable output (CSV, JSON, ALN, or clear text) suitable for GitHub, AI-chat tools, and automation.
- Shared library hooks (`extern "C"`) are kept minimal and stable for cross-language use.
- Directory paths follow the mono-repo layout (`cpp/tools/`), making wiring predictable for build scripts and MCP/SQLite registries.
