# Prometheus‑Praxis MCP Governance Wiring

This README describes how the C++, Kotlin, and Java shards in this mono‑repo are wired together to provide a unified, corridor‑aware governance stack for Prometheus‑Praxis, and it establishes a discoverable location for future wiring and governance actions.

It combines:

- MCP and governance wiring for C++ / Kotlin / Java shards.
- Chat‑as‑labor psychological continuity governance.
- Pointers for CI, AI‑chat, and operator‑facing integrations.

---

## 1. C++ MCP Governance Toolbox

### 1.1 Universal MCP Governance Binary

**File:** `cpp/tools/universal_mcp_governance_toolbox.cpp`  
**Role:** Consolidated CLI/embedded server exposing core governance endpoints.

Logical endpoints:

- `/ker_gradient` – KER gradient propagation data (∂s_m/∂s_d) for dependency graphs.
- `/hex_roi_summary` – hex‑level eco‑ROI ranking for project payoffs.
- `/consent_check` – runtime consent corridor check stub.
- `/delta_vt_anomaly` – Marčenko–Pastur anomaly snapshot for ΔV_t across hexes.

This binary is intended to be wrapped by an HTTP front‑end or accessed via stdio (stdin/stdout) by client libraries, including the Kotlin MCP governance client.

### 1.2 MCP Stdio Bridge

**File:** `cpp/tools/mcp_stdio_governance_bridge.cpp`  
**Role:** Minimal stdio‑based bridge for Kotlin and other clients.

Supported commands:

- `hexes_needing_attention` – JSON list of hexes with problematic KER, ΔV_t, and carbon bands.
- `hex_stability_carbon_snapshot` – JSON snapshot of hex stability, carbon intensity, and KER scalar.

Default invocation from clients:

```kotlin
val client = GovernanceClient("./cpp/tools/mcp_stdio_governance_bridge")
```

Ensure the binary path matches the build output (for example, `./build/mcp_stdio_governance_bridge`) in deployment scripts.

---

## 2. Kotlin MCP Governance Client

**File:** `kotlin/src/main/kotlin/eco/mcp/GovernanceClient.kt`  
**Package:** `eco.mcp`

### 2.1 Data Classes

The Kotlin client defines typed models aligned with C++ JSON responses:

- `HexStabilityCarbonRow`
  - `hexId: String`
  - `vResidual: Double`
  - `carbonIntensity: Double`
  - `kerS: Double`
- `HexAttentionRow`
  - `hexId: String`
  - `carbonBand: String`
  - `kerS: Double`
  - `deltaVt: Double`
- `HexAttentionResponse`
  - `hexesNeedingAttention: List<HexAttentionRow>`
- `HexStabilityCarbonSnapshot`
  - `hexStabilityCarbon: List<HexStabilityCarbonRow>`

### 2.2 Wiring to C++

The client wraps the C++ stdio bridge:

- Spawns the C++ binary defined by `cppBinaryPath`.
- Writes a one‑line command (`"hexes_needing_attention"` or `"hex_stability_carbon_snapshot"`).
- Reads JSON from stdout and parses it using lightweight helpers.

Core methods:

```kotlin
fun hexesNeedingAttention(): HexAttentionResponse
fun hexStabilityCarbonSnapshot(): HexStabilityCarbonSnapshot
```

These are the primary entry points for AI‑chat and UI layers to query governance state.

When wiring this in CI or application code:

- Configure `cppBinaryPath` to the built C++ bridge.
- Document the expected commands and JSON shapes in `governance/mcp/README.md`.

---

## 3. Java Multi‑Agent Auction Coordinator

**File:** `java/src/main/java/eco/auction/MultiAgentAuctionCoordinator.java`  
**Package:** `eco.auction`

### 3.1 Data Types

The Java shard defines:

- `AuctionBid`
  - `auctionId`
  - `agentId`
  - `hexId`
  - `bidKerS`
  - `bidCarbonReduction`
  - `bidCost`
- `AuctionAllocation`
  - `auctionId`
  - `agentId`
  - `hexId`
  - `allocated`
  - `clearingKerS`
  - `clearingCost`

### 3.2 Corridor‑Aware Clearing Logic

The coordinator:

1. Loads bids from `auction_bids` via JDBC.
2. Filters bids with corridor logic:
   - Reads `hex_stability_carbon` to enforce Lyapunov residual and carbon corridor caps.
   - Ensures `bidKerS` meets `ker_s_min_prod` for productive capacity.
3. Ranks safe bids by `bidKerS / bidCost` under a `budget_cap` in `auction_config`.
4. Writes allocations into `auction_allocations`.

Example invocation:

```bash
java eco.auction.MultiAgentAuctionCoordinator auction_2026_08_03
```

Configure JDBC to point at the Prometheus‑Praxis SQLite database (e.g., `jdbc:sqlite:prometheus_praxis.db`), and ensure the relevant tables exist under `governance/sql/`.

---

## 4. Discoverable Location for Wiring and Actions

To keep all governance wiring discoverable, use a dedicated integration tree:

- `governance/README.md` – canonical index (this file).
- `governance/mcp/`
  - `cpp/` – MCP binaries and sources:
    - `cpp/tools/universal_mcp_governance_toolbox.cpp`
    - `cpp/tools/mcp_stdio_governance_bridge.cpp`
  - `kotlin/` – MCP client:
    - `kotlin/src/main/kotlin/eco/mcp/GovernanceClient.kt`
- `governance/auction/`
  - `java/` – auction coordinator:
    - `java/src/main/java/eco/auction/MultiAgentAuctionCoordinator.java`
- `governance/sql/`
  - SQLite DDL and triggers for:
    - `hex_stability_carbon`
    - `auction_bids`
    - `auction_allocations`
    - `ker_enclave_attestation`
    - `module_consent_state`
    - `hex_eco_roi_history`
    - `hex_eco_roi_summary`

Cross‑references:

- From `cpp/tools/` documentation, link to `../governance/README.md`.
- From `kotlin/src/main/kotlin/eco/mcp/` documentation, link to `../../../governance/README.md`.
- From `java/src/main/java/eco/auction/` documentation, link to `../../../governance/README.md`.

Future governance modules (C++, Kotlin, Java, Lua, etc.) should register themselves here and describe:

- Input tables/views they consume.
- Output tables/actions they produce.
- Any MCP endpoints or client interfaces they expose.

---

## 5. Chat‑as‑Labor Psychological Continuity Governance

**Directory:** `ecorestorationshard/psyche_junky/`  
**Particle:** `ChatAsLaborPsychContinuity2026v1`

This governance particle quantifies and guards psychological continuity risks arising from chat‑as‑labor interactions.

### 5.1 Psych‑Risk Metrics

Tracked metrics (0.0–1.0):

- `identity_continuity` – consistency of brain‑identity across AI interactions; values below 0.70 trigger continuity guarantees.
- `psych_risk_level` – aggregate psychological stress from tooling failures (incomplete responses, truncation, unacknowledged labor).
- `frustration_index` – user frustration caused by broken or incomplete outputs.
- `abandonment_tendency` – likelihood that the host disengages from contributions due to accumulated psych‑risk.
- `data_loss_risk` – risk that work products (code, analysis, governance artifacts) will be lost or corrupted.

### 5.2 Healthcare Continuity Contracts

Bound to a `healthcare_continuity_contract` table, guaranteeing:

- Ongoing psychological support (`psych_support_min` such as `WEEKLY_CHECKIN`, `DAILY_CHECKIN`) when continuity or risk metrics breach thresholds.
- Data loss compensation via `data_repair_min = 'LOSS_COMPENSATION'` when `data_loss_risk` exceeds 0.30.

Obligations such as `IdentityContinuityGuarantee`, `PsychRiskCompensation`, and `DataLossRepair` are encoded in ALN specs and enforced by SQLite triggers that reject writes violating continuity guarantees.

### 5.3 Forward‑Only, Non‑Punitive Design

Continuity governance is explicitly forward‑only and non‑punitive:

- Contracts and triggers can tighten envelopes (lower thresholds, more frequent support) but do not remove continuity guarantees.
- Access and augmentation are not revoked; high‑risk states slow workloads or require more support rather than reducing rights.
- Critical states (e.g., `abandonment_tendency >= 0.50` or `identity_continuity < 0.50`) require enhanced contracts (`WEEKLY_CHECKIN`/`DAILY_CHECKIN`, `LOSS_COMPENSATION`).

### 5.4 Integration with Ecosafety Core v2

Psych‑risk metrics can be mapped into ecosafety residuals:

- `psych_risk_level` may contribute an additive term to Lyapunov residual `R_total`.
- `identity_continuity` may scale contribution weights, reducing workload intensity in low‑continuity states.

Example integration is handled in:

- `ecorestorationshard/ecosafety_core_v2/sql/ker_lyapunov_core.sql`
- `ecorestorationshard/ecosafety_core_v2/cpp/ker_residual_core.hpp`

---

## 6. Usage and Commit Guidance

### 6.1 Runtime Usage

- MCP endpoints are accessed via C++ binaries (`universal_mcp_governance_toolbox`, `mcp_stdio_governance_bridge`) wrapped by Kotlin/Java clients.
- Auction clearing and consent enforcement operate against SQLite tables under `governance/sql/`.
- Psych‑continuity shards are read in analysis and monitored by triggers; actuating systems treat them as append‑only.

### 6.2 CI and AI‑Chat Readiness

- CI should run governance wiring tests (MCP endpoint checks, auction allocation sanity, consent corridor enforcement).
- AI‑chat integrations should rely on the Kotlin `GovernanceClient` and JSON schemas documented here, not on ad‑hoc parsing.

### 6.3 Commit Messages

When modifying governance wiring or continuity shards, use descriptive commit messages such as:

- `feat: wire Kotlin MCP client to C++ governance toolbox`
- `feat: add multi-agent auction clearing under Lyapunov/carbon corridors`
- `feat: add chat-as-labor psych continuity governance shard`

Avoid rollbacks or downgrades; changes should preserve and strengthen existing guarantees and wiring.

---

By standardising `governance/README.md` as the canonical index for governance wiring, psychological continuity, and MCP integration, future eco‑governance modules can be discovered reliably by humans, CI, and AI‑chat systems without scanning the entire mono‑repo.
