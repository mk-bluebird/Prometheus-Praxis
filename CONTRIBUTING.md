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
