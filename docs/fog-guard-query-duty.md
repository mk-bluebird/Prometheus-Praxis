# FogGuardQueryDuty

`FogGuardQueryDuty` is the Javasphere browser-duty facade for querying FOG guard (`safestep`) and FOG router decisions from a Rust / ALN / SQLite backend. It exposes a small set of JSON request and response shapes that AI browsers and agents can use without touching actuation stacks. [file:24][file:70]

The duty assumes a transport function (MCP, HTTP, or similar) that forwards JSON to a non-actuating gateway, where `cyboquatic-ecosafety` and `fog-router-guard` implement the semantics. [file:23][file:24]

---

## 1. Transport Contract

The duty never performs IO itself. It requires an injected `transport`:

```js
const duty = new FogGuardQueryDuty(async (request) => {
  // send request over MCP/HTTP to Rust gateway
  // return parsed JSON response
});
```

The `request` object is one of the types documented below. The transport must be:

- Side-effect free from the browser’s perspective.
- Non-actuating: it must not directly drive hardware; that remains in a separate, guarded stack. [file:23][file:70]

---

## 2. FogGuardEvaluateRequest

### Purpose

Ask the backend to run `safestep(envelope, corridor_present, cfg)` for a given node, using the latest `CyboNodeEcosafetyEnvelope` row from the ecosafety shard. [file:24]

### Request JSON

```json
{
  "type": "FogGuardEvaluateRequest",
  "nodeId": "string",
  "familyId": "string",
  "corridorPresent": true,
  "guardConfig": {
    "bands": {
      "roh_ceiling": 0.3,
      "residual_max": 0.0
    },
    "ker": {
      "k_min": 0.9,
      "e_min": 0.9,
      "r_max": 0.2
    }
  }
}
```

- `type`  
  - Literal `"FogGuardEvaluateRequest"` identifying this request.  
- `nodeId`  
  - Identifier for the FOG / sewer node; used to fetch the latest ecosafety envelope. [file:24]  
- `familyId`  
  - ALN shard family, default `"CyboquaticEcosafetyEnvelopePhoenix2026v1"`. [file:24]  
- `corridorPresent`  
  - Boolean flag indicating whether the route planner found a valid corridor for this step. [file:23]  
- `guardConfig` (optional)  
  - Overrides for the FOG guard configuration; if omitted, the backend uses `FogGuardConfig::default()`. [file:24]  

If `guardConfig` is omitted:

```json
"guardConfig": null
```

### Response JSON

```json
{
  "type": "FogGuardEvaluateResponse",
  "nodeId": "string",
  "familyId": "string",
  "verdict": "Allow",
  "details": {
    "roh": 0.12,
    "residual": -0.001,
    "k": 0.93,
    "e": 0.94,
    "r": 0.12,
    "safestep_ok": true,
    "corridor_present": true
  }
}
```

- `type`  
  - Literal `"FogGuardEvaluateResponse"`.  
- `nodeId`, `familyId`  
  - Echo of the request.  
- `verdict`  
  - String representation of `FogGuardVerdict`: `"Allow"` or `"Stop"`. [file:24]  
- `details`  
  - Optional diagnostic fields, derived from `FogGuardInput`:  
  - `roh`: risk-of-harm scalar \(R\) from `RiskVector.maxcoord`. [file:24]  
  - `residual`: Lyapunov residual \(V_{t+1} - V_t\). [file:24]  
  - `k`, `e`, `r`: KER window values. [file:24]  
  - `safestep_ok`: upstream `kerdeployable` flag. [file:24]  
  - `corridor_present`: final corridor flag used in the guard. [file:23]  

---

## 3. FogRouteDecisionRequest

### Purpose

Ask the backend FOG router guard to compute a `FogRouteDecision` (`AllowRoute` / `BlockRoute`) for a node and optional routing window. [file:23][file:24]

### Request JSON

```json
{
  "type": "FogRouteDecisionRequest",
  "nodeId": "string",
  "familyId": "string",
  "windowId": "optional-string",
  "guardConfig": {
    "bands": {
      "roh_ceiling": 0.3,
      "residual_max": 0.0
    },
    "ker": {
      "k_min": 0.9,
      "e_min": 0.9,
      "r_max": 0.2
    }
  }
}
```

- `type`  
  - Literal `"FogRouteDecisionRequest"`.  
- `nodeId`  
  - Node identifier used to build a `FogNodeSnapshot` from ecosafety shards. [file:23][file:24]  
- `familyId`  
  - ALN family for ecosafety envelopes; same default as above. [file:24]  
- `windowId` (optional)  
  - Logical identifier for a routing window; backend-specific (e.g., time bucket or shard key). [file:23]  
- `guardConfig` (optional)  
  - Overrides for `FogGuardConfig`; as above. [file:24]  

### Response JSON

```json
{
  "type": "FogRouteDecisionResponse",
  "nodeId": "string",
  "familyId": "string",
  "windowId": "optional-string",
  "decision": "AllowRoute",
  "context": {
    "verdict": "Allow",
    "roh": 0.18,
    "k": 0.92,
    "e": 0.93,
    "r": 0.18,
    "kerdeployable": true,
    "corridor_present": true
  }
}
```

- `type`  
  - Literal `"FogRouteDecisionResponse"`.  
- `decision`  
  - `"AllowRoute"` or `"BlockRoute"`, mirroring `FogRouteDecision`. [file:23]  
- `context`  
  - Ecosafety context, including:  
  - `verdict`: underlying `FogGuardVerdict`. [file:24]  
  - `roh`, `k`, `e`, `r`, `kerdeployable`, `corridor_present`: as above. [file:23][file:24]  

---

## 4. EcosafetyEnvelopeRequest / Response (companion duty)

Although defined in a separate module, `FogGuardQueryDuty` is typically used alongside `CyboquaticEcosafetyEnvelopeDuty`. [file:24][file:70]

### Request JSON

```json
{
  "type": "EcosafetyEnvelopeRequest",
  "nodeId": "string",
  "familyId": "string",
  "timeRange": {
    "start": "2026-07-09T01:00:00Z",
    "end": "2026-07-09T02:00:00Z"
  },
  "limit": 1
}
```

### Response JSON

```json
{
  "type": "EcosafetyEnvelopeResponse",
  "nodeId": "string",
  "familyId": "string",
  "envelopes": [
    {
      "lane": "Production",
      "risk": {
        "rcec": 0.12,
        "rsat": 0.05,
        "rsurcharge": 0.03,
        "rbiodiv": 0.08,
        "rvt": 0.02,
        "rgovernance": 0.01
      },
      "weights": {
        "wcec": 1.0,
        "wsat": 1.0,
        "wsurcharge": 1.0,
        "wbiodiv": 1.0,
        "wvt": 1.0,
        "wgovernance": 1.0
      },
      "residual": {
        "value": 0.0
      },
      "ker": {
        "k": 0.93,
        "e": 0.94,
        "r": 0.12,
        "kerdeployable": true
      },
      "evidencehex": "0x...",
      "did": "did:bostrom:..."
    }
  ]
}
```

- Fields mirror `CyboNodeEcosafetyEnvelope` and `KERWindow` from the Rust crate, with KER compressed to `k`, `e`, `r`, and `kerdeployable`. [file:24]

---

## 5. Example Workflow

A typical AI browser flow:

1. Fetch latest ecosafety envelope:

   ```json
   {
     "type": "EcosafetyEnvelopeRequest",
     "nodeId": "node-123",
     "familyId": "CyboquaticEcosafetyEnvelopePhoenix2026v1",
     "limit": 1
   }
   ```

2. Decide whether the route is allowed for a specific corridor:

   ```json
   {
     "type": "FogGuardEvaluateRequest",
     "nodeId": "node-123",
     "familyId": "CyboquaticEcosafetyEnvelopePhoenix2026v1",
     "corridorPresent": true,
     "guardConfig": null
   }
   ```

3. For router-level decisions, request a full route decision:

   ```json
   {
     "type": "FogRouteDecisionRequest",
     "nodeId": "node-123",
     "familyId": "CyboquaticEcosafetyEnvelopePhoenix2026v1",
     "windowId": "window-2026-07-09T01"
   }
   ```

In all cases, the browser stays non‑actuating and the Rust / ALN backend remains the executable authority for ecosafety and routing decisions. [file:23][file:24][file:70]
