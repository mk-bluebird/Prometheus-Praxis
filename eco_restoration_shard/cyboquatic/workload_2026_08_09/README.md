<!-- File: eco_restoration_shard/cyboquatic/workload_2026_08_09/README.md -->
# Cyboquatic workload corridor

This artifact set evaluates hydraulic pumping energy demand (`energyreqJ`) and a bounded operational residual (`deltaVt`) for ecological canal-restoration machinery.

- `cpp/` provides standalone C++20 command-line assessment logic.
- `java/` provides JVM telemetry assessment logic.
- `kotlin/` serializes validated workload frames.
- `lua/` routes frames through a non-actuating review gate.
- `sql/` persists KER, FOG, and canal-node telemetry with SQLite invariants.
- `aln/` expresses the admission corridor for governed analysis.

## Validation Commands

### Tool Versions
- **C++ Compiler:** g++ (Debian 12.2.0-14+deb12u1) 12.2.0
- **JDK:** OpenJDK 17.0.16
- **SQLite:** 3.40.1
- **Lua:** 5.4.4

### C++ Compile and Run
```bash
cd cpp/simulation
g++ -std=c++20 -O2 -Wall -Wextra -Wpedantic cyboquatic_workload_2026_08_09.cpp -o workload
./workload phoenix-canal-pump-01 0.035 4.2 0.78 900.0 2.1 0.82 0.000035 0.08
# Or use built-in sample (no arguments):
./workload
```

### Java Compile and Run
```bash
cd java/cyboquatic/src/main/java
javac org/prometheuspraxis/cyboquatic/WorkloadTelemetry.java
java org.prometheuspraxis.cyboquatic.WorkloadTelemetry phoenix-canal-pump-01 0.035 4.2 0.78 900.0 2.1 0.82 0.000035 0.08
# Or use built-in sample (no arguments):
java org.prometheuspraxis.cyboquatic.WorkloadTelemetry
```

### SQLite Schema Load, Fixture Load, and View Queries
```bash
cd /tmp
rm -f test_cyboquatic.db
sqlite3 test_cyboquatic.db < /workspace/sql/cyboquatic/workload_telemetry_2026_08_09.sql
sqlite3 test_cyboquatic.db ".read /workspace/eco_restoration_shard/cyboquatic/workload_2026_08_09/fixtures/accepted_frame.sql"
sqlite3 test_cyboquatic.db "SELECT COUNT(*) FROM v_cyboquatic_workload_admissible;"  # Expected: 1
sqlite3 test_cyboquatic.db "SELECT COUNT(*) FROM v_cyboquatic_workload_review;"     # Expected: 0
```

### Lua Gate Evaluation
```bash
lua5.4 -e '
local gate = dofile("/workspace/lua/cyboquatic/workload_gate.lua")
-- Accepted frame
local r1 = gate.evaluate({energyreqJ=1658368.6, deltaVt=0.044, knowledgeFactor=0.96, ecoImpactValue=0.81, fogConfidence=0.85})
print("accepted:", r1.accepted, "route:", r1.route)
-- FOG-rejected frame
local r2 = gate.evaluate({energyreqJ=1658368.6, deltaVt=0.044, knowledgeFactor=0.96, ecoImpactValue=0.81, fogConfidence=0.50})
print("accepted:", r2.accepted, "route:", r2.route, "reason:", r2.reason)
'
```

### ALN Corridor
The ALN file (`aln/cyboquatic/workload_corridor_2026_08_09.aln`) is a declarative contract consumed by the repository's existing ALN tooling. Do not add or install an ALN interpreter.

## Implementation Note (2026-08-10)

All validation commands passed successfully:
- C++ compiled with `-std=c++20 -O2 -Wall -Wextra -Wpedantic` and produced matching output for both default sample and CLI arguments.
- Java compiled and ran with identical output to C++.
- SQLite schema loaded without errors; fixture inserted one row; `v_cyboquatic_workload_admissible` returned 1 row; `v_cyboquatic_workload_review` returned 0 rows.
- Lua gate correctly accepted valid frames and rejected FOG-failed frames with `human-review` route.
