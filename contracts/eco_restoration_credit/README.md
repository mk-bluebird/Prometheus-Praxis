<!-- filename: contracts/eco_restoration_credit/README.md -->
<!-- destination: https://github.com/mk-bluebird/Prometheus-Praxis/contracts/eco_restoration_credit/README.md -->
<!-- license: MIT OR Apache-2.0 -->

# Eco Restoration Credit Contract (Bostrom / CosmWasm)

This contract implements a non‑actuating eco‑restoration credit protocol on Bostrom, tightly bound to the Phoenix KER‑Lyapunov constitution and hex risk shards.[file:14][web:130][web:136]

## Overview

- **Purpose**:
  - Mint eco‑restoration credits when a Lyapunov‑verified AI workload completes.
  - Burn credits when physical restoration (trees, canal remediation) is completed.
  - Tie every credit to a Phoenix hex (`hex_id`), workload (`workload_id`), and risk shard (`phoenix.uhi.hex.risk.v1.aln` or similar).[file:14]

- **Non‑actuating**:
  - The contract never controls pumps, HVAC, or physical machinery.
  - It only manages ledger entries for eco‑credits, assuming off‑chain Lyapunov and corridor verification.

## Messages

### InstantiateMsg

```json
{
  "admin": "bostrom1..."
}
```

- `admin`:
  - Address allowed to mint and burn credits.
  - Typically a multisig or governance controller.

### ExecuteMsg

- `MintEcoCredit`:

  ```json
  {
    "mint_eco_credit": {
      "to": "bostrom1recipient...",
      "hex_id": "PHX-HEX-1234",
      "workload_id": "ai-job-ker-001",
      "amount": "100",
      "delta_v": "-0.015",
      "ker_hash": "sha256:...",
      "risk_shard_id": "phoenix.uhi.hex.risk.v1.aln"
    }
  }
  ```

  - Mints `amount` eco‑credits to `to`, tied to `hex_id` and `workload_id`.
  - `delta_v`:
    - Encodes Lyapunov residual change `V(t+1) - V(t)`, or a hash of the proof.
    - Must be negative and verified off‑chain.
  - `ker_hash`:
    - Hash of K/E/R factors and invariants computed by `ecosafety-core-v2`.
  - `risk_shard_id`:
    - ID of the ALN shard carrying the hex risk coordinates (e.g., UHI triad).[file:14]

- `BurnEcoCredit`:

  ```json
  {
    "burn_eco_credit": {
      "from": "bostrom1recipient...",
      "hex_id": "PHX-HEX-1234",
      "workload_id": "ai-job-ker-001",
      "amount": "100",
      "completion_hash": "sha256:restoration-proof..."
    }
  }
  ```

  - Burns `amount` credits from `from` for the given `hex_id` and `workload_id`.
  - `completion_hash`:
    - Encodes proof of restoration completion (e.g., field report, monitoring data).
    - Verified off‑chain or via oracles.

### QueryMsg

- `GetBalance`:

  ```json
  {
    "get_balance": {
      "address": "bostrom1recipient...",
      "hex_id": "PHX-HEX-1234"
    }
  }
  ```

  - Returns all `CreditRecord`s for an address, optionally filtered by `hex_id`.

## Internal Data Structures

- `Config`:
  - Stores `admin` address.
- `CreditRecord`:
  - `hex_id`, `workload_id`.
  - `amount` (Uint128).
  - `delta_v`, `ker_hash`, `risk_shard_id`.
  - `completed` flag (restoration done).

Storage layout:

- Keys under `balance:addr:hex_id:workload_id` store serialized `CreditRecord`.

## Governance and Invariants

- Minting:

  - Only `admin` can call `MintEcoCredit`.
  - The contract requires non‑empty `delta_v`, `ker_hash`, `risk_shard_id`.
  - Lyapunov invariant (`delta_v < 0`) and non‑offsettable plane compliance are checked off‑chain using `ecosafety-core-v2` before mint.[file:14]

- Burning:

  - Only `admin` can call `BurnEcoCredit`.
  - `completion_hash` must be provided and validated off‑chain.
  - Marks `CreditRecord.completed = true` after burn.

- Linkage:

  - `risk_shard_id` should refer to a valid ALN shard (e.g. `phoenix.uhi.hex.risk.v1.aln`) registered in the Prometheus‑Praxis repo index and AI‑safe catalog.[file:14]

## Build and Deploy

Prerequisites:[web:138]

- Rust toolchain with `wasm32-unknown-unknown` target.
- CosmWasm build tools.

Build:

```bash
cd contracts/eco_restoration_credit
cargo wasm
```

Run unit tests:

```bash
RUST_BACKTRACE=1 cargo unit-test
```

Generate JSON schema for messages:

```bash
cargo schema
```

Deploy to Bostrom (example):

1. Upload `.wasm`:

   ```bash
   wasmcli tx wasm store eco_restoration_credit.wasm --from <your-key> --gas auto -y
   ```

2. Instantiate with admin address:

   ```bash
   wasmcli tx wasm instantiate <code-id> '{"admin": "<admin-address>"}' \
     --from <your-key> --label "eco-restoration-credit" --gas auto -y
   ```

3. Use `MintEcoCredit` and `BurnEcoCredit` messages through your governance pipelines or multisig controllers.

## Safety and Ecosystem Integration

- Non‑actuating:
  - Contract handles credits only, not physical actions.
  - Physical interventions are controlled by other systems and human governance.

- Ecosystem integration:
  - Tied to Phoenix hex UHI shards and KER‑Lyapunov constitution via `ker_hash`, `delta_v`, and `risk_shard_id`.
  - Discoverable through AI‑safe catalog and manifest views in Prometheus‑Praxis.[file:14]

This contract, together with `ecosafety-core-v2` and the Phoenix hex risk shards, forms the ledger layer of your AI‑to‑eco‑restoration pipeline, ensuring credits are minted only when workloads provably improve systemic stability and burned once real restoration is completed.
