# Maintenance Session Checklist

This document lists repeatable actions a collaborator can do in one sitting to improve repo health. Each step is a one-line command using Python or native tools only (no cargo, no pip install).

## Quick Health Check (5 minutes)

Run these commands to get a quick status of the repository:

```bash
# 1. Run Python unit tests
python -m unittest discover tests/python

# 2. Check for orphaned ALN files and duplicates
python tools/repo_quality_check.py

# 3. Find missing docstrings in Python modules
python tools/docstring_check.py
```

## Documentation Updates (10 minutes)

If you've added new ALN specs or modified existing ones:

```bash
# Update docs/ALN-SPECS.md with new specs
# (Manual edit: add entry with purpose, inputs, outputs, consumers)

# Regenerate shard layout diagram if prometheus-shard-layout.v1.aln changed
lua tools/prometheus_shard_layout_plantuml.lua
```

## Example Scenarios (Optional)

Verify KER and corridor examples work correctly:

```bash
# Run KER score computation example
python examples/ker/compute_ker_scores.py

# Add new examples under examples/ker/ or examples/corridors/
```

## Lua Tool Verification

If you modified Lua scripts or ALN files they consume:

```bash
# Verify bee corridor ledger integrity
lua tools/sunflower_bee_corridor_ledger_verifier.lua

# Regenerate region representation visualization
lua tools/region_representation_shard_visualizer.lua
```

## Full Session Checklist

Complete all steps for a thorough maintenance session:

- [ ] **Tests pass**: `python -m unittest discover tests/python` returns OK
- [ ] **No critical quality issues**: `python tools/repo_quality_check.py` shows no FAIL
- [ ] **Docstrings added**: Critical APIs have docstrings (run checker, add as needed)
- [ ] **Docs updated**: `docs/ALN-SPECS.md` reflects any new/changed specs
- [ ] **Diagrams regenerated**: Shard PlantUML is current (if ALN changed)
- [ ] **Examples verified**: Example scripts run without errors

## One-Liner Summary

For a quick all-in-one check (tests + quality + docstrings):

```bash
python -m unittest discover tests/python && python tools/repo_quality_check.py && python tools/docstring_check.py
```

Exit codes:
- `0`: All checks passed
- Non-zero: One or more checks failed (review output)

## When to Run

- **Before opening a PR**: Run all checks to ensure your changes don't break anything
- **Weekly maintenance**: Pick one area (tests, docs, examples) to improve
- **After adding ALN specs**: Always update `docs/ALN-SPECS.md` and run quality check

## Troubleshooting

### Tests fail
- Check if you're running from the repo root (`/workspace`)
- Ensure Python 3.x is available (`python --version`)
- Review test output for specific failures

### Quality check reports orphans
- Either the ALN file truly has no consumers (consider removing)
- Or it's referenced but not detected (add explicit reference in code/docs)

### Docstring checker reports missing
- Add module-level docstring at top of file
- Add docstrings to public functions/classes (those not starting with `_`)
- Skip private helpers and test functions (they're excluded automatically)
