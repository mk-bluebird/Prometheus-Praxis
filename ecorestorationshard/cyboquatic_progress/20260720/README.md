<!-- filename: ecorestorationshard/cyboquatic_progress/20260720/README.md
     destination: ecorestorationshard/cyboquatic_progress/20260720/README.md
     repo-target: https://github.com/mk-bluebird/Prometheus-Praxis -->

# Cyboquatic Progress 2026-07-20 – Blast-Radius Surcharge (Domain g)

- Domain g focuses on blast-radius tables for surcharge breaches using SQLite indices plus C++/Java modeling and telemetry, aligned with the Phoenix cyboquaticprogress spec.[file:2]
- This shard for 20260720 adds:
  - `sql/cyboquatic_dailyprogress_20260720.sql` defining `dailyprogress`, `blastradius_surcharge`, and KER/FOG/Canal parameter tables with strict invariants.[file:2][file:13]
  - `cpp/blastradius_model_20260720.cpp` providing a non-actuating C++ model for surcharge-dependent breach probability and radius.[file:13]
  - `java/BlastRadiusTelemetry20260720.java` writing blast-radius samples into the shared `dbcyboquaticdailyprogress.sqlite` evidence DB.[file:2]
  - `kotlin/BlastRadiusView20260720.kt` exposing an agent-friendly view that joins KER metrics with blast-radius rows for a given day.[file:2][file:13]
  - `lua/blastradius_cli_20260720.lua` offering a low-power CLI for listing surcharge envelopes from SQLite.[file:2]
  - `aln/BlastRadiusGovernance20260720.aln` binding the shard to DID `bostrom18sd2ujv24ual9c9pshtxys6j8knh6xaead9ye7` with explicit K,E,R and a contractive Lyapunov hint.[file:13]

- File placement follows the Phoenix Hex Anchors strategy:
  - Root: `ecorestorationshard/cyboquatic_progress/20260720` for the YYYYMMDD shard.[file:2]
  - Language subfolders: `cpp`, `java`, `kotlin`, `lua`, `sql`, `aln`.[file:2]
  - Evidence rows in SQLite must be registered against a Phoenix hex anchor in `Eco-Fort/db/phoenixhexregistry.sql` and mirrored in `ecorestorationshard/hex/PHXCYBOHEXANCHORS.md`.[file:2]

- All artifacts are non-actuating, diagnostic-only, and designed to:
  - Tighten eco-safety corridors by quantifying surcharge blast envelopes in KER space.
  - Support energy-efficient cyboquatic machinery planning where canal nodes and FOG media are treated as eco-industrial assets rather than uncontrolled risk sources.[file:12][file:21]
