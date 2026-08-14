#!/usr/bin/env python3
"""Black-box regression tests for Foundation CLI commands."""

import json
import subprocess
import sys


def run_command(exe_path, args):
    """Run executable with args and capture stdout, stderr, returncode."""
    result = subprocess.run(
        [exe_path] + args,
        capture_output=True,
        text=True
    )
    return result.stdout, result.stderr, result.returncode


def test_foundation_self_check(exe_path):
    """Test --foundation-self-check produces valid JSON ending in one newline."""
    stdout, stderr, rc = run_command(exe_path, ["--foundation-self-check"])
    
    # stderr must be empty for valid command
    if stderr:
        print(f"FAIL foundation-self-check: stderr not empty: {stderr!r}")
        return False
    
    # stdout must end with exactly one newline
    if not stdout.endswith('\n'):
        print(f"FAIL foundation-self-check: stdout does not end with newline")
        return False
    if stdout.endswith('\n\n'):
        print(f"FAIL foundation-self-check: stdout ends with multiple newlines")
        return False
    
    # Must be valid UTF-8 JSON
    try:
        data = json.loads(stdout.strip())
    except json.JSONDecodeError as e:
        print(f"FAIL foundation-self-check: invalid JSON: {e}")
        return False
    
    # Exit code must be 0 or 2
    if rc not in (0, 2):
        print(f"FAIL foundation-self-check: unexpected exit code {rc}")
        return False
    
    print("PASS foundation-self-check")
    return True


def test_foundation_extension_self_test(exe_path):
    """Test --foundation-extension-self-test produces exact key=value output."""
    stdout, stderr, rc = run_command(exe_path, ["--foundation-extension-self-test"])
    
    # stderr must be empty for valid command
    if stderr:
        print(f"FAIL foundation-extension-self-test: stderr not empty: {stderr!r}")
        return False
    
    # stdout must be exactly "foundation_extensions_self_test=0\n" or "=1\n"
    expected_0 = "foundation_extensions_self_test=0\n"
    expected_1 = "foundation_extensions_self_test=1\n"
    if stdout not in (expected_0, expected_1):
        print(f"FAIL foundation-extension-self-test: unexpected output {stdout!r}")
        return False
    
    # Exit code must be 0 or 2
    if rc not in (0, 2):
        print(f"FAIL foundation-extension-self-test: unexpected exit code {rc}")
        return False
    
    print("PASS foundation-extension-self-test")
    return True


def test_unsupported_command(exe_path):
    """Test unsupported command writes to stderr and returns 64."""
    stdout, stderr, rc = run_command(exe_path, ["--unsupported"])
    
    # stderr must be exactly "unsupported command\n"
    if stderr != "unsupported command\n":
        print(f"FAIL unsupported: stderr mismatch: {stderr!r} != 'unsupported command\\n'")
        return False
    
    # Exit code must be 64
    if rc != 64:
        print(f"FAIL unsupported: exit code {rc} != 64")
        return False
    
    print("PASS unsupported-command")
    return True


def test_no_command(exe_path):
    """Test no command argument writes nonempty stderr and returns 64."""
    stdout, stderr, rc = run_command(exe_path, [])
    
    # stderr must be nonempty
    if not stderr:
        print(f"FAIL no-command: stderr is empty")
        return False
    
    # Exit code must be 64
    if rc != 64:
        print(f"FAIL no-command: exit code {rc} != 64")
        return False
    
    print("PASS no-command")
    return True


def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <foundation_executable_path>", file=sys.stderr)
        sys.exit(1)
    
    exe_path = sys.argv[1]
    
    all_passed = True
    
    if not test_foundation_self_check(exe_path):
        all_passed = False
    
    if not test_foundation_extension_self_test(exe_path):
        all_passed = False
    
    if not test_unsupported_command(exe_path):
        all_passed = False
    
    if not test_no_command(exe_path):
        all_passed = False
    
    if all_passed:
        print("\nAll tests PASSED")
        sys.exit(0)
    else:
        print("\nSome tests FAILED")
        sys.exit(1)


if __name__ == "__main__":
    main()
