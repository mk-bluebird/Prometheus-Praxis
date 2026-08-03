# File: python/tools/governance_trigger_runner.py
import subprocess
import shlex
import sys
from typing import List, Tuple

"""
Python runner for the Automated Integration Test Suite for Governance Triggers.

Responsibilities:
- Define a set of SQL snippets that should exercise each governance trigger
  (CarbonAwareCorridor, NeuroConsentCorridor, WaterRights, etc.).
- Call the C++ harness `governance_trigger_test_harness` per snippet.
- Collect and print a simple coverage report.

Usage:
    python governance_trigger_runner.py prometheus_praxis.db ./governance_trigger_test_harness
"""

TriggerTest = Tuple[str, str]  # (label, SQL snippet)


def build_tests() -> List[TriggerTest]:
    tests: List[TriggerTest] = []

    # Example tests for CarbonAwareCorridor trigger:
    tests.append((
        "CarbonAwareCorridor_GREEN_valid",
        "INSERT INTO hex_stability_carbon "
        "(hex_id, band, ker_k, ker_e, ker_r, carbon_intensity, max_carbon) "
        "VALUES ('hex_TEST_GREEN', 'GREEN_BAND', 0.9, 0.85, 0.2, 0.2, 1.0);"
    ))
    tests.append((
        "CarbonAwareCorridor_GREEN_invalid_band",
        "INSERT INTO hex_stability_carbon "
        "(hex_id, band, ker_k, ker_e, ker_r, carbon_intensity, max_carbon) "
        "VALUES ('hex_TEST_GREEN_BAD', 'NEUTRAL', 0.9, 0.85, 0.2, 0.2, 1.0);"
    ))

    # Example tests for NeuroConsentCorridor triggers:
    tests.append((
        "NeuroConsentCorridor_level3_active",
        "INSERT INTO neuro_consent_corridor "
        "(consent_id, module_id, subject_id, consent_level, consent_state, consent_scope, ts_granted) "
        "VALUES ('consent_TEST_1', 'module_NEURO_001', 'subject_X', 3, 'ACTIVE', 'read-only', '2026-08-03T12:00:00');"
    ))
    tests.append((
        "NeuroConsentCorridor_level3_pending_invalid",
        "INSERT INTO neuro_consent_corridor "
        "(consent_id, module_id, subject_id, consent_level, consent_state, consent_scope, ts_granted) "
        "VALUES ('consent_TEST_2', 'module_NEURO_001', 'subject_X', 3, 'PENDING', 'read-only', NULL);"
    ))

    # Example tests for WaterRights triggers:
    tests.append((
        "WaterRights_daily_limit_valid",
        "INSERT INTO water_rights "
        "(right_id, holder_id, source_id, daily_limit_m3, seasonal_limit_m3, priority_class, is_exempt, allocated_today_m3, allocated_season_m3) "
        "VALUES ('right_TEST_1', 'holder_A', 'source_CANAL', 10.0, 100.0, 1, 0, 5.0, 20.0);"
    ))
    tests.append((
        "WaterRights_daily_limit_exceeded",
        "INSERT INTO water_rights "
        "(right_id, holder_id, source_id, daily_limit_m3, seasonal_limit_m3, priority_class, is_exempt, allocated_today_m3, allocated_season_m3) "
        "VALUES ('right_TEST_2', 'holder_A', 'source_CANAL', 10.0, 100.0, 1, 0, 15.0, 20.0);"
    ))

    return tests


def run_harness(db_path: str, harness_path: str, label: str, sql: str) -> bool:
    cmd = f"{shlex.quote(harness_path)} {shlex.quote(db_path)} {shlex.quote(label)} {shlex.quote(sql)}"
    proc = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    output = proc.communicate()[0]
    rc = proc.returncode
    sys.stdout.write(output)
    return rc == 0


def main():
    if len(sys.argv) < 3:
        print("Usage: python governance_trigger_runner.py <db_path> <harness_path>")
        sys.exit(1)

    db_path = sys.argv[1]
    harness_path = sys.argv[2]

    tests = build_tests()
    total = len(tests)
    passed = 0

    print(f"Running governance trigger integration tests (total={total})...\n")

    for label, sql in tests:
        ok = run_harness(db_path, harness_path, label, sql)
        if ok:
            passed += 1

    print(f"\nCoverage summary: passed={passed}/{total} ({100.0 * passed / total:.1f}%)")

    sys.exit(0 if passed == total else 1)


if __name__ == "__main__":
    main()
