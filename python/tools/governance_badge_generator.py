# File: python/tools/governance_badge_generator.py
import os
import re
import sys
from typing import List, Tuple

"""
Repository Badges and Documentation Generator

Scans all README.md files under the repo, extracts KER triads or ker_s values
from Markdown text, computes an average ker_s, and writes a governance health
badge line into the main README (or prints it to stdout).

Usage:
    python python/tools/governance_badge_generator.py <repo_root> [--write-main]

If --write-main is given, the script appends or updates a line of the form:

    Governance health badge: avg ker_s = 0.62

in the root README.md. Otherwise, it just prints the computed badge.
"""

KER_TRIAD_REGEX = re.compile(
    r"k\s*[:=]\s*([0-9]*\.?[0-9]+)\s*,?\s*"
    r"e\s*[:=]\s*([0-9]*\.?[0-9]+)\s*,?\s*"
    r"r\s*[:=]\s*([0-9]*\.?[0-9]+)",
    re.IGNORECASE
)

KER_S_REGEX = re.compile(
    r"ker[_\s]*s\s*[:=]\s*([0-9]*\.?[0-9]+)",
    re.IGNORECASE
)


def find_readmes(root: str) -> List[str]:
    readmes = []
    for dirpath, _, filenames in os.walk(root):
        for fname in filenames:
            if fname.lower() == "readme.md":
                readmes.append(os.path.join(dirpath, fname))
    return readmes


def extract_ker_values_from_file(path: str) -> List[float]:
    vals: List[float] = []
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()

    # Extract explicit ker_s mentions
    for m in KER_S_REGEX.finditer(text):
        try:
            vals.append(float(m.group(1)))
        except ValueError:
            continue

    # Extract triads "k=..., e=..., r=..." and compute s = k*e - r
    for m in KER_TRIAD_REGEX.finditer(text):
        try:
            k = float(m.group(1))
            e = float(m.group(2))
            r = float(m.group(3))
            s = k * e - r
            vals.append(s)
        except ValueError:
            continue

    return vals


def compute_avg_ker_s(root: str) -> Tuple[float, int]:
    readmes = find_readmes(root)
    all_vals: List[float] = []
    for path in readmes:
        vals = extract_ker_values_from_file(path)
        all_vals.extend(vals)

    if not all_vals:
        return 0.0, 0
    avg = sum(all_vals) / len(all_vals)
    return avg, len(all_vals)


def update_main_readme(root: str, avg_s: float) -> None:
    main_readme = os.path.join(root, "README.md")
    badge_line = f"Governance health badge: avg ker_s = {avg_s:.2f}\n"

    if not os.path.exists(main_readme):
        # Create a minimal README with the badge.
        with open(main_readme, "w", encoding="utf-8") as f:
            f.write("# Prometheus-Praxis Governance\n\n")
            f.write(badge_line)
        return

    # Update or append badge line.
    with open(main_readme, "r", encoding="utf-8") as f:
        lines = f.readlines()

    updated = False
    for i, line in enumerate(lines):
        if line.startswith("Governance health badge: avg ker_s"):
            lines[i] = badge_line
            updated = True
            break

    if not updated:
        # Append near the top, after title if present.
        if lines and lines[0].startswith("#"):
            lines.insert(2, badge_line)  # after title and blank line
        else:
            lines.insert(0, badge_line)

    with open(main_readme, "w", encoding="utf-8") as f:
        f.writelines(lines)


def main():
    if len(sys.argv) < 2:
        print("Usage: python governance_badge_generator.py <repo_root> [--write-main]")
        sys.exit(1)

    root = sys.argv[1]
    write_main = "--write-main" in sys.argv[2:]

    avg_s, count = compute_avg_ker_s(root)
    print(f"Computed governance health badge from {count} KER entries: avg ker_s = {avg_s:.2f}")

    if write_main:
        update_main_readme(root, avg_s)
        print("Main README.md updated with governance health badge.")


if __name__ == "__main__":
    main()
