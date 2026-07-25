#!/usr/bin/env python3
"""
Cyboquatic C++ Module Inventory Script

This script inventories C++ files across main cyboquatic directories and the daily
cyboquaticprogress/* folders, producing a JSON index for wiring work.

Usage:
    python tools/cyboquatic_cpp_inventory.py

Output:
    ecorestorationshard/output/cyboquatic_cpp_inventory.json

NOTE: This script is diagnostic and non-actuating. It must be run with `python` only;
      no `cargo`, no external tool installation. Uses only Python standard library.
"""

import os
import re
import json
from pathlib import Path


def extract_function_names(file_path: str) -> list:
    """
    Extract function names from a C++ file using simple heuristics.
    Looks for lines starting with type specifiers followed by function-like patterns.
    """
    functions = []
    # Patterns to match function definitions
    patterns = [
        r'^(?:static\s+)?(?:inline\s+)?(?:double|float|int|void|bool|size_t|std::string|const\s+\w+)\s+(\w+)\s*\(',
        r'^(?:struct|class)\s+(\w+)',
        r'^(\w+)\s*\([^)]*\)\s*(?:const)?\s*\{?\s*$',
        r'^int\s+main\s*\(',
    ]
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                # Skip comments
                if line.startswith('//') or line.startswith('/*'):
                    continue
                for pattern in patterns:
                    match = re.match(pattern, line)
                    if match:
                        func_name = match.group(1)
                        # Filter out keywords and common non-function matches
                        if func_name not in {'if', 'while', 'for', 'switch', 'return', 'sizeof', 'alignof'}:
                            functions.append(func_name)
                        break
    except Exception as e:
        functions.append(f"ERROR_READING: {str(e)}")
    
    return list(set(functions))  # Deduplicate


def inventory_directory(base_path: Path, relative_base: Path) -> list:
    """
    Walk a directory tree and collect .cpp and .hpp files with metadata.
    """
    results = []
    
    if not base_path.exists():
        return results
    
    for root, dirs, files in os.walk(base_path):
        root_path = Path(root)
        for file in files:
            if file.endswith('.cpp') or file.endswith('.hpp'):
                file_path = root_path / file
                rel_path = file_path.relative_to(relative_base)
                
                entry = {
                    "relative_path": str(rel_path),
                    "file_name": file,
                    "functions": extract_function_names(str(file_path))
                }
                results.append(entry)
    
    return results


def main():
    # Base paths
    repo_root = Path(__file__).parent.parent  # ecorestorationshard
    relative_base = repo_root.parent  # workspace root for relative paths
    
    # Main cyboquatic engine directories
    main_dirs = [
        repo_root / "cyboquatic_index" / "src",
        repo_root / "cyboquatic",
        repo_root / "cyboquatics",
    ]
    
    # Daily progress folders
    progress_folders = [
        repo_root / "cyboquaticprogress" / "20260718",
        repo_root / "cyboquaticprogress" / "20260719",
        repo_root / "cyboquaticprogress" / "20260723-d-cyboquaticworkload",
        repo_root / "cyboquaticprogress" / "20260724-g-blastradius",
    ]
    
    all_entries = []
    
    # Inventory main directories
    for dir_path in main_dirs:
        entries = inventory_directory(dir_path, relative_base)
        for entry in entries:
            entry["category"] = "main_engine"
            entry["source_dir"] = str(dir_path.relative_to(relative_base))
        all_entries.extend(entries)
    
    # Inventory daily progress folders
    for dir_path in progress_folders:
        entries = inventory_directory(dir_path, relative_base)
        for entry in entries:
            entry["category"] = "daily_progress"
            entry["source_dir"] = str(dir_path.relative_to(relative_base))
        all_entries.extend(entries)
    
    # Build output structure
    output = {
        "generated_at": "2026-07-25T00:00:00Z",
        "repo_root": str(relative_base),
        "total_files": len(all_entries),
        "files": all_entries
    }
    
    # Write output
    output_path = repo_root / "output" / "cyboquatic_cpp_inventory.json"
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(output, f, indent=2)
    
    print(f"Inventory complete: {len(all_entries)} files indexed")
    print(f"Output written to: {output_path}")


if __name__ == "__main__":
    main()
