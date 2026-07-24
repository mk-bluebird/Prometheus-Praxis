#!/usr/bin/env python3
"""
cyboquatic_cpp_engine_index.py

Indexes C++ workload/drainage/AI-node kernels in main Cyboquatic engine directories.
Uses only standard Python library (os, pathlib, json, sqlite3, re).

Outputs:
- output/cyboquatic_cpp_engine_index.json
- Optional SQLite table 'engine_cpp_modules' in db/cyboquatic_engine_index.db
"""

import os
import re
import json
import sqlite3
from pathlib import Path
from datetime import datetime, timezone


# Engine directories to scan
ENGINE_DIRS = [
    Path("/workspace/crates/prometheus_praxis_ai/src/engine/cpp"),
    Path("/workspace/ecorestorationshard/ppx_aln/src"),
]

# Output paths
OUTPUT_DIR = Path("/workspace/output")
JSON_OUTPUT = OUTPUT_DIR / "cyboquatic_cpp_engine_index.json"
DB_OUTPUT = Path("/workspace/db/cyboquatic_engine_index.db")


def extract_function_names(source_code: str) -> list:
    """Extract function names from C++ source using simple regex."""
    # Match function definitions: return_type function_name(params)
    pattern = r'^[a-zA-Z_][a-zA-Z0-9_<>*:\s]*\s+(\w+)\s*\([^)]*\)\s*(?:const)?\s*(?:override)?\s*(?:final)?\s*\{'
    functions = []
    for match in re.finditer(pattern, source_code, re.MULTILINE):
        func_name = match.group(1)
        # Filter out keywords and common non-function tokens
        if func_name not in ('if', 'while', 'for', 'switch', 'catch', 'namespace'):
            functions.append(func_name)
    return functions


def extract_includes(source_code: str) -> list:
    """Extract #include directives from C++ source."""
    pattern = r'#include\s*[<"]([^>"]+)[>"]'
    return re.findall(pattern, source_code)


def classify_module(file_path: Path) -> str:
    """Classify module by domain based on filename/content."""
    name_lower = file_path.name.lower()
    if 'workload' in name_lower:
        return 'workload'
    elif 'drainage' in name_lower or 'hydraulic' in name_lower:
        return 'drainage'
    elif 'ai_node' in name_lower or 'ainode' in name_lower or 'datacenter' in name_lower:
        return 'ai_node'
    elif 'bridge' in name_lower or 'ffi' in name_lower:
        return 'bridge'
    else:
        return 'other'


def scan_cpp_file(file_path: Path, base_dir: Path) -> dict:
    """Scan a single C++ file and extract metadata."""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
    except Exception as e:
        return {
            'error': str(e),
            'file_name': file_path.name,
            'relative_path': str(file_path.relative_to(base_dir)) if base_dir in file_path.parents else str(file_path),
        }

    # Extract header comment (first few lines)
    lines = content.split('\n')
    header_lines = []
    in_header = False
    for line in lines[:30]:
        if line.strip().startswith('//'):
            in_header = True
            header_lines.append(line.strip())
        elif in_header and line.strip() == '':
            continue
        elif in_header:
            break

    functions = extract_function_names(content)
    includes = extract_includes(content)

    return {
        'file_name': file_path.name,
        'relative_path': str(file_path.relative_to(base_dir)) if base_dir in file_path.parents else str(file_path),
        'absolute_path': str(file_path),
        'file_type': 'hpp' if file_path.suffix == '.hpp' else 'cpp',
        'domain': classify_module(file_path),
        'header_comment': header_lines[:10],
        'functions': functions,
        'includes': includes,
        'line_count': len(lines),
        'size_bytes': len(content.encode('utf-8')),
    }


def scan_engine_directories() -> list:
    """Scan all engine directories for C++ files."""
    modules = []

    for base_dir in ENGINE_DIRS:
        if not base_dir.exists():
            print(f"Warning: Directory does not exist: {base_dir}")
            continue

        for ext in ['*.cpp', '*.hpp']:
            for file_path in base_dir.glob(ext):
                module_info = scan_cpp_file(file_path, base_dir)
                module_info['source_dir'] = str(base_dir)
                modules.append(module_info)

    return modules


def write_json_report(modules: list) -> None:
    """Write JSON report to output directory."""
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    report = {
        'generated_at': datetime.now(timezone.utc).isoformat(),
        'engine_dirs_scanned': [str(d) for d in ENGINE_DIRS],
        'total_modules': len(modules),
        'modules_by_domain': {},
        'modules': modules,
    }

    # Aggregate by domain
    domain_counts = {}
    for mod in modules:
        domain = mod.get('domain', 'unknown')
        domain_counts[domain] = domain_counts.get(domain, 0) + 1
    report['modules_by_domain'] = domain_counts

    with open(JSON_OUTPUT, 'w', encoding='utf-8') as f:
        json.dump(report, f, indent=2)

    print(f"JSON report written to: {JSON_OUTPUT}")


def write_sqlite_table(modules: list) -> None:
    """Write modules to SQLite database."""
    conn = sqlite3.connect(str(DB_OUTPUT))
    cursor = conn.cursor()

    # Create table
    cursor.execute('''
        CREATE TABLE IF NOT EXISTS engine_cpp_modules (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_name TEXT NOT NULL,
            relative_path TEXT NOT NULL,
            absolute_path TEXT,
            file_type TEXT CHECK(file_type IN ('cpp', 'hpp')),
            domain TEXT,
            source_dir TEXT,
            functions TEXT,  -- JSON array
            includes TEXT,   -- JSON array
            line_count INTEGER,
            size_bytes INTEGER,
            indexed_at TEXT DEFAULT CURRENT_TIMESTAMP
        )
    ''')

    # Clear existing data for re-indexing
    cursor.execute('DELETE FROM engine_cpp_modules')

    # Insert modules
    for mod in modules:
        if 'error' in mod:
            continue
        cursor.execute('''
            INSERT INTO engine_cpp_modules
            (file_name, relative_path, absolute_path, file_type, domain, source_dir, functions, includes, line_count, size_bytes)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        ''', (
            mod['file_name'],
            mod['relative_path'],
            mod.get('absolute_path', ''),
            mod['file_type'],
            mod['domain'],
            mod.get('source_dir', ''),
            json.dumps(mod.get('functions', [])),
            json.dumps(mod.get('includes', [])),
            mod.get('line_count', 0),
            mod.get('size_bytes', 0),
        ))

    conn.commit()

    # Create index for domain queries
    cursor.execute('CREATE INDEX IF NOT EXISTS idx_domain ON engine_cpp_modules(domain)')
    cursor.execute('CREATE INDEX IF NOT EXISTS idx_file_type ON engine_cpp_modules(file_type)')

    conn.close()
    print(f"SQLite table written to: {DB_OUTPUT}")


def main():
    print("Cyboquatic C++ Engine Indexer")
    print("=" * 40)

    # Scan directories
    modules = scan_engine_directories()
    print(f"Found {len(modules)} C++ modules")

    # Write outputs
    write_json_report(modules)
    write_sqlite_table(modules)

    # Print summary
    print("\nSummary by domain:")
    domain_counts = {}
    for mod in modules:
        domain = mod.get('domain', 'unknown')
        domain_counts[domain] = domain_counts.get(domain, 0) + 1

    for domain, count in sorted(domain_counts.items()):
        print(f"  {domain}: {count}")

    print("\nDone.")


if __name__ == '__main__':
    main()
