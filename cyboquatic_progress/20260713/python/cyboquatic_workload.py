#!/usr/bin/env python3
"""
filename: eco_restoration_shard/cyboquatic_progress/20260713/python/cyboquatic_workload.py
domain: (d) Cyboquatic workload in Python (non-actuating diagnostics)
purpose: Python implementation of workload risk kernel, K,E,R scoring, and SQLite integration.
         Designed for AI-chat platforms and coding-agents with clear API boundaries.
"""

import sqlite3
from dataclasses import dataclass, field
from typing import Optional, Tuple, List, Dict, Any
from datetime import datetime, timezone


# =============================================================================
# Constants (mirrored from C++/Java/Kotlin implementations)
# =============================================================================

W_ENERGY = 0.8
W_HYDRAULIC = 1.0
W_UNCERTAINTY = 0.6

ENERGY_TAILWIND_SAFE_RATIO = 1.2
ENERGY_MIN_RATIO = 0.0
ENERGY_MAX_RATIO = 2.5

# Extended risk weights for next-step research
W_VELOCITY = 0.7
W_SENSOR_HEALTH = 0.5


# =============================================================================
# Data Classes
# =============================================================================

@dataclass
class WorkloadRiskVector:
    """
    Risk vector with coordinates for energy, hydraulic, uncertainty,
    plus extended coordinates for canal velocity and sensor health.
    All values clamped to [0, 1].
    """
    renergy: float = 0.0
    rhydraulic: float = 0.0
    runcertainty: float = 0.0
    rvelocity: float = 0.0
    rsensor_health: float = 0.0

    def __post_init__(self):
        self.renergy = self._clamp01(self.renergy)
        self.rhydraulic = self._clamp01(self.rhydraulic)
        self.runcertainty = self._clamp01(self.runcertainty)
        self.rvelocity = self._clamp01(self.rvelocity)
        self.rsensor_health = self._clamp01(self.rsensor_health)

    @staticmethod
    def _clamp01(x: float) -> float:
        return max(0.0, min(1.0, x))

    def residual(self, include_extended: bool = False) -> float:
        """
        Compute Lyapunov residual Vt = Σ w_j * r_j^2
        
        Args:
            include_extended: If True, includes velocity and sensor_health risks
        
        Returns:
            Lyapunov residual value
        """
        vt = (W_ENERGY * self.renergy ** 2 +
              W_HYDRAULIC * self.rhydraulic ** 2 +
              W_UNCERTAINTY * self.runcertainty ** 2)
        
        if include_extended:
            vt += (W_VELOCITY * self.rvelocity ** 2 +
                   W_SENSOR_HEALTH * self.rsensor_health ** 2)
        
        return vt

    def max_risk(self) -> float:
        """Return the maximum risk coordinate."""
        return max(
            self.renergy, self.rhydraulic, self.runcertainty,
            self.rvelocity, self.rsensor_health
        )

    def to_dict(self) -> Dict[str, float]:
        """Convert to dictionary for JSON serialization."""
        return {
            "renergy": self.renergy,
            "rhydraulic": self.rhydraulic,
            "runcertainty": self.runcertainty,
            "rvelocity": self.rvelocity,
            "rsensor_health": self.rsensor_health
        }


@dataclass
class WorkloadKer:
    """
    K,E,R triad scores derived from risk vector and ΔVt.
    
    Attributes:
        vt: Current Lyapunov residual
        delta_vt: Change in residual (vt_after - vt_before)
        k: Knowledge factor [0, 1]
        e: Eco-impact factor [0, 1]
        r: Risk-of-harm factor [0, 1]
    """
    vt: float
    delta_vt: float
    k: float
    e: float
    r: float

    def to_dict(self) -> Dict[str, float]:
        """Convert to dictionary for JSON serialization."""
        return {
            "vt": self.vt,
            "delta_vt": self.delta_vt,
            "k": self.k,
            "e": self.e,
            "r": self.r
        }

    def is_safe(self, k_min: float = 0.9, e_min: float = 0.9, r_max: float = 0.15) -> bool:
        """
        Check if K,E,R scores meet safety thresholds for production coupling.
        
        Args:
            k_min: Minimum acceptable K factor
            e_min: Minimum acceptable E factor
            r_max: Maximum acceptable R factor
        
        Returns:
            True if all thresholds are met
        """
        return self.k >= k_min and self.e >= e_min and self.r <= r_max


@dataclass
class WorkloadSample:
    """
    Complete workload sample binding energetics, risk, residuals, and K,E,R scores.
    
    This is the primary data structure for AI-chat platforms and coding-agents.
    """
    sample_id: str
    node_id: str
    timestamp_utc: str
    energy_req_j: float
    energy_surplus_j: float
    hydraulic_risk: float
    uncertainty_risk: float
    canal_velocity_mps: float = 0.0
    sensor_health: float = 1.0
    risk: WorkloadRiskVector = field(default_factory=WorkloadRiskVector)
    vt_before: float = 0.0
    vt_after: float = 0.0
    delta_vt: float = 0.0
    ker: Optional[WorkloadKer] = None

    def to_dict(self) -> Dict[str, Any]:
        """Convert to dictionary for JSON serialization."""
        return {
            "sample_id": self.sample_id,
            "node_id": self.node_id,
            "timestamp_utc": self.timestamp_utc,
            "energy_req_j": self.energy_req_j,
            "energy_surplus_j": self.energy_surplus_j,
            "hydraulic_risk": self.hydraulic_risk,
            "uncertainty_risk": self.uncertainty_risk,
            "canal_velocity_mps": self.canal_velocity_mps,
            "sensor_health": self.sensor_health,
            "risk": self.risk.to_dict(),
            "vt_before": self.vt_before,
            "vt_after": self.vt_after,
            "delta_vt": self.delta_vt,
            "ker": self.ker.to_dict() if self.ker else None
        }


# =============================================================================
# Core Functions
# =============================================================================

def normalize_risk(
    energy_req_j: float,
    energy_surplus_j: float,
    hydraulic_risk: float,
    uncertainty_risk: float,
    canal_velocity_mps: float = 0.0,
    sensor_health: float = 1.0,
    velocity_threshold_mps: float = 2.0
) -> WorkloadRiskVector:
    """
    Normalize input metrics into risk coordinates in [0, 1].
    
    The energy risk uses a "tailwind-safe corridor" logic:
    - ratio >= 1.2 (surplus >= 20% of required) → renergy = 0 (safe)
    - ratio <= 0 → renergy = 1 (severe shortfall)
    - Linear interpolation between bands
    
    Args:
        energy_req_j: Required energy in Joules
        energy_surplus_j: Available surplus energy in Joules
        hydraulic_risk: Raw hydraulic risk [0, 1]
        uncertainty_risk: Raw telemetry/model uncertainty [0, 1]
        canal_velocity_mps: Canal water velocity in m/s
        sensor_health: Sensor health score [0, 1], where 1 is perfect
        velocity_threshold_mps: Velocity threshold above which risk increases
    
    Returns:
        WorkloadRiskVector with normalized coordinates
    """
    # Energy risk based on tailwind ratio
    if energy_req_j <= 0.0:
        ratio = ENERGY_MAX_RATIO
    else:
        ratio = energy_surplus_j / energy_req_j

    if ratio >= ENERGY_TAILWIND_SAFE_RATIO:
        renergy_raw = 0.0
    elif ratio <= ENERGY_MIN_RATIO:
        renergy_raw = 1.0
    else:
        bounded_ratio = min(ratio, ENERGY_MAX_RATIO)
        span = ENERGY_TAILWIND_SAFE_RATIO - ENERGY_MIN_RATIO
        rel = (bounded_ratio - ENERGY_MIN_RATIO) / span
        renergy_raw = 1.0 - rel

    renergy = WorkloadRiskVector._clamp01(renergy_raw)

    # Velocity risk: increases when velocity exceeds threshold
    rvelocity = 0.0
    if canal_velocity_mps > velocity_threshold_mps:
        rvelocity = min(1.0, (canal_velocity_mps - velocity_threshold_mps) / velocity_threshold_mps)

    # Sensor health risk: inverse of health score
    rsensor_health = WorkloadRiskVector._clamp01(1.0 - sensor_health)

    return WorkloadRiskVector(
        renergy=renergy,
        rhydraulic=WorkloadRiskVector._clamp01(hydraulic_risk),
        runcertainty=WorkloadRiskVector._clamp01(uncertainty_risk),
        rvelocity=rvelocity,
        rsensor_health=rsensor_health
    )


def compute_ker(risk: WorkloadRiskVector, vt_before: float, include_extended: bool = False) -> WorkloadKer:
    """
    Compute K,E,R triad from risk vector and prior residual.
    
    Logic:
    - K (Knowledge): High when max risk is low and ΔVt ≤ 0
    - E (Eco-impact): High when residual is low and ΔVt ≤ 0
    - R (Risk-of-harm): Derived from residual, increased when ΔVt > 0
    
    Args:
        risk: Normalized risk vector
        vt_before: Prior Lyapunov residual
        include_extended: Whether to include extended risks in residual
    
    Returns:
        WorkloadKer with computed scores
    """
    vt_before_clamped = max(0.0, vt_before)
    vt_after = risk.residual(include_extended=include_extended)
    delta_vt = vt_after - vt_before_clamped

    max_r = risk.max_risk()

    # K factor: decreases with high risk or positive ΔVt
    k = 0.95 - 0.4 * max_r
    if delta_vt > 0.0:
        k -= 0.25
    k = WorkloadRiskVector._clamp01(k)

    # E factor: decreases with high residual or positive ΔVt
    e = 0.95 - vt_after
    if delta_vt > 0.0:
        e -= 0.3
    e = WorkloadRiskVector._clamp01(e)

    # R factor: increases with residual and positive ΔVt
    r = vt_after
    if delta_vt > 0.0:
        r += delta_vt
    r = WorkloadRiskVector._clamp01(r)

    return WorkloadKer(vt=vt_after, delta_vt=delta_vt, k=k, e=e, r=r)


def evaluate_workload(
    energy_req_j: float,
    energy_surplus_j: float,
    hydraulic_risk: float,
    uncertainty_risk: float,
    vt_before: float = 0.0,
    canal_velocity_mps: float = 0.0,
    sensor_health: float = 1.0,
    include_extended: bool = False
) -> Dict[str, Any]:
    """
    Evaluate a complete workload sample and return all metrics.
    
    This is the main entry point for AI-chat platforms and coding-agents.
    
    Args:
        energy_req_j: Required energy in Joules
        energy_surplus_j: Available surplus energy in Joules
        hydraulic_risk: Raw hydraulic risk [0, 1]
        uncertainty_risk: Raw telemetry/model uncertainty [0, 1]
        vt_before: Prior Lyapunov residual
        canal_velocity_mps: Canal water velocity in m/s
        sensor_health: Sensor health score [0, 1]
        include_extended: Whether to include extended risks in calculations
    
    Returns:
        Dictionary with all computed metrics
    """
    risk = normalize_risk(
        energy_req_j, energy_surplus_j, hydraulic_risk, uncertainty_risk,
        canal_velocity_mps, sensor_health
    )
    
    ker = compute_ker(risk, vt_before, include_extended=include_extended)
    
    return {
        "risk": risk.to_dict(),
        "vt_before": max(0.0, vt_before),
        "vt_after": ker.vt,
        "delta_vt": ker.delta_vt,
        "ker": ker.to_dict(),
        "is_safe": ker.is_safe()
    }


def make_sample(
    sample_id: str,
    node_id: str,
    timestamp_utc: str,
    energy_req_j: float,
    energy_surplus_j: float,
    hydraulic_risk: float,
    uncertainty_risk: float,
    vt_before: float = 0.0,
    canal_velocity_mps: float = 0.0,
    sensor_health: float = 1.0,
    include_extended: bool = False
) -> WorkloadSample:
    """
    Create a complete WorkloadSample with all computed fields.
    
    Args:
        sample_id: Unique sample identifier
        node_id: Phoenix canal node identifier
        timestamp_utc: ISO 8601 UTC timestamp
        energy_req_j: Required energy in Joules
        energy_surplus_j: Available surplus energy in Joules
        hydraulic_risk: Raw hydraulic risk [0, 1]
        uncertainty_risk: Raw telemetry/model uncertainty [0, 1]
        vt_before: Prior Lyapunov residual
        canal_velocity_mps: Canal water velocity in m/s
        sensor_health: Sensor health score [0, 1]
        include_extended: Whether to include extended risks
    
    Returns:
        Complete WorkloadSample object
    """
    risk = normalize_risk(
        energy_req_j, energy_surplus_j, hydraulic_risk, uncertainty_risk,
        canal_velocity_mps, sensor_health
    )
    
    vt_before_clamped = max(0.0, vt_before)
    vt_after = risk.residual(include_extended=include_extended)
    delta_vt = vt_after - vt_before_clamped
    ker = compute_ker(risk, vt_before, include_extended=include_extended)
    
    return WorkloadSample(
        sample_id=sample_id,
        node_id=node_id,
        timestamp_utc=timestamp_utc,
        energy_req_j=energy_req_j,
        energy_surplus_j=energy_surplus_j,
        hydraulic_risk=hydraulic_risk,
        uncertainty_risk=uncertainty_risk,
        canal_velocity_mps=canal_velocity_mps,
        sensor_health=sensor_health,
        risk=risk,
        vt_before=vt_before_clamped,
        vt_after=vt_after,
        delta_vt=delta_vt,
        ker=ker
    )


# =============================================================================
# SQLite Integration
# =============================================================================

def ensure_daily_progress_schema(conn: sqlite3.Connection) -> None:
    """
    Ensure the daily_progress table exists with all required columns.
    
    Extended schema includes canal_velocity_mps, velocity_risk, 
    sensor_health, and sensor_health_risk as per next-step research.
    """
    cursor = conn.cursor()
    
    cursor.executescript("""
        PRAGMA foreign_keys = ON;
        
        CREATE TABLE IF NOT EXISTS daily_progress (
            progress_id       INTEGER PRIMARY KEY AUTOINCREMENT,
            yyyymmdd          TEXT    NOT NULL,
            domain            TEXT    NOT NULL,
            subtask_id        TEXT    NOT NULL,
            node_id           TEXT    NOT NULL,
            sample_id         TEXT    NOT NULL,
            timestamp_utc     TEXT    NOT NULL,
            energy_req_j      REAL    NOT NULL,
            energy_surplus_j  REAL    NOT NULL,
            hydraulic_risk    REAL    NOT NULL,
            uncertainty_risk  REAL    NOT NULL,
            canal_velocity_mps REAL   DEFAULT 0.0,
            sensor_health     REAL    DEFAULT 1.0,
            renergy           REAL    NOT NULL,
            rhydraulic        REAL    NOT NULL,
            runcertainty      REAL    NOT NULL,
            rvelocity         REAL    DEFAULT 0.0,
            rsensor_health    REAL    DEFAULT 0.0,
            vt_before         REAL    NOT NULL,
            vt_after          REAL    NOT NULL,
            delta_vt          REAL    NOT NULL,
            k_factor          REAL    NOT NULL,
            e_factor          REAL    NOT NULL,
            r_factor          REAL    NOT NULL,
            phoenix_hex       TEXT    NOT NULL,
            prior_pointer     TEXT    NOT NULL
        );
        
        CREATE INDEX IF NOT EXISTS idx_daily_progress_date 
            ON daily_progress(yyyymmdd);
        
        CREATE INDEX IF NOT EXISTS idx_daily_progress_node_time 
            ON daily_progress(node_id, timestamp_utc);
        
        -- View: Per-node workload summary over time windows
        CREATE VIEW IF NOT EXISTS v_cybo_workload_window AS
        SELECT 
            node_id,
            yyyymmdd,
            COUNT(*) as sample_count,
            AVG(energy_req_j) as avg_energy_req_j,
            AVG(energy_surplus_j) as avg_energy_surplus_j,
            AVG(renergy) as avg_renergy,
            AVG(rhydraulic) as avg_rhydraulic,
            AVG(runcertainty) as avg_runcertainty,
            AVG(rvelocity) as avg_rvelocity,
            AVG(rsensor_health) as avg_rsensor_health,
            AVG(vt_after) as avg_vt_after,
            AVG(k_factor) as avg_k_factor,
            AVG(e_factor) as avg_e_factor,
            AVG(r_factor) as avg_r_factor,
            MAX(delta_vt) as max_delta_vt,
            MIN(k_factor) as min_k_factor,
            MIN(e_factor) as min_e_factor,
            MAX(r_factor) as max_r_factor
        FROM daily_progress
        GROUP BY node_id, yyyymmdd;
        
        -- View: Safe workload candidates (K>=0.9, E>=0.9, R<=0.15)
        CREATE VIEW IF NOT EXISTS v_safe_workload_candidates AS
        SELECT *
        FROM daily_progress
        WHERE k_factor >= 0.9 
          AND e_factor >= 0.9 
          AND r_factor <= 0.15
          AND delta_vt <= 0.0;
    """)
    
    conn.commit()


def insert_daily_progress(
    conn: sqlite3.Connection,
    sample: WorkloadSample,
    yyyymmdd: str = "20260713",
    subtask_id: str = "PHX-CANAL-WL-2026-07-13",
    phoenix_hex: str = "0x20260713PHX3345NWorkloadEnergyDeltaVtPython",
    prior_pointer: str = "20260709/workload_energy_dvt_rust"
) -> int:
    """
    Insert a workload sample into the daily_progress table.
    
    Args:
        conn: SQLite connection
        sample: WorkloadSample object
        yyyymmdd: Date string (YYYYMMDD)
        subtask_id: Subtask identifier
        phoenix_hex: Phoenix corridor evidence hex
        prior_pointer: Pointer to prior day's artifact
    
    Returns:
        The row ID of the inserted record
    """
    cursor = conn.cursor()
    
    cursor.execute("""
        INSERT INTO daily_progress (
            yyyymmdd, domain, subtask_id,
            node_id, sample_id, timestamp_utc,
            energy_req_j, energy_surplus_j,
            hydraulic_risk, uncertainty_risk,
            canal_velocity_mps, sensor_health,
            renergy, rhydraulic, runcertainty,
            rvelocity, rsensor_health,
            vt_before, vt_after, delta_vt,
            k_factor, e_factor, r_factor,
            phoenix_hex, prior_pointer
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    """, (
        yyyymmdd,
        "workload_energy_dvt",
        subtask_id,
        sample.node_id,
        sample.sample_id,
        sample.timestamp_utc,
        sample.energy_req_j,
        sample.energy_surplus_j,
        sample.hydraulic_risk,
        sample.uncertainty_risk,
        sample.canal_velocity_mps,
        sample.sensor_health,
        sample.risk.renergy,
        sample.risk.rhydraulic,
        sample.risk.runcertainty,
        sample.risk.rvelocity,
        sample.risk.rsensor_health,
        sample.vt_before,
        sample.vt_after,
        sample.delta_vt,
        sample.ker.k,
        sample.ker.e,
        sample.ker.r,
        phoenix_hex,
        prior_pointer
    ))
    
    conn.commit()
    return cursor.lastrowid


def query_daily_progress(
    conn: sqlite3.Connection,
    yyyymmdd: Optional[str] = None,
    node_id: Optional[str] = None,
    limit: int = 100
) -> List[Dict[str, Any]]:
    """
    Query daily_progress records with optional filters.
    
    Args:
        conn: SQLite connection
        yyyymmdd: Filter by date (optional)
        node_id: Filter by node (optional)
        limit: Maximum number of rows to return
    
    Returns:
        List of dictionaries representing workload samples
    """
    cursor = conn.cursor()
    
    conditions = []
    params = []
    
    if yyyymmdd:
        conditions.append("yyyymmdd = ?")
        params.append(yyyymmdd)
    
    if node_id:
        conditions.append("node_id = ?")
        params.append(node_id)
    
    where_clause = ""
    if conditions:
        where_clause = "WHERE " + " AND ".join(conditions)
    
    query = f"""
        SELECT * FROM daily_progress
        {where_clause}
        ORDER BY timestamp_utc DESC
        LIMIT ?
    """
    params.append(limit)
    
    cursor.execute(query, params)
    columns = [desc[0] for desc in cursor.description]
    
    return [dict(zip(columns, row)) for row in cursor.fetchall()]


# =============================================================================
# CLI Interface
# =============================================================================

def main():
    """
    Command-line interface for cyboquatic workload evaluation.
    
    Usage examples:
        # Evaluate workload (basic):
        python cyboquatic_workload.py eval 1000 1200 0.2 0.3 0.1
        
        # Evaluate with extended metrics:
        python cyboquatic_workload.py eval-extended 1000 1200 0.2 0.3 0.1 1.5 0.95
        
        # Insert into database:
        python cyboquatic_workload.py insert mydb.sqlite PHX-NODE-01 SAMPLE-001
        
        # Query database:
        python cyboquatic_workload.py query mydb.sqlite --date 20260713
    """
    import sys
    import json
    
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    command = sys.argv[1]
    
    if command == "eval":
        if len(sys.argv) != 7:
            print("Usage: python cyboquatic_workload.py eval <energy_req_j> <energy_surplus_j> <hydraulic_risk> <uncertainty_risk> <vt_before>")
            sys.exit(1)
        
        result = evaluate_workload(
            float(sys.argv[2]),
            float(sys.argv[3]),
            float(sys.argv[4]),
            float(sys.argv[5]),
            float(sys.argv[6])
        )
        print(json.dumps(result, indent=2))
    
    elif command == "eval-extended":
        if len(sys.argv) != 9:
            print("Usage: python cyboquatic_workload.py eval-extended <energy_req_j> <energy_surplus_j> <hydraulic_risk> <uncertainty_risk> <vt_before> <canal_velocity_mps> <sensor_health>")
            sys.exit(1)
        
        result = evaluate_workload(
            float(sys.argv[2]),
            float(sys.argv[3]),
            float(sys.argv[4]),
            float(sys.argv[5]),
            float(sys.argv[6]),
            float(sys.argv[7]),
            float(sys.argv[8]),
            include_extended=True
        )
        print(json.dumps(result, indent=2))
    
    elif command == "insert":
        if len(sys.argv) < 5:
            print("Usage: python cyboquatic_workload.py insert <db_path> <node_id> <sample_id> [options]")
            sys.exit(1)
        
        db_path = sys.argv[2]
        node_id = sys.argv[3]
        sample_id = sys.argv[4]
        
        # Default test values
        energy_req_j = float(sys.argv[5]) if len(sys.argv) > 5 else 1000.0
        energy_surplus_j = float(sys.argv[6]) if len(sys.argv) > 6 else 1200.0
        hydraulic_risk = float(sys.argv[7]) if len(sys.argv) > 7 else 0.2
        uncertainty_risk = float(sys.argv[8]) if len(sys.argv) > 8 else 0.3
        vt_before = float(sys.argv[9]) if len(sys.argv) > 9 else 0.1
        canal_velocity_mps = float(sys.argv[10]) if len(sys.argv) > 10 else 1.0
        sensor_health = float(sys.argv[11]) if len(sys.argv) > 11 else 0.95
        
        conn = sqlite3.connect(db_path)
        ensure_daily_progress_schema(conn)
        
        timestamp = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H%M%SZ")
        sample = make_sample(
            sample_id, node_id, timestamp,
            energy_req_j, energy_surplus_j,
            hydraulic_risk, uncertainty_risk,
            vt_before, canal_velocity_mps, sensor_health,
            include_extended=True
        )
        
        row_id = insert_daily_progress(conn, sample)
        print(f"Inserted sample with row_id={row_id}")
        print(f"K={sample.ker.k:.4f} E={sample.ker.e:.4f} R={sample.ker.r:.4f} safe={sample.ker.is_safe()}")
        
        conn.close()
    
    elif command == "query":
        if len(sys.argv) < 3:
            print("Usage: python cyboquatic_workload.py query <db_path> [--date YYYYMMDD] [--node NODE_ID]")
            sys.exit(1)
        
        db_path = sys.argv[2]
        yyyymmdd = None
        node_id = None
        
        i = 3
        while i < len(sys.argv):
            if sys.argv[i] == "--date" and i + 1 < len(sys.argv):
                yyyymmdd = sys.argv[i + 1]
                i += 2
            elif sys.argv[i] == "--node" and i + 1 < len(sys.argv):
                node_id = sys.argv[i + 1]
                i += 2
            else:
                i += 1
        
        conn = sqlite3.connect(db_path)
        results = query_daily_progress(conn, yyyymmdd, node_id)
        
        print(json.dumps(results, indent=2))
        conn.close()
    
    elif command == "schema":
        if len(sys.argv) < 3:
            print("Usage: python cyboquatic_workload.py schema <db_path>")
            sys.exit(1)
        
        db_path = sys.argv[2]
        conn = sqlite3.connect(db_path)
        ensure_daily_progress_schema(conn)
        print(f"Schema ensured in {db_path}")
        conn.close()
    
    else:
        print(f"Unknown command: {command}")
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
