## Servo JLF controller and TitanMetrics

- The **servo JLF controller** writes RFC4180 CSV telemetry rows to `titan-net/qpudatashards/ServoJlfHealthCoord2026v1.csv`.[^1]
- Each row corresponds to one control tick for a given `nodeid,axisid` and must include:
    - `nodeid, axisid` (edge controller identity).
    - `r_tau, r_a` normalized residuals for torque and anomaly, in $[0,1]$.
    - `Jk` (current JLF residual) plus `Jsafe, Jwarn, Jmax` band parameters.
    - `w_tau, w_a` weights used in the JLF quadratic form.
    - `vtbefore, vtafter` local Lyapunov surrogate values for this axis.
    - `evidencehex` tying the row to the loaded profile and kernel version.[^1]


### Ingestion into TitanMetrics2026v1

- Titan-aware Custernodes treat `ServoJlfHealthCoord2026v1.csv` as a standard qpudatashard family and stream it through `titan-metrics-core` alongside other planes.[^1]
- The TitanMetrics kernel maps each row into:
    - A `TitanMetricsInput` record keyed by `shardid = evidencehex` and `plane = SERVO-JLF`.
    - Risk coordinates such as:
        - `rkernelperf` derived from `Jk` and its bands (`Jsafe,Jwarn,Jmax`).
        - Local Lyapunov residual contribution from `vtbefore,vtafter` and $(r_\tau,r_a)$.[^1]
- The kernel enforces the global corridors from `TitanMetricsCorridor2026v1`:
    - $\text{RoH} \leq \text{rohmax}$, $\text{Veco} \leq \text{vecomax}$, $\text{Lyap} \geq \text{lyapmin}$, with typical caps $\text{rohmax} = 0.30, \text{vecomax} = 0.30, \text{lyapmin} = 0.85$.[^1]
- If a servo JLF row would violate these invariants (e.g. `vtafter > vtbefore` or `Jk` exceeding corridor bounds), TitanMetrics either:
    - Emits a metrics row with explicit failure flags for governance analysis, or
    - Quarantines the shard and omits it from corridor aggregation, depending on your EnvPlanes policy.[^1]


### Evidence and provenance

- `evidencehex` in each telemetry row is treated as a **hint** and registry index, not proof by itself.[^1]
- TitanNet resolves `evidencehex` into a full evidence payload (MERKLE lineage, KER summaries, neurorights masks) using the same registry and validators as other planes, then:
    - Recomputes KER and Lyapunov values from raw qpudatashards.
    - Checks non-expansion $V_{t+1} \le V_t$ and corridor caps before admitting the servo metrics into TitanEcoLedger or planner views.[^1]

This keeps the JLF controller firmly in the **non‑actuating** role: it only emits governed telemetry; all trust, risk, and corridor decisions happen inside TitanMetrics2026v1 and the ALN governance layer.[^1]

<div align="center">⁂</div>

[^1]: this-research-encompasses-thre-HXIIdNx_SBWH.PTxfg_zvw.md
