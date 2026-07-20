# filename: python/ppx_agent.py
from __future__ import annotations
from typing import Any, Dict, Iterable
from pathlib import Path
from .ppx_io import read_csv

def load_eco_shards(root: str | Path) -> Iterable[Dict[str, Any]]:
    root_path = Path(root)
    for csv_path in root_path.rglob("*.csv"):
        if "qpudatashards" in csv_path.as_posix():
            yield from read_csv(csv_path)

def simple_agent_loop(root: str | Path) -> None:
    for row in load_eco_shards(root):
        # Placeholder logic: collaborators can replace this with real analysis.
        node_id = row.get("nodeid", "UNKNOWN")
        print(f"Found shard row for node: {node_id}")
