# filename: ai_safe_wrappers/phoenix_uhi_hex_risk_wrapper.py

import json
import subprocess
from typing import List
from pydantic import BaseModel, validator

class HexUhiRisk(BaseModel):
    hex_id: str
    r_t: float
    r_c: float
    r_a: float
    r_thermal: float
    r_biodiv: float
    r_energy: float
    r_ai: float

    @validator("r_t", "r_c", "r_a", "r_thermal", "r_biodiv", "r_energy", "r_ai")
    def in_unit_interval(cls, v: float) -> float:
        if not (0.0 <= v <= 1.0):
            raise ValueError(f"risk coordinate out of [0,1]: {v}")
        return v

    @validator("r_biodiv")
    def biodiv_non_offsettable(cls, v: float) -> float:
        if v > 0.50:
            raise ValueError(f"biodiv risk exceeds non-offsettable corridor: {v}")
        return v

    @validator("r_energy")
    def carbon_non_offsettable(cls, v: float) -> float:
        if v > 0.60:
            raise ValueError(f"carbon/energy risk exceeds non-offsettable corridor: {v}")
        return v

    @validator("r_ai")
    def neuro_non_offsettable(cls, v: float) -> float:
        if v > 0.65:
            raise ValueError(f"AI/neurorights risk exceeds non-offsettable corridor: {v}")
        return v

def run_phoenix_uhi_hex_risk(json_input_path: str) -> List[HexUhiRisk]:
    """Run the phoenix_uhi_hex_risk binary and return validated risk records."""
    try:
        proc = subprocess.Popen(
            ["./phoenix_uhi_hex_risk", "--json", json_input_path],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        stdout, stderr = proc.communicate()

        if proc.returncode != 0:
            raise RuntimeError(f"phoenix_uhi_hex_risk failed: {stderr}")

        safe_records: List[HexUhiRisk] = []
        for line in stdout.splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                data = json.loads(line)
                record = HexUhiRisk(**data)
                safe_records.append(record)
            except Exception as e:
                # Unsafe or malformed shard; log and skip.
                print(f"[AI-SAFE] Dropping shard due to validation error: {e}")

        return safe_records
    except Exception as e:
        raise RuntimeError(f"Failed to run phoenix_uhi_hex_risk: {e}")
