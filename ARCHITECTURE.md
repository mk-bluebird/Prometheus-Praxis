<!-- filename: ARCHITECTURE.md -->
<!-- destination: github.com/mk-bluebird/Prometheus-Praxis -->

# Prometheus‑Praxis Architecture

Prometheus‑Praxis implements an eco‑planner superloop that links Rust frames, ALN policy, SQL storage, and Android/Web overlays into a single governance‑first system.[file:32]

## Superloop overview

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

## Metrics and observability

- The optional `metrics` feature on core crates exports:
  - Evaluation durations.
  - Condition numbers and ecosafety distances.
  - K/E/R aggregates.
- External Prometheus endpoints scrape these metrics via thin adapters; the core crates never expose HTTP themselves, preserving clear separation between kernels and transport.[file:49][file:32]

## Safety and governance

- ALN grammar is the constitutional spine:
  - Every eco‑kernel, frame, and overlay is bound to ALN corridors and deployment kernels.
  - All superloop iterations must satisfy `V(t+1) ≤ V(t)` and K/E/R deployment thresholds before any actuation in physical systems.[file:32]
- This keeps the Prometheus‑Praxis loop mathematically grounded, auditable, and eco‑aligned, while allowing superintelligence‑scale orchestration under explicit, DID‑bound policy.
