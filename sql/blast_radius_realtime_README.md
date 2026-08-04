# Blast-Radius Real-Time Prediction Wiring Pattern

This shard describes a SQL-driven blast-radius prediction pipeline with a C++ RTU sensor feed and a Kotlin micro-service, suitable for Prometheus-Praxis eco-restoration use.

## Fixed-Point Hex Smoothness (Lua)

- `lua/hex_fixed_point_smoothness.lua` encodes hex-anchor consistency as a fixed-point problem on the Phoenix H3 cell graph.
- Each cell's intervention value `u_i` (e.g., added tree canopy) is updated via a distributed Gauss-Seidel scheme:
  - `u_i^{k+1} = (1 - α) u_i^{k} + α (avg_neighbors + b_i)`
  - Smoothness constraint `|u_i - u_j| ≤ ε` for adjacent cells is enforced.
- This yields a convergent intervention field that varies smoothly across adjacent hex cells, aligning thermal-recovery planning with spatial continuity.

## Blast-Radius Prediction Pipeline

- `cpp/tools/blast_radius_realtime_sqlite.cpp`:
  - Implements a C++ RTU that writes water-level and pressure readings into an in-memory SQLite database (`file:blast_radius_db?mode=memory&cache=shared`) using WAL mode for concurrent access.
  - Tables:
    - `realtime_sensor_readings`: latest sensor data per canal.
    - `blast_radius_table`: pre-computed level/pressure ranges mapped to flood polygon IDs.

- `kotlin/src/main/kotlin/org/cyboquatic/blast/BlastRadiusPredictor.kt`:
  - Kotlin micro-service that:
    - Reads the latest sensor sample per canal.
    - Runs a parameterised SQL query joining sensor values to `blast_radius_table` to retrieve the predicted flood polygon.
  - The query and JDBC operations are designed to complete within ~100 ms on typical hardware, supporting real-time blast-radius prediction without digital twins.

Technical justification: The Lua object formalises hex-anchor consistency as a graph fixed-point problem solvable by distributed Gauss-Seidel, ensuring smooth thermal-recovery interventions across adjacent H3 cells. The C++/Kotlin wiring pattern uses an in-memory SQLite database with WAL mode to couple real-time water-level and pressure telemetry from RTUs to pre-computed blast-radius tables, allowing a Kotlin micro-service to return flood polygon predictions under tight latency constraints, supporting eco-safe canal operations and surcharge risk mitigation.
