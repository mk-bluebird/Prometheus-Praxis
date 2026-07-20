# filename: ecorestorationshard/ppx_aln/README.md
# destination: ecorestorationshard/ppx_aln/README.md
# repo-target: https://github.com/mk-bluebird/Prometheus-Praxis
#
# Purpose
# Describe the ppx_aln directory, its subfolders, and how ALN v2 contracts
# compile together with C++ throughout the repository in a GitHub/AI-chat
# safe, non-actuating manner. [file:2][file:34]

## ppx_aln directory role

- Name: `ppx_aln` (Phoenix Prometheus ALN bridge). [file:34]
- Location: `ecorestorationshard/ppx_aln`.
- Role: Research-only, non-actuating bridge between ALN v2 contracts and C++ workloads in Prometheus-Praxis; hardware-facing repos import headers and ALN particles but live under EcoNet/Eco-Fort. [file:2][file:34]

## Subdirectories and files

- `aln/`
  - `ppx_aln_contracts.aln`
  - Contains ALN v2 particles for:
    - SafeStepRule (Lyapunov residual gates).
    - DeployDecisionKernel (K,E,R thresholds).
    - PlaneWeights (Lyapunov weights per plane).
    - RepoManifest (marks `ppx_aln` as RESEARCH, non-actuating). [file:34]
- `src/`
  - `aln_cpp_bridge.hpp`
  - C++ header defining:
    - `KerTriad`, `PlaneWeights`, `RiskPlanes`.
    - `SafeStepRule`, `DeployDecisionKernel`.
    - `compute_residual`, `safestep_holds`, `derive_ker`, `deploy_admissible`.
    - `is_non_research_directory` helper to distinguish research vs hardware paths. [file:2][file:34]
- `docs/`
  - `ppx_aln_usage.md`
  - Guidance for collaborators on:
    - How to call the C++ bridge from cyboquatic workload models.
    - How hardware repos import ALN contracts and bridge headers while keeping actuation outside ecorestorationshard. [file:34]

## How ALN and C++ compile together

- C++ workloads (e.g., `ecorestorationshard/cyboquaticprogress/YYYYMMDD/cpp/*.cpp`) include `aln_cpp_bridge.hpp` to:
  - Compute residual \(V(t)\) from risk planes and weights.
  - Derive K,E,R triads consistent with ALN contracts.
  - Check safe-step and deploy gates in non-actuating diagnostic code. [file:2][file:32][file:34]
- ALN particles in `aln/ppx_aln_contracts.aln` define the same structures:
  - SafeStepRuleRow epsilon/vt_ceiling aligned with C++ SafeStepRule.
  - DeployKernelRow K,E,R thresholds aligned with C++ DeployDecisionKernel.
  - PlaneWeightsRow weights aligned with C++ PlaneWeights. [file:34]
- CI can:
  - Parse ALN contracts.
  - Generate C++ constants or configuration files.
  - Verify that C++ bridge usage matches ALN invariants (nonnegative weights, K,E,R in [0,1], V(t+1) ≤ V(t) + epsilon). [file:34]

## Non-research directories and hardware wiring

- Inside Prometheus-Praxis:
  - `ecorestorationshard/ppx_aln` is always RESEARCH, non-actuating, per RepoManifestRow invariant. [file:2][file:34]
  - Any directory under `EcoNet/hardware` or `Eco-Fort/actuators` is considered non-research by `is_non_research_directory`, and must:
    - Import `aln_cpp_bridge.hpp` and ALN contracts as read-only policy.
    - Implement actual hardware wiring under separate governance repos. [file:34]
- GitHub/AI-chat safety:
  - AI-chat agents can:
    - Read ALN and C++ bridge files.
    - Propose new workloads or contracts.
    - Never introduce actuators inside `ecorestorationshard/ppx_aln`; any actuation code is confined to dedicated hardware repos that obey the same ALN gates. [file:2][file:34]

## Usage pattern for collaborators and agents

- For new cyboquatic workloads:
  - Define risk planes and weights in C++ using `RiskPlanes` and `PlaneWeights`.
  - Compute residual and KER via `compute_residual` and `derive_ker`.
  - Apply `safestep_holds` and `deploy_admissible` to classify workloads as admissible or research-only. [file:2][file:32]
- For ALN governance:
  - Extend `ppx_aln_contracts.aln` with new particles as needed.
  - Keep RepoManifest rows RESEARCH/nonactuating for Prometheus-Praxis.
  - Use phoenixhexregistry and DID bindings to anchor new contracts. [file:2][file:36]
