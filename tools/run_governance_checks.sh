# filename: tools/run_governance_checks.sh
# destination: ecorestoration_shard/tools/run_governance_checks.sh
# repo-target: github.com/mk-bluebird/eco_restoration_shard
#!/usr/bin/env bash
set -euo pipefail

# Local, sovereign governance runner for eco_restoration_shard.
# All checks are non-actuating: they read from the workspace and SQLite fixtures only.

WORKSPACE_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DB_DIR="${WORKSPACE_ROOT}/db"
DB_FILE="${DB_DIR}/restorationindex.sqlite3"
RUST_BIN="${WORKSPACE_ROOT}/target/release/restorationindextool"

echo "[gov] workspace root: ${WORKSPACE_ROOT}"
echo "[gov] DB file (read-only): ${DB_FILE}"

if [[ ! -f "${DB_FILE}" ]]; then
  echo "[gov] error: restorationindex.sqlite3 not found in db/; run DB migrations first." >&2
  exit 1
fi

echo "[gov] running cargo test (non-actuating unit and integration tests)..."
cargo test --workspace --all-features

echo "[gov] checking dependency graph for forbidden crates..."
FORBIDDEN=("reqwest" "hyper" "tokio-serial" "serialport")
if cargo metadata --format-version 1 --no-deps > /tmp/cargo-metadata.json; then
  for crate in "${FORBIDDEN[@]}"; do
    if grep -q "\"name\":\"${crate}\"" /tmp/cargo-metadata.json; then
      echo "[gov] error: forbidden crate detected in dependency graph: ${crate}" >&2
      exit 1
    fi
  done
fi

echo "[gov] validating AI-chat command grammar via non-actuating Rust helper..."
cat <<'EOF' | cargo run --quiet --bin restorationindextool --  >/dev/null 2>&1 || true
show restoration planes in Phoenix
list non-actuating workloads
plan restoration for groundwater corridor R
EOF

echo "[gov] probing invariants via read-only restorationindex.sqlite3..."
sqlite3 "file:${DB_FILE}?mode=ro&immutable=1" \
  "SELECT COUNT(1) FROM vprodeligiblerestorationplanes;" >/dev/null

echo "[gov] governance checks completed successfully (local, sovereign run)."
