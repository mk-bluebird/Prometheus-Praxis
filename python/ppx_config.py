# filename: python/ppx_config.py
"""Configuration utilities for Prometheus-Praxis Python tools."""

from __future__ import annotations
import json
import os
from pathlib import Path
from typing import Any, Dict


def load_json_config(path: str | Path) -> Dict[str, Any]:
    """Load and parse a JSON configuration file.
    
    Args:
        path: Path to the JSON configuration file.
        
    Returns:
        Parsed configuration as a dictionary.
        
    Raises:
        FileNotFoundError: If the config file does not exist.
        json.JSONDecodeError: If the file contains invalid JSON.
    """
    p = Path(path)
    with p.open("r", encoding="utf-8") as f:
        return json.load(f)


def env_or_default(key: str, default: str) -> str:
    """Get environment variable value or return default.
    
    Args:
        key: Environment variable name.
        default: Default value if variable is not set.
        
    Returns:
        Environment variable value or default.
    """
    return os.environ.get(key, default)
