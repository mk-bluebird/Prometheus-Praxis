# 10‑Axis Telemetry → Risk Coordinates

This document is the normative mapping from AI node telemetry into the risk
coordinates used by `AiDatacenterNode2026v1` and by `ai_node_shard`.[file:91]

## Primary axes (contribute to Vt)

- CUE (kg CO₂/kWh)
  - Ideal: 0.05
  - Ceiling: 0.40
  - Risk: \( r_{\text{cue}} = \max(0, \min(1, (x - 0.05) / (0.40 - 0.05))) \).[file:91]
- Eco‑per‑Joule
  - Ideal: 1.0
  - Floor: 0.2
  - Risk: 0 if \(x \ge 1.0\); else \( r = \max(0, \min(1, (1.0 - x) / (1.0 - 0.2))) \).[file:91]
- PUE
  - Ideal: 1.05
  - Ceiling: 1.40
  - Risk: \( r_{\text{pue}} = \max(0, \min(1, (x - 1.05) / (1.40 - 1.05))) \).[file:91]
- Eco‑task ratio (fraction)
  - Ideal: 0.50+
  - Floor: 0.20
  - Risk: 0 if \(x \ge 0.50\); else \( r = \max(0, \min(1, (0.50 - x) / (0.50 - 0.20))) \).[file:91]
- WUE (L/kWh)
  - Ideal: 0.20
  - Ceiling: 1.50
  - Risk: \( r_{\text{wue}} = \max(0, \min(1, (x - 0.20) / (1.50 - 0.20))) \).[file:91]
- Embodied CO₂
  - Ideal: 0
  - Ceiling: 100
  - Risk: \( r_{\text{embodied}} = \max(0, \min(1, x / 100)) \).[file:91]
- Energy intensity
  - Ideal: 0.10 kWh/workload
  - Ceiling: 0.50
  - Risk: \( r_{\text{energy}} = \max(0, \min(1, (x - 0.10) / (0.50 - 0.10))) \).[file:91]
- Joules per inference
  - Ideal: 1.0
  - Ceiling: 10.0
  - Risk: \( r_{\text{jin}} = \max(0, \min(1, (x - 1.0) / (10.0 - 1.0))) \).[file:91]
- Heat reuse (ERE)
  - Ideal: ≥ 0.5
  - Risk: 0 if \(x \ge 0.5\); else \( r_{\text{heat}} = \max(0, \min(1, (0.5 - x) / 0.5)) \).[file:91]
- Topology violations
  - Ideal: 0
  - Ceiling: 5
  - Risk: \( r_{\text{top}} = \max(0, \min(1, \text{violations} / 5)) \).[file:91]

## Secondary axes (K‑boost only)

- Tokens per joule:
  - Normalized as \( b_{\text{tok}} = \max(0, \min(1, x / 10^6)) \).[file:91]
- Utilisation:
  - \( b_{\text{util}} = \max(0, \min(1, x / 0.9)) \).[file:91]
- ERE:
  - \( b_{\text{ere}} = \max(0, \min(1, x / 1.0)) \).[file:91]

K‑boost fraction:
- \(K_{\text{boost}} = 0.05 \cdot (0.4\,b_{\text{tok}} + 0.3\,b_{\text{util}} + 0.3\,b_{\text{ere}})\).[file:92]
