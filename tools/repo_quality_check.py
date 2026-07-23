#!/usr/bin/env python3
"""
Repo quality checker for Prometheus-Praxis.

Scans for:
- Orphaned .aln files (not referenced by any tool/script)
- Duplicated spec names or version clashes
- Missing documentation entries

Usage:
    python tools/repo_quality_check.py
"""

import os
import re
import sys
from pathlib import Path
from typing import Dict, List, Set, Tuple


def find_all_aln_files(root: str) -> List[Path]:
    """Find all .aln files in the repository."""
    aln_files = []
    for dirpath, _, filenames in os.walk(root):
        # Skip hidden directories and common non-source dirs
        if any(part.startswith('.') for part in Path(dirpath).parts):
            continue
        if 'target' in dirpath or '__pycache__' in dirpath:
            continue
        for fname in filenames:
            if fname.endswith('.aln'):
                aln_files.append(Path(dirpath) / fname)
    return aln_files


def find_references_to_aln(root: str, aln_basename: str) -> List[Tuple[str, int, str]]:
    """Find references to an ALN file in Python, Lua, Rust, and Markdown files."""
    references = []
    # Patterns that might reference an ALN file
    patterns = [
        re.compile(re.escape(aln_basename)),
        re.compile(re.escape(aln_basename.replace('.aln', ''))),
    ]
    
    for dirpath, _, filenames in os.walk(root):
        if any(part.startswith('.') for part in Path(dirpath).parts):
            continue
        if 'target' in dirpath or '__pycache__' in dirpath:
            continue
        for fname in filenames:
            if not (fname.endswith(('.py', '.lua', '.rs', '.md', '.toml'))):
                continue
            filepath = Path(dirpath) / fname
            try:
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    for line_num, line in enumerate(f, 1):
                        for pattern in patterns:
                            if pattern.search(line):
                                references.append((str(filepath), line_num, line.strip()))
                                break
            except (IOError, OSError):
                pass
    return references


def check_for_duplicates(aln_files: List[Path]) -> Dict[str, List[Path]]:
    """Check for duplicate spec names or version clashes."""
    by_name: Dict[str, List[Path]] = {}
    for aln_file in aln_files:
        # Extract base name without path
        basename = aln_file.name
        # Normalize: remove version suffix for comparison
        name_key = re.sub(r'\.v\d+\.aln$', '.aln', basename)
        name_key = re.sub(r'-\d{4}v\d+\.aln$', '.aln', name_key)
        
        if name_key not in by_name:
            by_name[name_key] = []
        by_name[name_key].append(aln_file)
    
    # Return only those with potential duplicates
    return {k: v for k, v in by_name.items() if len(v) > 1}


def check_aln_docs_coverage(aln_files: List[Path], docs_path: str) -> List[Path]:
    """Check which ALN files are not documented in docs/ALN-SPECS.md."""
    undocumented = []
    docs_file = Path(docs_path) / 'ALN-SPECS.md'
    
    if not docs_file.exists():
        return list(aln_files)  # All are undocumented if doc doesn't exist
    
    with open(docs_file, 'r', encoding='utf-8') as f:
        docs_content = f.read()
    
    for aln_file in aln_files:
        basename = aln_file.name
        # Check if the file is mentioned in the docs
        if basename not in docs_content:
            # Also check without full path
            rel_path = str(aln_file.relative_to(Path(docs_path).parent))
            if rel_path not in docs_content and basename not in docs_content:
                undocumented.append(aln_file)
    
    return undocumented


def run_quality_check(root: str = '.') -> int:
    """Run all quality checks and print a summary report."""
    root_path = Path(root).resolve()
    
    print("=" * 60)
    print("Prometheus-Praxis Repo Quality Check")
    print("=" * 60)
    print()
    
    # Find all ALN files
    aln_files = find_all_aln_files(str(root_path))
    print(f"Found {len(aln_files)} .aln files")
    print()
    
    # Check for orphans
    print("-" * 60)
    print("Checking for orphaned .aln files...")
    print("-" * 60)
    orphaned = []
    for aln_file in aln_files:
        refs = find_references_to_aln(str(root_path), aln_file.name)
        # Filter out self-references (e.g., ALN file mentioning itself in comments)
        meaningful_refs = [
            r for r in refs 
            if not str(r[0]).endswith('.aln')
        ]
        if len(meaningful_refs) == 0:
            orphaned.append(aln_file)
    
    if orphaned:
        print(f"WARNING: Found {len(orphaned)} potentially orphaned .aln files:")
        for aln_file in orphaned[:10]:  # Show first 10
            print(f"  - {aln_file.relative_to(root_path)}")
        if len(orphaned) > 10:
            print(f"  ... and {len(orphaned) - 10} more")
    else:
        print("OK: No orphaned .aln files detected")
    print()
    
    # Check for duplicates
    print("-" * 60)
    print("Checking for duplicate spec names...")
    print("-" * 60)
    duplicates = check_for_duplicates(aln_files)
    
    if duplicates:
        print(f"WARNING: Found {len(duplicates)} potential duplicate spec names:")
        for name_key, files in duplicates.items():
            print(f"  Pattern '{name_key}' matched:")
            for f in files:
                print(f"    - {f.relative_to(root_path)}")
    else:
        print("OK: No duplicate spec names detected")
    print()
    
    # Check documentation coverage
    print("-" * 60)
    print("Checking ALN documentation coverage...")
    print("-" * 60)
    docs_path = root_path / 'docs'
    undocumented = check_aln_docs_coverage(aln_files, str(docs_path))
    
    if undocumented:
        print(f"INFO: {len(undocumented)} .aln files not documented in docs/ALN-SPECS.md:")
        for aln_file in undocumented[:10]:
            print(f"  - {aln_file.relative_to(root_path)}")
        if len(undocumented) > 10:
            print(f"  ... and {len(undocumented) - 10} more")
        print("Consider adding these to docs/ALN-SPECS.md")
    else:
        print("OK: All .aln files are documented")
    print()
    
    # Summary
    print("=" * 60)
    print("SUMMARY")
    print("=" * 60)
    issues = len(orphaned) + len(duplicates)
    if issues == 0:
        print("✓ PASS: No critical issues found")
        return 0
    else:
        print(f"✗ FAIL: Found {issues} issue(s)")
        print(f"  - Orphaned files: {len(orphaned)}")
        print(f"  - Duplicate names: {len(duplicates)}")
        return 1


if __name__ == '__main__':
    root_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
    sys.exit(run_quality_check(root_dir))
