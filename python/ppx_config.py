# filename: python/ppx_config.py
from __future__ import annotations
import json
import os
from pathlib import Path
from typing import Any, Dict

def load_json_config(path: str | Path) -> Dict[str, Any]:
    p = Path(path)
    with p.open("r", encoding="utf-8") as f:
        return json.load(f)

def env_or_default(key: str, default: str) -> str:
    return os.environ.get(key, default)
