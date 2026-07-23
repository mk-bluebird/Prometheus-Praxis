<!-- filename: CONTRIBUTING.md -->
<!-- destination: github.com/mk-bluebird/Prometheus-Praxis -->

# Contributing to Prometheus‑Praxis

Prometheus‑Praxis is a GitHub‑first, kernel‑first eco‑planner for Cyboquatic and related restoration stacks.[file:32]

- All Rust crates are non‑actuating by default; they compute recognition metrics, ecosafety distances, and CEIM/KER scalars that are later gated by ALN `safesteprule` and `deploydecisionkernel` contracts before any actuation occurs.[file:32]
- All changes must keep the repository machine‑readable and zero‑guesswork: paths, filenames, and schemas must allow agents to infer meaning directly from the filesystem and ALN indices.[file:32]

## Workflow

- Fork the repo and create topic branches per feature.
- Run `cargo test` and the CI workflow locally, including `cargo hack` and `cargo deny`, before opening a PR.
- Keep `Frames.toml` and ALN manifests in sync with any new frames or ecosafety coordinates you introduce.

## Coding guidelines

- Rust edition 2024, `rust-version = "1.85"`, `#![forbid(unsafe_code)]` in all crates.
- Kani remains mandatory for verification crates; do not make it optional or change its version without an explicit governance decision.
- New metrics or frames must:
  - Be non‑actuating in this crate.
  - Be representable in ALN (names, ranges, corridors) so they can be governed by the same Lyapunov and K/E/R envelopes as existing ecosafety metrics.[file:32]

---

## ALN Spec Structure

ALN (Augmented Ledger Notation) files define governance particles, corridors, KER scores, and shard layouts. Follow these conventions:

### File Naming

- Use lowercase with hyphens or underscores: `eco_safety_plane.v1.aln`, `corridor-blast-radius-2026v1.aln`
- Include version suffix: `.v1.aln`, `.v2.aln`
- Prefix with domain when applicable: `identity_`, `ker_`, `shard_`, `corridor_`

### Spec Sections

A well-structured ALN spec contains:

```aln
-- Record definitions (schema)
record ParticleName {
    field_name = type
}

-- Instance data
row ParticleName {
    field_name = "value"
}

-- Corridor definitions
corridor CorridorID {
    metric = "name"
    min = 0.0
    max = 1.0
}

-- KER scoring
ker_score {
    knowledge = 0.9
    eco_impact = 0.85
    risk_of_harm = 0.1
}
```

### Key Spec Categories

| Category | Prefix/Pattern | Purpose |
|----------|----------------|---------|
| Identity | `identity_*`, `ALE-IDENTITY-*` | DID bindings, brain-ID aliases, data sovereignty |
| Corridors | `corridor_*`, `*-corridor-*` | Ecosafety bounds, Lyapunov envelopes |
| KER | `ker_*`, `KER*`, `*-ker-*` | Knowledge/Eco/Risk scoring policies |
| Shard Layout | `shard_*`, `*-shard-*` | Data partitioning, zone assignments |
| Blast Radius | `blast_*`, `blastradius_*` | Impact scope definitions |
| Governance | `gov_*`, `governance_*` | Gate bindings, deployment kernels |

---

## Crate and Tool Naming Conventions

### Rust Crates

- Use `snake_case` for crate names: `eco_wealth_kernel`, `blast_radius_spine`
- Place under `/workspace/crates/` with descriptive names
- Each crate must have `README.md` explaining purpose and inputs/outputs

### Python Tools

- Use `snake_case`: `ppx_diagnostics.py`, `repo_quality_check.py`
- Place under `tools/`, `python/`, or `scripts/`
- Must run with native Python (no external packages beyond stdlib for core tools)

### Lua Scripts

- Use `snake_case`: `prometheus_shard_layout_plantuml.lua`
- Place under `tools/` or `lua/`
- Must run with standard Lua interpreter

---

## Running Validations

### Python Tests

```bash
python -m unittest discover tests/python
```

### Repo Quality Check

```bash
python tools/repo_quality_check.py
```

### Docstring Check

```bash
python tools/docstring_check.py
```

### Lua Shard Diagram Regeneration

```bash
lua tools/prometheus_shard_layout_plantuml.lua
```

### Ledger Verification

```bash
lua tools/sunflower_bee_corridor_ledger_verifier.lua
```

---

## First-Issue Checklist for New Collaborators

Welcome! Here's how to get started safely:

### Where to Start

1. **Read the docs**: Start with `README.md`, `ARCHITECTURE.md`, and `docs/ALN-SPECS.md`
2. **Pick a tagged issue**: Look for issues labeled `good-first-issue` or `help-wanted`
3. **Understand the ALN invariant**: Never modify ALN specs without understanding their consumers

### How to Add a New ALN Spec Safely

- [ ] Create the `.aln` file in the appropriate directory (`aln/`, `aln-specs/`, or domain-specific folder)
- [ ] Follow naming conventions (see above)
- [ ] Add a brief header comment with purpose and author
- [ ] Reference the spec in at least one tool or script (avoid orphaned files)
- [ ] Update `docs/ALN-SPECS.md` with the new spec's purpose and consumers
- [ ] Run `python tools/repo_quality_check.py` to verify no orphans or duplicates
- [ ] Run `python -m unittest discover tests/python` to ensure tests pass
- [ ] Open a PR with a clear description of what the spec governs

### Safe Files to Modify

- **Safe**: Python tools in `tools/`, `python/`, `scripts/`; documentation in `docs/`, `*.md`; example ALN files in `examples/`
- **Caution**: Core ALN specs in `aln/`, `core/aln/`; Rust crates in `crates/`
- **Avoid**: Generated files, database schemas under `db/` unless you understand the migration path

### AI-Chat Specific Guidelines

If you're an AI assistant contributing to this repo:

- [ ] Always respect ALN invariants (never weaken safety constraints)
- [ ] Avoid blacklisted primitives (Argon2, BLAKE, SHA3-256, "digital twins")
- [ ] Use native tools only (no `pip install`, no cargo for simple tasks)
- [ ] Prefer reading ALN specs over inferring behavior from code
- [ ] When in doubt, add a comment or docstring rather than making assumptions

---

## Quick Reference Commands

| Task | Command |
|------|---------|
| Run all Python tests | `python -m unittest discover tests/python` |
| Check for orphaned ALN files | `python tools/repo_quality_check.py` |
| Find missing docstrings | `python tools/docstring_check.py` |
| Regenerate shard diagrams | `lua tools/prometheus_shard_layout_plantuml.lua` |
| Verify bee corridor ledger | `lua tools/sunflower_bee_corridor_ledger_verifier.lua` |
| Build Rust workspace | `cargo build --workspace` |
| Run Rust tests | `cargo test --workspace` |

For a complete maintenance session checklist, see `docs/MAINTENANCE_SESSION.md`.
