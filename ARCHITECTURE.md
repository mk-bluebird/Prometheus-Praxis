<!-- filename: ARCHITECTURE.md -->
<!-- destination: github.com/mk-bluebird/Prometheus-Praxis -->

# Prometheus‑Praxis Architecture

Prometheus‑Praxis implements an eco‑planner superloop that links Rust frames, ALN policy, SQL storage, and Android/Web overlays into a single governance‑first system.[file:32]

## Repository Layout

This is a **mono-repo** containing multiple technology stacks:

```
/workspace/
├── aln/                    # ALN governance specs (core)
├── aln-specs/              # Additional ALN specifications
├── core/aln/               # Core ALN definitions
├── crates/                 # Rust crates (non-actuating by default)
│   ├── eco_wealth_kernel/  # Eco-wealth scoring
│   ├── blast_radius_spine/ # Blast radius calculations
│   └── ...
├── python/                 # Python tools and utilities
│   ├── ppx_diagnostics.py  # Unified diagnostics
│   ├── ppx_metrics.py      # Metrics collection
│   └── ppx_config.py       # Configuration utilities
├── tools/                  # Executable tools (Python, Lua, Shell)
│   ├── repo_quality_check.py    # ALN orphan/duplicate checker
│   ├── docstring_check.py       # Python docstring validator
│   ├── prometheus_shard_layout_plantuml.lua
│   └── sunflower_bee_corridor_ledger_verifier.lua
├── lua/                    # Lua scripts for visualizations
├── docs/                   # Documentation
│   ├── ALN-SPECS.md        # Index of all ALN specs
│   └── MAINTENANCE_SESSION.md
├── tests/python/           # Python unit tests
├── examples/               # Example scenarios and demos
│   ├── ker/                # KER score examples
│   └── corridors/          # Corridor spec examples
├── db/                     # SQL schemas and migrations
└── config/                 # Configuration files (TOML, YAML)
```

### Key Directories

| Directory | Contents | Primary Language |
|-----------|----------|------------------|
| `aln/`, `aln-specs/` | ALN governance particles, corridors, KER policies | ALN |
| `crates/` | Rust libraries (diagnostics, planning, governance math) | Rust |
| `python/` | Core Python utilities used across tools | Python |
| `tools/` | Executable scripts for validation and quality checks | Python/Lua/Shell |
| `lua/` | Visualization and ledger verification scripts | Lua |
| `docs/` | Documentation for collaborators | Markdown |

## Superloop Overview

- Data enters as qpudatashards (CSV/ALN, e.g. Arizona water, materials) validated by JSON Schema.[file:32]
- Rust crates (e.g. `cyboquatic-core`) compute:
  - Mass/energy kernels.
  - Ecosafety distances and risk coordinates.
  - K/E/R scores under CEIM/KER.[file:32]
- ALN contracts (`safesteprule`, `deploydecisionkernel`) gate any move or deployment based on Lyapunov residuals and K/E/R thresholds.[file:32]
- SQL/SQLite storage provides durable state for frames and windows.
- Android overlays and city maps render Cyboquatic overlays (heatmaps, nodes, corridors) using non‑actuating data from Rust crates.[file:32]

## Frames and FrameRegistry

- Frames (biodiversity, Lyapunov, ESPD, etc.) are diagnostic suites that:
  - Read from shard/SQL inputs.
  - Compute recognition metrics only.
  - Export their results as ALN‑ready shards, CSV, or metrics.[file:32]
- `Frames.toml` enables/disables frames without recompilation, and `FrameRegistry` is the Rust side that drives this selection.

## Shard Layout and PlantUML Diagrams

The shard layout defines how data and responsibilities are partitioned across zones. To regenerate the shard topology diagram:

```bash
# Using Lua script (no cargo required)
lua tools/prometheus_shard_layout_plantuml.lua
```

This reads `aln/prometheus-shard-layout.v1.aln` and embeds a PlantUML diagram into `ARCHITECTURE.md`.

See `docs/ALN-SPECS.md` for a complete index of shard-related specs and their consumers.

## Metrics and Observability

- The optional `metrics` feature on core crates exports:
  - Evaluation durations.
  - Condition numbers and ecosafety distances.
  - K/E/R aggregates.
- External Prometheus endpoints scrape these metrics via thin adapters; the core crates never expose HTTP themselves, preserving clear separation between kernels and transport.[file:49][file:32]

## Safety and Governance

- ALN grammar is the constitutional spine:
  - Every eco‑kernel, frame, and overlay is bound to ALN corridors and deployment kernels.
  - All superloop iterations must satisfy `V(t+1) ≤ V(t)` and K/E/R deployment thresholds before any actuation in physical systems.[file:32]
- This keeps the Prometheus‑Praxis loop mathematically grounded, auditable, and eco‑aligned, while allowing superintelligence‑scale orchestration under explicit, DID‑bound policy.

## For Collaborators

### Quick Start

1. **Read the docs**: Start with `README.md` and `CONTRIBUTING.md`
2. **Understand ALN specs**: See `docs/ALN-SPECS.md` for an index
3. **Run validations**: Use Python tools in `tools/` (no installations needed)

### Running Tests and Checks

```bash
# Python unit tests
python -m unittest discover tests/python

# Repo quality check (orphans, duplicates)
python tools/repo_quality_check.py

# Docstring coverage
python tools/docstring_check.py

# Regenerate shard diagrams
lua tools/prometheus_shard_layout_plantuml.lua
```

For a complete maintenance session checklist, see `docs/MAINTENANCE_SESSION.md`.
