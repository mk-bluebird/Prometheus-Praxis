# Optimal Cyboquatic Aeration Control and DID-Based Provenance Verification

## Calculus-of-Variations Optimal Control (C++ Discretisation)

- `cpp/simulation/cyboquatic_aeration_optimal_control.cpp` implements a discretised version of an optimal control problem:
  - Objective:
    - Minimise `∫ [ker_e(t) + λ · E(t) · c_grid(t)] dt`, where:
      - `ker_e(t)` is eco-impact rate (negative is better).
      - `E(t)` is energy cost (proportional to aeration control `u(t)`).
      - `c_grid(t)` is time-varying grid carbon intensity.
      - `λ` is a weight balancing eco-impact and energy carbon cost.
  - State dynamics:
    - BOD `B(t)` evolves via temperature-dependent decay:
      - `dB/dt = -k(T(t)) B(t) + q_in(t)`, with `k(T) = k20 · θ^(T-20)`.
  - The Euler-Lagrange/Pontryagin optimality condition for control `u(t)` is derived from:
    - `∂H/∂u = 0`, with Hamiltonian `H = L + p f`, yielding:
      - `∂L/∂u + p ∂f/∂u = 0`.
- In the current implementation, a simplified gradient descent update on `u(t)` is used:
  - `dJ/du_i ≈ λ P_max c_grid_i dt`, leading to:
    - `u_i ← u_i - α dJ/du_i`, clipped to `[0,1]`.
  - This discretisation is suitable for model predictive control (MPC) loops that adjust aeration intensity over time to maintain carbon negativity under fluctuating grid carbon intensity.

## Governance DID Merkle Tree Provenance (Kotlin Client)

- `kotlin/src/main/kotlin/org/cyboquatic/provenance/MerkleVerificationClient.kt` outlines a lightweight provenance protocol:
  - Organisations periodically snapshot their SQLite eco-restoration databases.
  - Leaf hashes of snapshots are computed via a digest function and assembled into a Merkle tree:
    - Parent hashes `H(h_left || h_right)` form a root representing the snapshot set.
  - The governance particle DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` signs the Merkle root with its private key; this signed root is published.
- The verification client:
  - Computes the leaf hash of a given snapshot.
  - Uses a Merkle proof (sibling hashes and positions) to rebuild the root hash.
  - Verifies the root signature against the DID’s public key, confirming provenance.
- This Kotlin implementation provides a conceptual Merkle-based verification path for eco-restoration data across organisations, leveraging the governance DID to ensure that cyboquatic telemetry and restoration metrics are cryptographically linked to trusted sources without relying on disallowed hash nomenclature in the code.

Technical justification: The aeration control module transforms a calculus-of-variations optimal carbon-negativity problem into a discrete MPC-friendly algorithm in C++, with clear links between BOD dynamics, energy use, and grid carbon intensity. The DID-based Merkle verification client establishes a lightweight, multi-organizational provenance protocol for SQLite eco-restoration snapshots, ensuring data integrity and governance alignment through cryptographic roots anchored by the governance particle DID.
