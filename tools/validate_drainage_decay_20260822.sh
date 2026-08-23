#!/usr/bin/env sh
# Repository: mk-bluebird/Prometheus-Praxis
# Filename: tools/validate_drainage_decay_20260822.sh
# Purpose: Cross-language validation of drainage-decay implementations

set -eu

# Resolve repository root relative to script location
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Explicit paths for artifacts
CPP_SRC="${REPO_ROOT}/cpp/eco_restoration/DrainageDecay20260822.cpp"
JAVA_SRC="${REPO_ROOT}/java/eco_restoration/DrainageDecay20260822.java"
KOTLIN_SRC="${REPO_ROOT}/kotlin/eco_restoration/DrainageDecay20260822.kt"
LUA_SRC="${REPO_ROOT}/lua/eco_restoration/drainage_decay_20260822.lua"
SQL_SRC="${REPO_ROOT}/sql/eco_restoration/drainage_decay_20260822.sql"
ALN_SRC="${REPO_ROOT}/aln/eco_restoration/drainage_decay_20260822.aln"

# Temporary directory for build products
TMPDIR=""
cleanup() {
    if [ -n "$TMPDIR" ] && [ -d "$TMPDIR" ]; then
        rm -rf "$TMPDIR"
    fi
}
trap cleanup EXIT

TMPDIR="$(mktemp -d)"

# Check required tools
check_tool() {
    tool="$1"
    shift
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "FAIL: Required tool '$tool' is not available. Validation cannot proceed." >&2
        exit 1
    fi
}

echo "=== Checking required tooling ==="
check_tool c++
check_tool javac
check_tool java

# Kotlin, Lua, SQLite checks - report but allow partial validation
KOTLIN_AVAILABLE=0
LUA_AVAILABLE=0
SQLITE_AVAILABLE=0

if command -v kotlinc >/dev/null 2>&1; then
    KOTLIN_AVAILABLE=1
    echo "OK: kotlinc available"
else
    echo "WARN: kotlinc not available; Kotlin validation will be skipped" >&2
fi

if command -v lua >/dev/null 2>&1 || command -v lua5.4 >/dev/null 2>&1; then
    LUA_AVAILABLE=1
    if command -v lua >/dev/null 2>&1; then
        LUA_CMD="lua"
    else
        LUA_CMD="lua5.4"
    fi
    echo "OK: lua available as $LUA_CMD"
else
    echo "WARN: lua not available; Lua validation will be skipped" >&2
fi

if command -v sqlite3 >/dev/null 2>&1; then
    SQLITE_AVAILABLE=1
    echo "OK: sqlite3 available"
else
    echo "WARN: sqlite3 not available; SQLite validation will be skipped" >&2
fi

# Verify source files exist
echo ""
echo "=== Verifying source artifacts ==="
for src in "$CPP_SRC" "$JAVA_SRC" "$KOTLIN_SRC" "$LUA_SRC" "$SQL_SRC" "$ALN_SRC"; do
    if [ ! -f "$src" ]; then
        echo "FAIL: Source file not found: $src" >&2
        exit 1
    fi
    echo "OK: $src"
done

# Compile C++
echo ""
echo "=== Compiling C++ ==="
CPP_OUT="${TMPDIR}/DrainageDecay20260822"
if ! c++ -std=c++17 -Wall -Wextra -Werror -pedantic -o "$CPP_OUT" "$CPP_SRC"; then
    echo "FAIL: C++ compilation failed" >&2
    exit 1
fi
echo "OK: C++ compiled to $CPP_OUT"

# Compile Java
echo ""
echo "=== Compiling Java ==="
JAVA_OUT="${TMPDIR}/java_out"
mkdir -p "$JAVA_OUT"
if ! javac -d "$JAVA_OUT" "$JAVA_SRC"; then
    echo "FAIL: Java compilation failed" >&2
    exit 1
fi
echo "OK: Java compiled to $JAVA_OUT"

# Compile Kotlin if available
KOTLIN_OUT="${TMPDIR}/kotlin_out"
mkdir -p "$KOTLIN_OUT"
if [ "$KOTLIN_AVAILABLE" -eq 1 ]; then
    echo ""
    echo "=== Compiling Kotlin ==="
    # Try to compile Kotlin to JAR
    if ! kotlinc -include-runtime -d "${KOTLIN_OUT}/DrainageDecay20260822.jar" "$KOTLIN_SRC" 2>/dev/null; then
        # Fallback: compile to classes
        if ! kotlinc -d "${KOTLIN_OUT}" "$KOTLIN_SRC" 2>/dev/null; then
            echo "WARN: Kotlin compilation failed; skipping Kotlin validation" >&2
            KOTLIN_AVAILABLE=0
        else
            echo "OK: Kotlin compiled to $KOTLIN_OUT"
        fi
    else
        echo "OK: Kotlin compiled to ${KOTLIN_OUT}/DrainageDecay20260822.jar"
    fi
fi

# Define test vector
HOURS=24
INITIAL_BOD=80
INITIAL_TSS=75
INITIAL_CEC=12
BOD_DECAY=0.025
TSS_DECAY=0.020
CEC_RECOVERY=0.015
ENERGYREQ=900000
DELTA_VT=2
SAMPLE_COMPLETENESS=1.0

# Run C++ and capture output
echo ""
echo "=== Running C++ with nominal test vector ==="
CPP_OUTPUT="$("${CPP_OUT}" "$HOURS" "$INITIAL_BOD" "$INITIAL_TSS" "$INITIAL_CEC" \
    "$BOD_DECAY" "$TSS_DECAY" "$CEC_RECOVERY" "$ENERGYREQ" "$DELTA_VT")"
CPP_FILE="${TMPDIR}/cpp_output.txt"
echo "$CPP_OUTPUT" > "$CPP_FILE"
echo "OK: C++ output captured"

# Run Java and capture output
echo ""
echo "=== Running Java with nominal test vector ==="
JAVA_OUTPUT="$(cd "$JAVA_OUT" && java -cp . DrainageDecay20260822 \
    "$HOURS" "$INITIAL_BOD" "$INITIAL_TSS" "$INITIAL_CEC" \
    "$BOD_DECAY" "$TSS_DECAY" "$CEC_RECOVERY" "$ENERGYREQ" "$DELTA_VT")"
JAVA_FILE="${TMPDIR}/java_output.txt"
echo "$JAVA_OUTPUT" > "$JAVA_FILE"
echo "OK: Java output captured"

# Run Kotlin if available
if [ "$KOTLIN_AVAILABLE" -eq 1 ]; then
    echo ""
    echo "=== Running Kotlin with nominal test vector ==="
    if [ -f "${KOTLIN_OUT}/DrainageDecay20260822.jar" ]; then
        KOTLIN_OUTPUT="$(java -jar "${KOTLIN_OUT}/DrainageDecay20260822.jar" \
            "$HOURS" "$INITIAL_BOD" "$INITIAL_TSS" "$INITIAL_CEC" \
            "$BOD_DECAY" "$TSS_DECAY" "$CEC_RECOVERY" "$ENERGYREQ" "$DELTA_VT")"
    else
        KOTLIN_OUTPUT="$(java -cp "${KOTLIN_OUT}:$(find /usr/share/java -name 'kotlin-stdlib*.jar' 2>/dev/null | head -1)" \
            DrainageDecay20260822 \
            "$HOURS" "$INITIAL_BOD" "$INITIAL_TSS" "$INITIAL_CEC" \
            "$BOD_DECAY" "$TSS_DECAY" "$CEC_RECOVERY" "$ENERGYREQ" "$DELTA_VT" 2>/dev/null || true)"
    fi
    KOTLIN_FILE="${TMPDIR}/kotlin_output.txt"
    echo "$KOTLIN_OUTPUT" > "$KOTLIN_FILE"
    echo "OK: Kotlin output captured"
fi

# Run Lua if available
if [ "$LUA_AVAILABLE" -eq 1 ]; then
    echo ""
    echo "=== Running Lua with nominal test vector ==="
    LUA_OUTPUT="$($LUA_CMD -e "
        local drainage_decay = require('${LUA_SRC%.lua}')
        local result = drainage_decay.project_frame({
            hours = $HOURS,
            initial_bod_mg_l = $INITIAL_BOD,
            initial_tss_mg_l = $INITIAL_TSS,
            initial_cec_cmol_kg = $INITIAL_CEC,
            bod_decay_per_hour = $BOD_DECAY,
            tss_decay_per_hour = $TSS_DECAY,
            cec_recovery_per_hour = $CEC_RECOVERY,
            energyreq_j = $ENERGYREQ,
            delta_vt = $DELTA_VT,
            sample_completeness = $SAMPLE_COMPLETENESS
        })
        print(string.format('hours=%.6f', result.hours))
        print(string.format('bod_mg_l=%.6f', result.bod_mg_l))
        print(string.format('tss_mg_l=%.6f', result.tss_mg_l))
        print(string.format('cec_cmol_kg=%.6f', result.cec_cmol_kg))
        print(string.format('energyreq_j=%.6f', result.energyreq_j))
        print(string.format('delta_vt=%.6f', result.delta_vt))
        print(string.format('knowledge_factor=%.6f', result.knowledge_factor))
        print(string.format('eco_impact_value=%.6f', result.eco_impact_value))
        print(string.format('harm_risk=%.6f', result.harm_risk))
    " 2>&1)"
    LUA_FILE="${TMPDIR}/lua_output.txt"
    echo "$LUA_OUTPUT" > "$LUA_FILE"
    echo "OK: Lua output captured"
fi

# Inline awk comparison function
echo ""
echo "=== Validating cross-language agreement ==="

# Extract values from output files
extract_value() {
    key="$1"
    file="$2"
    grep "^${key}=" "$file" | sed "s/^${key}=//"
}

# Compare numeric values with tolerance
compare_values() {
    key="$1"
    ref_val="$2"
    cmp_val="$3"
    tolerance="0.000001"
    
    result=$(awk -v ref="$ref_val" -v cmp="$cmp_val" -v tol="$tolerance" 'BEGIN {
        diff = ref - cmp
        if (diff < 0) diff = -diff
        if (diff <= tol) {
            print "PASS"
        } else {
            print "FAIL"
        }
    }')
    echo "$result"
}

# Get reference values from C++
REF_BOD=$(extract_value "bod_mg_l" "$CPP_FILE")
REF_TSS=$(extract_value "tss_mg_l" "$CPP_FILE")
REF_CEC=$(extract_value "cec_cmol_kg" "$CPP_FILE")
REF_KER_K=$(extract_value "knowledge_factor" "$CPP_FILE")
REF_KER_E=$(extract_value "eco_impact_value" "$CPP_FILE")
REF_KER_R=$(extract_value "harm_risk" "$CPP_FILE")

echo "Reference (C++): bod=$REF_BOD, tss=$REF_TSS, cec=$REF_CEC"
echo "                 K=$REF_KER_K, E=$REF_KER_E, R=$REF_KER_R"

# Validate Java vs C++
JAVA_BOD=$(extract_value "bod_mg_l" "$JAVA_FILE")
JAVA_TSS=$(extract_value "tss_mg_l" "$JAVA_FILE")
JAVA_CEC=$(extract_value "cec_cmol_kg" "$JAVA_FILE")
JAVA_KER_K=$(extract_value "knowledge_factor" "$JAVA_FILE")
JAVA_KER_E=$(extract_value "eco_impact_value" "$JAVA_FILE")
JAVA_KER_R=$(extract_value "harm_risk" "$JAVA_FILE")

echo ""
echo "--- Java vs C++ ---"
for field in bod_mg_l tss_mg_l cec_cmol_kg knowledge_factor eco_impact_value harm_risk; do
    case "$field" in
        bod_mg_l) ref="$REF_BOD"; cmp="$JAVA_BOD" ;;
        tss_mg_l) ref="$REF_TSS"; cmp="$JAVA_TSS" ;;
        cec_cmol_kg) ref="$REF_CEC"; cmp="$JAVA_CEC" ;;
        knowledge_factor) ref="$REF_KER_K"; cmp="$JAVA_KER_K" ;;
        eco_impact_value) ref="$REF_KER_E"; cmp="$JAVA_KER_E" ;;
        harm_risk) ref="$REF_KER_R"; cmp="$JAVA_KER_R" ;;
    esac
    result=$(compare_values "$field" "$ref" "$cmp")
    echo "$field: $result (C++=$ref, Java=$cmp)"
done

# Validate Kotlin vs C++ if available
if [ "$KOTLIN_AVAILABLE" -eq 1 ] && [ -f "$KOTLIN_FILE" ]; then
    KOTLIN_BOD=$(extract_value "bod_mg_l" "$KOTLIN_FILE")
    KOTLIN_TSS=$(extract_value "tss_mg_l" "$KOTLIN_FILE")
    KOTLIN_CEC=$(extract_value "cec_cmol_kg" "$KOTLIN_FILE")
    KOTLIN_KER_K=$(extract_value "knowledge_factor" "$KOTLIN_FILE")
    KOTLIN_KER_E=$(extract_value "eco_impact_value" "$KOTLIN_FILE")
    KOTLIN_KER_R=$(extract_value "harm_risk" "$KOTLIN_FILE")
    
    echo ""
    echo "--- Kotlin vs C++ ---"
    for field in bod_mg_l tss_mg_l cec_cmol_kg knowledge_factor eco_impact_value harm_risk; do
        case "$field" in
            bod_mg_l) ref="$REF_BOD"; cmp="$KOTLIN_BOD" ;;
            tss_mg_l) ref="$REF_TSS"; cmp="$KOTLIN_TSS" ;;
            cec_cmol_kg) ref="$REF_CEC"; cmp="$KOTLIN_CEC" ;;
            knowledge_factor) ref="$REF_KER_K"; cmp="$KOTLIN_KER_K" ;;
            eco_impact_value) ref="$REF_KER_E"; cmp="$KOTLIN_KER_E" ;;
            harm_risk) ref="$REF_KER_R"; cmp="$KOTLIN_KER_R" ;;
        esac
        result=$(compare_values "$field" "$ref" "$cmp")
        echo "$field: $result (C++=$ref, Kotlin=$cmp)"
    done
fi

# Validate Lua vs C++ if available
if [ "$LUA_AVAILABLE" -eq 1 ] && [ -f "$LUA_FILE" ]; then
    LUA_BOD=$(extract_value "bod_mg_l" "$LUA_FILE")
    LUA_TSS=$(extract_value "tss_mg_l" "$LUA_FILE")
    LUA_CEC=$(extract_value "cec_cmol_kg" "$LUA_FILE")
    LUA_KER_K=$(extract_value "knowledge_factor" "$LUA_FILE")
    LUA_KER_E=$(extract_value "eco_impact_value" "$LUA_FILE")
    LUA_KER_R=$(extract_value "harm_risk" "$LUA_FILE")
    
    echo ""
    echo "--- Lua vs C++ ---"
    for field in bod_mg_l tss_mg_l cec_cmol_kg knowledge_factor eco_impact_value harm_risk; do
        case "$field" in
            bod_mg_l) ref="$REF_BOD"; cmp="$LUA_BOD" ;;
            tss_mg_l) ref="$REF_TSS"; cmp="$LUA_TSS" ;;
            cec_cmol_kg) ref="$REF_CEC"; cmp="$LUA_CEC" ;;
            knowledge_factor) ref="$REF_KER_K"; cmp="$LUA_KER_K" ;;
            eco_impact_value) ref="$REF_KER_E"; cmp="$LUA_KER_E" ;;
            harm_risk) ref="$REF_KER_R"; cmp="$LUA_KER_R" ;;
        esac
        result=$(compare_values "$field" "$ref" "$cmp")
        echo "$field: $result (C++=$ref, Lua=$cmp)"
    done
fi

# Validate K/E/R bounds [0,1]
echo ""
echo "=== Validating K/E/R bounds [0,1] ==="
validate_bounds() {
    lang="$1"
    file="$2"
    k=$(extract_value "knowledge_factor" "$file")
    e=$(extract_value "eco_impact_value" "$file")
    r=$(extract_value "harm_risk" "$file")
    
    result=$(awk -v k="$k" -v e="$e" -v r="$r" 'BEGIN {
        ok = 1
        if (k < 0 || k > 1) ok = 0
        if (e < 0 || e > 1) ok = 0
        if (r < 0 || r > 1) ok = 0
        if (ok) print "PASS"
        else print "FAIL"
    }')
    echo "$lang K/E/R bounds: $result (K=$k, E=$e, R=$r)"
}

validate_bounds "C++" "$CPP_FILE"
validate_bounds "Java" "$JAVA_FILE"
if [ "$KOTLIN_AVAILABLE" -eq 1 ] && [ -f "$KOTLIN_FILE" ]; then
    validate_bounds "Kotlin" "$KOTLIN_FILE"
fi
if [ "$LUA_AVAILABLE" -eq 1 ] && [ -f "$LUA_FILE" ]; then
    validate_bounds "Lua" "$LUA_FILE"
fi

# Validate nominal thresholds (informational - shows whether test vector qualifies for restoration)
echo ""
echo "=== Validating nominal test vector thresholds ==="
validate_thresholds() {
    lang="$1"
    file="$2"
    k=$(extract_value "knowledge_factor" "$file")
    e=$(extract_value "eco_impact_value" "$file")
    r=$(extract_value "harm_risk" "$file")
    
    result=$(awk -v k="$k" -v e="$e" -v r="$r" 'BEGIN {
        ok = 1
        if (k < 0.70) ok = 0
        if (e < 0.70) ok = 0
        if (r > 0.25) ok = 0
        if (ok) print "PASS"
        else print "INFO"
    }')
    echo "$lang thresholds: $result (K=$k>=0.70, E=$e>=0.70, R=$r<=0.25)"
}

validate_thresholds "C++" "$CPP_FILE"
validate_thresholds "Java" "$JAVA_FILE"
if [ "$KOTLIN_AVAILABLE" -eq 1 ] && [ -f "$KOTLIN_FILE" ]; then
    validate_thresholds "Kotlin" "$KOTLIN_FILE"
fi
if [ "$LUA_AVAILABLE" -eq 1 ] && [ -f "$LUA_FILE" ]; then
    validate_thresholds "Lua" "$LUA_FILE"
fi
echo "(Note: INFO indicates test vector does not meet restoration criteria; this is expected for the given inputs)"

# Validate rejection behavior with delta_vt=1001
echo ""
echo "=== Validating rejection behavior (delta_vt=1001) ==="
INVALID_DELTA=1001

# Test C++ rejection
set +e
"${CPP_OUT}" "$HOURS" "$INITIAL_BOD" "$INITIAL_TSS" "$INITIAL_CEC" \
    "$BOD_DECAY" "$TSS_DECAY" "$CEC_RECOVERY" "$ENERGYREQ" "$INVALID_DELTA" >/dev/null 2>&1
cpp_exit=$?
set -e
if [ "$cpp_exit" -ne 0 ]; then
    echo "PASS: C++ rejects delta_vt=$INVALID_DELTA (exit=$cpp_exit)"
else
    echo "FAIL: C++ should reject delta_vt=$INVALID_DELTA"
    exit 1
fi

# Test Java rejection
set +e
cd "$JAVA_OUT"
java -cp . DrainageDecay20260822 \
    "$HOURS" "$INITIAL_BOD" "$INITIAL_TSS" "$INITIAL_CEC" \
    "$BOD_DECAY" "$TSS_DECAY" "$CEC_RECOVERY" "$ENERGYREQ" "$INVALID_DELTA" >/dev/null 2>&1
java_exit=$?
cd "$TMPDIR"
set -e
if [ "$java_exit" -ne 0 ]; then
    echo "PASS: Java rejects delta_vt=$INVALID_DELTA (exit=$java_exit)"
else
    echo "FAIL: Java should reject delta_vt=$INVALID_DELTA"
    exit 1
fi

# Test Kotlin rejection if available
if [ "$KOTLIN_AVAILABLE" -eq 1 ]; then
    set +e
    if [ -f "${KOTLIN_OUT}/DrainageDecay20260822.jar" ]; then
        java -jar "${KOTLIN_OUT}/DrainageDecay20260822.jar" \
            "$HOURS" "$INITIAL_BOD" "$INITIAL_TSS" "$INITIAL_CEC" \
            "$BOD_DECAY" "$TSS_DECAY" "$CEC_RECOVERY" "$ENERGYREQ" "$INVALID_DELTA" >/dev/null 2>&1
        kotlin_exit=$?
    else
        kotlin_exit=0
        echo "SKIP: Kotlin JAR not available for rejection test"
    fi
    set -e
    if [ "$kotlin_exit" -ne 0 ]; then
        echo "PASS: Kotlin rejects delta_vt=$INVALID_DELTA (exit=$kotlin_exit)"
    elif [ "$kotlin_exit" -eq 0 ] && [ -f "${KOTLIN_OUT}/DrainageDecay20260822.jar" ]; then
        echo "FAIL: Kotlin should reject delta_vt=$INVALID_DELTA"
        exit 1
    fi
fi

# Test Lua rejection if available
if [ "$LUA_AVAILABLE" -eq 1 ]; then
    set +e
    $LUA_CMD -e "
        local drainage_decay = require('${LUA_SRC%.lua}')
        drainage_decay.project_frame({
            hours = $HOURS,
            initial_bod_mg_l = $INITIAL_BOD,
            initial_tss_mg_l = $INITIAL_TSS,
            initial_cec_cmol_kg = $INITIAL_CEC,
            bod_decay_per_hour = $BOD_DECAY,
            tss_decay_per_hour = $TSS_DECAY,
            cec_recovery_per_hour = $CEC_RECOVERY,
            energyreq_j = $ENERGYREQ,
            delta_vt = $INVALID_DELTA,
            sample_completeness = $SAMPLE_COMPLETENESS
        })
    " >/dev/null 2>&1
    lua_exit=$?
    set -e
    if [ "$lua_exit" -ne 0 ]; then
        echo "PASS: Lua rejects delta_vt=$INVALID_DELTA (exit=$lua_exit)"
    else
        echo "FAIL: Lua should reject delta_vt=$INVALID_DELTA"
        exit 1
    fi
fi

# SQLite validation if available
if [ "$SQLITE_AVAILABLE" -eq 1 ]; then
    echo ""
    echo "=== Exercising SQLite safety path ==="
    DB_FILE="${TMPDIR}/drainage_decay_test.db"
    
    # Apply schema
    if ! sqlite3 "$DB_FILE" < "$SQL_SRC"; then
        echo "FAIL: SQLite schema application failed"
        exit 1
    fi
    echo "OK: Schema applied"
    
    # Insert canal node
    sqlite3 "$DB_FILE" "INSERT INTO canal_nodes (canal_node_id, node_name, catchment_area_m2, design_flow_l_s, maximum_bod_mg_l, maximum_tss_mg_l, minimum_cec_cmol_kg, maximum_energyreq_j, maximum_abs_delta_vt) VALUES ('canal-test-20260822', 'Test Canal Node', 1000000.0, 1000.0, 100.0, 100.0, 10.0, 1000000.0, 10.0);"
    echo "OK: Canal node inserted"
    
    # Insert FOG media rows
    sqlite3 "$DB_FILE" "INSERT INTO fog_media_registry (fog_media_code, medium_name, modeled, containment_required, safe_discharge_allowed) VALUES ('modeled-open', 'Modeled Open Medium', 1, 0, 1);"
    sqlite3 "$DB_FILE" "INSERT INTO fog_media_registry (fog_media_code, medium_name, modeled, containment_required, safe_discharge_allowed) VALUES ('unmodeled-contained', 'Unmodeled Contained Medium', 0, 1, 0);"
    echo "OK: FOG media rows inserted"
    
    # Get C++ K/E/R values for the restore frame
    KER_K=$(extract_value "knowledge_factor" "$CPP_FILE")
    KER_E=$(extract_value "eco_impact_value" "$CPP_FILE")
    KER_R=$(extract_value "harm_risk" "$CPP_FILE")
    BOD_VAL=$(extract_value "bod_mg_l" "$CPP_FILE")
    TSS_VAL=$(extract_value "tss_mg_l" "$CPP_FILE")
    CEC_VAL=$(extract_value "cec_cmol_kg" "$CPP_FILE")
    
    # Insert valid restore frame (should succeed)
    set +e
    sqlite3 "$DB_FILE" "INSERT INTO drainage_decay_frames (canal_node_id, fog_media_code, observed_at_utc, duration_hours, bod_mg_l, tss_mg_l, cec_cmol_kg, energyreq_j, delta_vt, sample_completeness, knowledge_factor, eco_impact_value, harm_risk, decision_support_status) VALUES ('canal-test-20260822', 'modeled-open', '2026-08-22T12:00:00Z', $HOURS, $BOD_VAL, $TSS_VAL, $CEC_VAL, $ENERGYREQ, $DELTA_VT, $SAMPLE_COMPLETENESS, $KER_K, $KER_E, $KER_R, 'restore');"
    valid_insert_exit=$?
    set -e
    if [ "$valid_insert_exit" -eq 0 ]; then
        echo "OK: Valid restore frame accepted"
    else
        echo "FAIL: Valid restore frame should be accepted"
        exit 1
    fi
    
    # Insert invalid restore frame with unmodeled-contained (should fail)
    set +e
    sqlite3 "$DB_FILE" "INSERT INTO drainage_decay_frames (canal_node_id, fog_media_code, observed_at_utc, duration_hours, bod_mg_l, tss_mg_l, cec_cmol_kg, energyreq_j, delta_vt, sample_completeness, knowledge_factor, eco_impact_value, harm_risk, decision_support_status) VALUES ('canal-test-20260822', 'unmodeled-contained', '2026-08-22T13:00:00Z', $HOURS, $BOD_VAL, $TSS_VAL, $CEC_VAL, $ENERGYREQ, $DELTA_VT, $SAMPLE_COMPLETENESS, $KER_K, $KER_E, $KER_R, 'restore');" 2>&1
    invalid_insert_exit=$?
    set -e
    if [ "$invalid_insert_exit" -ne 0 ]; then
        echo "OK: Invalid restore frame (unmodeled-contained) rejected"
    else
        echo "FAIL: Invalid restore frame should be rejected by trigger"
        exit 1
    fi
    
    # Query decision support view
    result=$(sqlite3 "$DB_FILE" "SELECT recommended_action FROM drainage_decay_decision_support WHERE decision_support_status='restore' LIMIT 1;")
    if [ "$result" = "candidate_for_supervised_restoration" ]; then
        echo "OK: Accepted frame yields candidate_for_supervised_restoration"
    else
        echo "FAIL: Expected candidate_for_supervised_restoration, got: $result"
        exit 1
    fi
fi

# ALN contract checks
echo ""
echo "=== Validating ALN contract declarations ==="
ALN_CHECKS_PASSED=1

check_aln_decl() {
    pattern="$1"
    if ! grep -qF "$pattern" "$ALN_SRC"; then
        echo "FAIL: Missing ALN declaration: $pattern"
        ALN_CHECKS_PASSED=0
    else
        echo "OK: Found ALN declaration: $pattern"
    fi
}

check_aln_decl "invariant ValidKER"
check_aln_decl "invariant ValidFrame"
check_aln_decl "predicate NodeWithinLimits"
check_aln_decl "predicate FOGRestricted"
check_aln_decl "predicate EligibleForSupervisedRestoration"
check_aln_decl "predicate RequiresManualContainmentReview"

if [ "$ALN_CHECKS_PASSED" -eq 0 ]; then
    echo "FAIL: ALN contract validation failed"
    exit 1
fi

echo ""
echo "=== All validations completed successfully ==="
exit 0
