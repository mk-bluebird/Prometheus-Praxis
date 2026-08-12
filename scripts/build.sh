#!/usr/bin/env bash
# File: scripts/build.sh

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build/bin}"
CXX="${CXX:-g++}"
CXXFLAGS=(-std=c++20 -O2 -Wall -Wextra -Wpedantic)

TARGET_DIRS=(
  "cpp/eco_restoration"
  "cpp/simulation"
  "cpp/tools"
  "lua/eco_restoration"
  "sql/eco_restoration"
)

declare -A SOURCE_PACKAGES=(
  [hex_anchor_geodesic_calibration]="eigen3 sqlite3 proj"
  [canal_lyapunov_calibration]="eigen3"
  [delayed_cbf_ker_sensitivity]="sqlite3"
  [nsga_lane_threshold_tuning]="eigen3 sqlite3"
  [battery_chance_constrained_scheduler]="sqlite3"
  [carbon_and_water_risk_models]="eigen3 sqlite3"
)

need_command() {
  command -v "$1" >/dev/null 2>&1
}

pkg_flags() {
  local package
  for package in "$@"; do
    if ! need_command pkg-config || ! pkg-config --exists "$package"; then
      echo "Missing required package metadata: $package" >&2
      return 1
    fi
    pkg-config --cflags --libs "$package"
  done
}

compile_cpp() {
  local source="$1"
  local name
  local executable
  local packages
  local flags=()

  name="$(basename "${source%.cpp}")"
  executable="$BUILD_DIR/$name"
  packages="${SOURCE_PACKAGES[$name]:-}"

  if [[ -f "$executable" && "$executable" -nt "$source" ]]; then
    echo "UP-TO-DATE  $name"
    return 0
  fi

  if [[ -n "$packages" ]]; then
    read -r -a flags <<< "$(pkg_flags $packages)"
  fi

  echo "COMPILE      $name"
  "$CXX" "${CXXFLAGS[@]}" "$source" -o "$executable" "${flags[@]}"
}

build_cpp() {
  if ! need_command "$CXX"; then
    echo "Compiler unavailable: $CXX" >&2
    return 1
  fi

  mkdir -p "$BUILD_DIR"

  local directory
  local source
  for directory in "cpp/eco_restoration" "cpp/simulation" "cpp/tools"; do
    [[ -d "$REPO_ROOT/$directory" ]] || continue
    while IFS= read -r -d '' source; do
      compile_cpp "$source"
    done < <(find "$REPO_ROOT/$directory" -type f -name '*.cpp' -print0 | sort -z)
  done
}

validate_lua() {
  local directory="$REPO_ROOT/lua/eco_restoration"
  [[ -d "$directory" ]] || return 0

  if ! need_command luac; then
    echo "SKIP LUA     luac is unavailable"
    return 0
  fi

  local file
  while IFS= read -r -d '' file; do
    echo "LUA CHECK    ${file#$REPO_ROOT/}"
    luac -p "$file"
  done < <(find "$directory" -type f -name '*.lua' -print0 | sort -z)
}

validate_sql() {
  local directory="$REPO_ROOT/sql/eco_restoration"
  [[ -d "$directory" ]] || return 0

  if ! need_command sqlite3; then
    echo "SKIP SQL     sqlite3 is unavailable"
    return 0
  fi

  local file
  while IFS= read -r -d '' file; do
    echo "SQL CHECK    ${file#$REPO_ROOT/}"
    sqlite3 ":memory:" < "$file"
  done < <(find "$directory" -type f -name '*.sql' -print0 | sort -z)
}

run_tests() {
  local ctest_file="$REPO_ROOT/build/CTestTestfile.cmake"

  if [[ ! -f "$ctest_file" ]]; then
    echo "SKIP TEST    CTest metadata is absent"
    return 0
  fi

  if ! need_command ctest; then
    echo "CTest is unavailable" >&2
    return 1
  fi

  ctest --test-dir "$REPO_ROOT/build" --output-on-failure
}

inventory() {
  local directory
  local count

  echo "Repository: $REPO_ROOT"
  for directory in "${TARGET_DIRS[@]}"; do
    if [[ -d "$REPO_ROOT/$directory" ]]; then
      count="$(find "$REPO_ROOT/$directory" -type f \
        \( -name '*.cpp' -o -name '*.hpp' -o -name '*.lua' -o -name '*.sql' \) | wc -l | tr -d ' ')"
      echo "INVENTORY    $directory: $count files"
    fi
  done
}

usage() {
  cat <<'EOF'
Usage: scripts/build.sh <inventory|build|validate|test|all>

Commands:
  inventory  List source files in known eco-restoration directories.
  build      Compile C++20 executables using already available dependencies.
  validate   Run Lua and SQLite syntax validation when local tools exist.
  test       Run CTest only when a configured CTest build exists.
  all        Run inventory, build, validate, and test.

This script does not install tools and does not run Cargo commands.
EOF
}

main() {
  cd "$REPO_ROOT"

  case "${1:-all}" in
    inventory)
      inventory
      ;;
    build)
      inventory
      build_cpp
      ;;
    validate)
      validate_lua
      validate_sql
      ;;
    test)
      run_tests
      ;;
    all)
      inventory
      build_cpp
      validate_lua
      validate_sql
      run_tests
      ;;
    *)
      usage >&2
      return 2
      ;;
  esac
}

main "$@"
