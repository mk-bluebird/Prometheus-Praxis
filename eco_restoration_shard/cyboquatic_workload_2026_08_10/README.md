<!-- File: eco_restoration_shard/cyboquatic_workload_2026_08_10/README.md -->
# Cyboquatic Workload — 2026-08-10

This artifact set implements domain (d): a telemetry-derived `energyreqJ` and `deltaVt` corridor for canal and water-restoration machinery.

- C++ computes a 60-second hydraulic energy demand, KER-compatible residual, knowledge factor, and ecological-impact score.
- Java and Kotlin provide interoperable machinery and routing logic.
- Lua provides an inert FOG-aware maintenance admission predicate.
- SQLite persists indexed canal-node telemetry with KER, FOG, and canal parameters enforced by constraints and an insertion trigger.
- ALN v2 records the same advisory-only ecological operating corridor.

Compile C++: `c++ -std=c++20 -O2 cpp/eco_restoration/cyboquatic_workload_2026_08_10.cpp -o cyboquatic_workload`
Compile Java: `javac -d out java/cyboquatic_workload_2026_08_10/CyboquaticWorkload.java`
