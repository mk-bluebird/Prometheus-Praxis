# filename: python/ppx_io.py
from __future__ import annotations
from pathlib import Path
from typing import Iterable, Dict, Any
import csv
import json

def read_csv(path: str | Path) -> Iterable[Dict[str, Any]]:
    p = Path(path)
    with p.open("r", encoding="utf-8", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            yield row

def write_csv(path: str | Path, rows: Iterable[Dict[str, Any]]) -> None:
    p = Path(path)
    rows = list(rows)
    if not rows:
        return
    with p.open("w", encoding="utf-8", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)

def read_json(path: str | Path) -> Any:
    p = Path(path)
    with p.open("r", encoding="utf-8") as f:
        return json.load(f)

def write_json(path: str | Path, data: Any) -> None:
    p = Path(path)
    with p.open("w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
