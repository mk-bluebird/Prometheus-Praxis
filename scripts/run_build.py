# File: scripts/run_build.py

from pathlib import Path
import subprocess

repository_root = Path(__file__).resolve().parents[1]
subprocess.run(
    ["bash", "scripts/build.sh", "all"],
    cwd=repository_root,
    check=True,
)
