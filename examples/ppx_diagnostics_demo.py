# filename: examples/ppx_diagnostics_demo.py
# destination: github.com/mk-bluebird/Prometheus-Praxis/examples/ppx_diagnostics_demo.py
#
# Example: ingest a unified diff and metrics into UnifiedSystemDiagnosticState.
# - Uses the same sample diff as the C++ demo.
# - Prints Prometheus samples and a human-readable summary.

from datetime import datetime
from python.ppx_diagnostics import (
    UnifiedSystemDiagnosticState,
    parse_unified_diff,
)

def main() -> None:
    # Sample diff payload lines.
    diff_lines = [
        "@@ -1,3 +1,4 @@\n",
        " int main() {\n",
        "-    return 0;\n",
        "+    int x = 1;\n",
        "+    return x;\n",
        " }\n",
    ]

    delta = parse_unified_diff(
        origin_path="src/example.cpp",
        target_path="src/example.cpp",
        diff_lines=diff_lines,
        is_new_file=False,
    )

    state = UnifiedSystemDiagnosticState()

    # Add metrics mirroring the C++ demo.
    state.add_metric(
        name="ppx_analyzed_diffs_total",
        value=1.0,
        labels={"agent": "python-demo", "scope": "repository"},
        epoch=datetime.utcnow(),
    )

    state.add_metric(
        name="ppx_added_lines_total",
        value=delta.total_added_lines(),
        labels={"agent": "python-demo"},
        epoch=datetime.utcnow(),
    )

    state.add_metric(
        name="ppx_deleted_lines_total",
        value=delta.total_deleted_lines(),
        labels={"agent": "python-demo"},
        epoch=datetime.utcnow(),
    )

    state.add_file_delta(delta)

    print("=== Prometheus Samples ===")
    for metric in state.collected_metrics:
        print(metric.to_prometheus_sample())

    print("\n=== Diagnostic Summary ===")
    print(state.summarize())

    print("\n=== JSON Snapshot ===")
    print(state.to_json_dict())

if __name__ == "__main__":
    main()
