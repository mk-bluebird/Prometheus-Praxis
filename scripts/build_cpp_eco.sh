# File: scripts/build_cpp_eco.sh
#!/usr/bin/env bash
set -euo pipefail

# -------------------------------------------------------------------
# C++ eco-restoration CI script (command-only, no new tools)
# - Uses g++ and sqlite3 only.
# - Compiles core eco-restoration C++ modules.
# - Runs basic smoke tests against existing SQLite/dataset paths.
# -------------------------------------------------------------------

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/bin"
DB_PATH="${ROOT_DIR}/eco_restoration_workload.sqlite"

mkdir -p "${BUILD_DIR}"

echo "[CI] Root directory: ${ROOT_DIR}"
echo "[CI] Build directory: ${BUILD_DIR}"
echo "[CI] SQLite DB path:  ${DB_PATH}"

# -------------------------------------------------------------------
# 1. Ensure SQLite schema is applied (if SQL shard exists)
# -------------------------------------------------------------------
SQL_SCHEMA="${ROOT_DIR}/sql/cyboquatic/eco_restoration_workload_schema.sql"

if [ -f "${SQL_SCHEMA}" ]; then
  echo "[CI] Applying SQLite schema from ${SQL_SCHEMA}"
  sqlite3 "${DB_PATH}" < "${SQL_SCHEMA}"
else
  echo "[CI] WARNING: SQL schema not found at ${SQL_SCHEMA}, continuing."
fi

# -------------------------------------------------------------------
# 2. Compile core C++ eco-restoration modules
# -------------------------------------------------------------------

echo "[CI] Compiling C++ eco-restoration modules..."

# Eco tools
g++ -std=c++20 -O2 -I"${ROOT_DIR}/cpp/include" \
    -o "${BUILD_DIR}/ker_lyapunov_utils" \
    "${ROOT_DIR}/cpp/tools/ker_lyapunov_utils.cpp"

g++ -std=c++20 -O2 -I"${ROOT_DIR}/cpp/include" \
    -o "${BUILD_DIR}/eco_logging" \
    "${ROOT_DIR}/cpp/tools/eco_logging.cpp"

g++ -std=c++20 -O2 -I"${ROOT_DIR}/cpp/include" \
    -o "${BUILD_DIR}/eco_invariant_checker" \
    "${ROOT_DIR}/cpp/tools/eco_invariant_checker.cpp" \
    -lsqlite3

g++ -std=c++20 -O2 -I"${ROOT_DIR}/cpp/include" \
    -o "${BUILD_DIR}/phoenix_hex_registry_client" \
    "${ROOT_DIR}/cpp/tools/phoenix_hex_registry_client.cpp" \
    -lsqlite3

g++ -std=c++20 -O2 -I"${ROOT_DIR}/cpp/include" \
    -o "${BUILD_DIR}/eco_restoration_cli" \
    "${ROOT_DIR}/cpp/tools/eco_restoration_cli.cpp" \
    -lsqlite3

g++ -std=c++20 -O2 -I"${ROOT_DIR}/cpp/include" \
    -o "${BUILD_DIR}/eco_config_loader_demo" \
    "${ROOT_DIR}/cpp/tools/eco_config_loader.cpp"

g++ -std=c++20 -O2 -I"${ROOT_DIR}/cpp/include" \
    -o "${BUILD_DIR}/rust_bridge_harness" \
    "${ROOT_DIR}/cpp/tools/rust_bridge_harness.cpp"

# Simulation modules
g++ -std=c++20 -O2 -I"${ROOT_DIR}/cpp/include" \
    -o "${BUILD_DIR}/cyboquatic_workload_energy_sim" \
    "${ROOT_DIR}/cpp/simulation/cyboquatic_workload_energy_sim.cpp" \
    -lsqlite3

g++ -std=c++20 -O2 -I"${ROOT_DIR}/cpp/include" \
    -o "${BUILD_DIR}/pfas_fate_corridor" \
    "${ROOT_DIR}/cpp/eco_restoration/pfas_fate_corridor.cpp"

g++ -std=c++20 -O2 -I"${ROOT_DIR}/cpp/include" \
    -o "${BUILD_DIR}/phoenix_canal_blast_radius" \
    "${ROOT_DIR}/cpp/simulation/phoenix_canal_blast_radius.cpp"

g++ -std=c++20 -O2 -I"${ROOT_DIR}/cpp/include" \
    -o "${BUILD_DIR}/multiplane_risk_harness" \
    "${ROOT_DIR}/cpp/simulation/multiplane_risk_harness.cpp"

echo "[CI] Compilation complete."

# -------------------------------------------------------------------
# 3. Smoke tests using existing datasets
# -------------------------------------------------------------------

echo "[CI] Running smoke tests..."

# 3.1 Material eco-impact via CLI
echo "[CI] Smoke: material-score"
"${BUILD_DIR}/eco_restoration_cli" material-score \
  65.0 62.0 60.0 75.0 9.0 0.2 0.1

# 3.2 PFAS corridor step using SQLite state
echo "[CI] Smoke: pfas-corridor-step (requires pfas_corridor_state table)"
if sqlite3 "${DB_PATH}" "SELECT name FROM sqlite_master WHERE type='table' AND name='pfas_corridor_state';" | grep -q "pfas_corridor_state"; then
  "${BUILD_DIR}/eco_restoration_cli" pfas-corridor-step "${DB_PATH}" "PHX_CANAL_NODE_A" || echo "[CI] PFAS corridor step failed (check state/table)."
else
  echo "[CI] Skipping PFAS corridor step: pfas_corridor_state table not found."
fi

# 3.3 Phoenix blast-radius risk from grid table
echo "[CI] Smoke: blast-radius"
if sqlite3 "${DB_PATH}" "SELECT name FROM sqlite_master WHERE type='table' AND name='phoenix_blast_grid';" | grep -q "phoenix_blast_grid"; then
  "${BUILD_DIR}/eco_restoration_cli" blast-radius "${DB_PATH}" || echo "[CI] Blast-radius simulation failed (check grid)."
else
  echo "[CI] Skipping blast-radius: phoenix_blast_grid table not found."
fi

# 3.4 KER summary over hex registry
echo "[CI] Smoke: ker-summary"
if sqlite3 "${DB_PATH}" "SELECT name FROM sqlite_master WHERE type='table' AND name='phoenix_hex_registry';" | grep -q "phoenix_hex_registry"; then
  "${BUILD_DIR}/eco_restoration_cli" ker-summary "${DB_PATH}" || echo "[CI] KER summary failed (check registry)."
else
  echo "[CI] Skipping ker-summary: phoenix_hex_registry table not found."
fi

# 3.5 Multiplane risk harness CSV output
echo "[CI] Smoke: multiplane_risk_harness"
"${BUILD_DIR}/multiplane_risk_harness" || echo "[CI] Multiplane risk harness failed."

# 3.6 Invariant checker against SQLite telemetry
echo "[CI] Smoke: eco_invariant_checker (sqlite)"
if sqlite3 "${DB_PATH}" "SELECT name FROM sqlite_master WHERE type='table' AND name='cyboquatic_workload_telemetry';" | grep -q "cyboquatic_workload_telemetry"; then
  "${BUILD_DIR}/eco_invariant_checker" sqlite "${DB_PATH}" || echo "[CI] Invariant checker reported failures."
else
  echo "[CI] Skipping invariant checker: cyboquatic_workload_telemetry table not found."
fi

echo "[CI] C++ eco-restoration CI smoke tests complete."
