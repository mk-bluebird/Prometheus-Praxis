<!-- File: eco_restoration_shard/cyboquatic/workload_2026_08_09/README.md -->
# Cyboquatic workload corridor

This artifact set evaluates hydraulic pumping energy demand (`energyreqJ`) and a bounded operational residual (`deltaVt`) for ecological canal-restoration machinery.

- `cpp/` provides standalone C++20 command-line assessment logic.
- `java/` provides JVM telemetry assessment logic.
- `kotlin/` serializes validated workload frames.
- `lua/` routes frames through a non-actuating review gate.
- `sql/` persists KER, FOG, and canal-node telemetry with SQLite invariants.
- `aln/` expresses the admission corridor for governed analysis.

Compile C++ with `c++ -std=c++20 -O2 cyboquatic_workload_2026_08_09.cpp -o workload`.
Compile Java with `javac WorkloadTelemetry.java`; run with `java org.prometheuspraxis.cyboquatic.WorkloadTelemetry`.
