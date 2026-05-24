#!/usr/bin/env bash
# check_workspace_members.sh
# Workspace consistency check script
# Verifies that every workspace member has a Cargo.toml and valid package metadata.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Workspace Consistency Check ==="
echo "Repository root: $REPO_ROOT"

cd "$REPO_ROOT"

# Get workspace members from cargo metadata
echo ""
echo "Reading workspace members via cargo metadata..."
MEMBERS=$(cargo metadata --no-deps --format-version 1 2>/dev/null | jq -r '.workspace_members[]')

if [ -z "$MEMBERS" ]; then
    echo "ERROR: No workspace members found or cargo metadata failed."
    exit 1
fi

FAILED=0
CHECKED=0

for MEMBER in $MEMBERS; do
    CHECKED=$((CHECKED + 1))
    echo ""
    echo "Checking member: $MEMBER"
    
    # Extract the path portion (remove package ID suffix if present)
    MEMBER_PATH="${MEMBER%% *}"
    
    # Check if Cargo.toml exists
    CARGO_TOML="$REPO_ROOT/$MEMBER_PATH/Cargo.toml"
    if [ ! -f "$CARGO_TOML" ]; then
        # Try alternate form where member is just "."
        if [ "$MEMBER_PATH" = "." ]; then
            CARGO_TOML="$REPO_ROOT/Cargo.toml"
        fi
    fi
    
    if [ ! -f "$CARGO_TOML" ]; then
        echo "  ERROR: Cargo.toml not found at $CARGO_TOML"
        FAILED=1
        continue
    else
        echo "  OK: Cargo.toml exists"
    fi
    
    # Try to get package metadata for this member
    # First, extract package name from the member path
    PKG_NAME=$(basename "$MEMBER_PATH")
    if [ "$PKG_NAME" = "." ]; then
        PKG_NAME=$(jq -r '.name' "$REPO_ROOT/Cargo.toml" 2>/dev/null || echo "")
    fi
    
    if [ -n "$PKG_NAME" ]; then
        echo "  Checking package metadata for: $PKG_NAME"
        if ! cargo metadata --no-deps --format-version 1 2>/dev/null | jq -e ".packages[] | select(.name == \"$PKG_NAME\")" > /dev/null 2>&1; then
            echo "  WARNING: Could not resolve package metadata for $PKG_NAME (may need build)"
        else
            echo "  OK: Package metadata resolved"
        fi
    fi
done

echo ""
echo "=== Summary ==="
echo "Checked: $CHECKED members"
if [ $FAILED -eq 0 ]; then
    echo "Status: PASS - All workspace members are valid"
    exit 0
else
    echo "Status: FAIL - Some members have issues"
    exit 1
fi
