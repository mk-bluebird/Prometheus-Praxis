#!/usr/bin/env python3
"""
Docstring checker for Python modules.

Walks Python modules and reports public functions/classes missing docstrings.

Usage:
    python tools/docstring_check.py [directory]
"""

import ast
import os
import sys
from pathlib import Path
from typing import List, Tuple


def check_docstrings(filepath: Path) -> List[Tuple[int, str, str]]:
    """
    Check a Python file for missing docstrings in public functions and classes.
    
    Returns list of (line_number, type, name) tuples for items missing docstrings.
    """
    missing = []
    
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            source = f.read()
    except (IOError, OSError, UnicodeDecodeError):
        return missing
    
    try:
        tree = ast.parse(source)
    except SyntaxError:
        return missing
    
    for node in ast.walk(tree):
        # Check classes
        if isinstance(node, ast.ClassDef):
            if not node.name.startswith('_'):  # Public class
                if not ast.get_docstring(node):
                    missing.append((node.lineno, 'class', node.name))
        
        # Check functions (top-level and methods)
        elif isinstance(node, ast.FunctionDef) or isinstance(node, ast.AsyncFunctionDef):
            # Skip private/dunder methods except __init__
            if node.name.startswith('_') and node.name != '__init__':
                continue
            
            # Skip test functions
            if node.name.startswith('test_'):
                continue
            
            if not ast.get_docstring(node):
                missing.append((node.lineno, 'function', node.name))
    
    return missing


def run_docstring_check(root: str = '.') -> int:
    """Run docstring check on all Python files and print a summary report."""
    root_path = Path(root).resolve()
    
    print("=" * 60)
    print("Python Docstring Checker")
    print("=" * 60)
    print()
    
    total_files = 0
    total_missing = 0
    files_with_issues = []
    
    for dirpath, _, filenames in os.walk(root_path):
        # Skip hidden directories and common non-source dirs
        if any(part.startswith('.') for part in Path(dirpath).parts):
            continue
        if 'target' in dirpath or '__pycache__' in dirpath or 'venv' in dirpath:
            continue
        
        for fname in filenames:
            if not fname.endswith('.py'):
                continue
            
            filepath = Path(dirpath) / fname
            total_files += 1
            
            missing = check_docstrings(filepath)
            if missing:
                total_missing += len(missing)
                rel_path = filepath.relative_to(root_path)
                files_with_issues.append((rel_path, missing))
    
    print(f"Scanned {total_files} Python files")
    print()
    
    if files_with_issues:
        print("-" * 60)
        print("Missing docstrings:")
        print("-" * 60)
        for rel_path, missing in files_with_issues[:20]:  # Show first 20 files
            print(f"\n{rel_path}:")
            for line_num, item_type, name in sorted(missing)[:5]:  # Show first 5 per file
                print(f"  Line {line_num}: {item_type} '{name}'")
            if len(missing) > 5:
                print(f"  ... and {len(missing) - 5} more in this file")
        
        if len(files_with_issues) > 20:
            print(f"\n... and {len(files_with_issues) - 20} more files with issues")
        
        print()
        print(f"Total: {total_missing} missing docstrings in {len(files_with_issues)} files")
    else:
        print("OK: All public functions and classes have docstrings")
    
    print()
    print("=" * 60)
    print("SUMMARY")
    print("=" * 60)
    
    if total_missing == 0:
        print("✓ PASS: No missing docstrings found")
        return 0
    else:
        print(f"✗ FAIL: Found {total_missing} missing docstring(s)")
        print("Consider adding docstrings to improve API documentation")
        return 1


if __name__ == '__main__':
    root_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
    sys.exit(run_docstring_check(root_dir))
