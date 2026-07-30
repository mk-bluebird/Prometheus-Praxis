## Ecosafety Core v2 Spine

This directory hosts shared, non-actuating governance and Lyapunov core artifacts that all cyboquatic daily shards must depend on:

- `sql/ker_lyapunov_core.sql`  
  Canonical SQLite schema for risk planes, residual windows, K,E,R triads, and global corridor triggers.

- `cpp/ker_residual_core.hpp`  
  Header-only C++ kernel for computing Lyapunov residuals \(V_t\) and K,E,R from normalized risk vectors.

- `aln/obligations/AlwaysImproveResidual2026v1.aln`  
  ALN v2 obligation schema expressing the “always improve” residual proof requirement and lane gating.

These artifacts are non-actuating, hex‑anchored, and bound to Bostrom DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7`, forming the constitutional spine for superintelligence‑safe cyboquatic work.[4][15]
