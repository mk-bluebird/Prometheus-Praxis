# filename: tools/function_meta_ci_check.py
# purpose: CI helper to enforce that all exported functions are registered
#          in ppx.function.meta.v1.aln and function_catalog.sqlite

import os
import re
import sqlite3
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
ALN_META_PATH = REPO_ROOT / "aln" / "ppx.function.meta.v1.aln"
SQLITE_CATALOG_PATH = REPO_ROOT / "db" / "function_catalog.sqlite"

def load_function_catalog():
    """Load function_catalog entries from SQLite using stdlib only."""
    conn = sqlite3.connect(str(SQLITE_CATALOG_PATH))
    try:
        cursor = conn.cursor()
        cursor.execute(
            "SELECT function_id, requires_gate FROM function_catalog"
        )
        rows = cursor.fetchall()
        return {row[0]: {"requires_gate": bool(row[1])} for row in rows}
    finally:
        conn.close()

def scan_exported_functions():
    """
    Scan Rust source files for exported functions that should appear
    in ppx.function.meta.v1.aln and function_catalog.
    This is a simple heuristic based on `pub fn` patterns.
    """
    exported = set()
    src_dir = REPO_ROOT / "src"
    for root, _dirs, files in os.walk(src_dir):
        for name in files:
            if not name.endswith(".rs"):
                continue
            path = Path(root) / name
            text = path.read_text(encoding="utf-8")
            for match in re.finditer(r"pub\s+fn\s+([a-zA-Z0-9_]+)", text):
                exported.add(match.group(1))
    return exported

def load_aln_function_meta():
    """
    Load function_ids from ppx.function.meta.v1.aln.
    This is a lightweight parser that looks for `function_id` fields.
    """
    if not ALN_META_PATH.exists():
        return set()
    text = ALN_META_PATH.read_text(encoding="utf-8")
    ids = set()
    for line in text.splitlines():
        line = line.strip()
        if line.startswith("function_id"):
            parts = line.split()
            if len(parts) >= 2:
                ids.add(parts[1])
    return ids

def main():
    catalog = load_function_catalog()
    meta_ids = load_aln_function_meta()
    exported = scan_exported_functions()

    missing_in_meta = []
    missing_in_catalog = []

    for fn in exported:
        if fn not in meta_ids:
            missing_in_meta.append(fn)
        if fn not in catalog:
            missing_in_catalog.append(fn)

    if missing_in_meta or missing_in_catalog:
        print("function_meta_ci_check: FAILED")
        if missing_in_meta:
            print("  Exported functions missing from ppx.function.meta.v1.aln:")
            for fn in sorted(missing_in_meta):
                print(f"    - {fn}")
        if missing_in_catalog:
            print("  Exported functions missing from function_catalog.sqlite:")
            for fn in sorted(missing_in_catalog):
                print(f"    - {fn}")
        raise SystemExit(1)

    print("function_meta_ci_check: OK")

if __name__ == "__main__":
    main()
