// src/browser/duties/fog-guard-query-duty.js
// Javasphere duty for querying FOG guard and safestep decisions.
// Non-actuating: produces only JSON contracts for a Rust/MCP gateway.

export class FogGuardEvaluateRequest {
  constructor(options) {
    this.nodeId = options.nodeId;
    this.familyId =
      options.familyId ?? "CyboquaticEcosafetyEnvelopePhoenix2026v1";
    this corridorPresent = options.corridorPresent;
    this.guardConfig = options.guardConfig ?? null;
  }

  build() {
    if (!this.nodeId) {
      throw new Error("FogGuardEvaluateRequest requires nodeId");
    }
    if (typeof this.corridorPresent !== "boolean") {
      throw new Error("FogGuardEvaluateRequest requires corridorPresent boolean");
    }

    return {
      type: "FogGuardEvaluateRequest",
      nodeId: this.nodeId,
      familyId: this.familyId,
      corridorPresent: this.corridorPresent,
      guardConfig: this.guardConfig,
    };
  }
}

export class FogRouteDecisionRequest {
  constructor(options) {
    this.nodeId = options.nodeId;
    this.familyId =
      options.familyId ?? "CyboquaticEcosafetyEnvelopePhoenix2026v1";
    this.windowId = options.windowId ?? null;
    this.guardConfig = options.guardConfig ?? null;
  }

  build() {
    if (!this.nodeId) {
      throw new Error("FogRouteDecisionRequest requires nodeId");
    }

    return {
      type: "FogRouteDecisionRequest",
      nodeId: this.nodeId,
      familyId: this.familyId,
      windowId: this.windowId,
      guardConfig: this.guardConfig,
    };
  }
}

export class FogGuardQueryDuty {
  /**
   * @param {(request: any) => Promise<any>} transport
   *   Injected function that sends requests to a backend Rust/MCP gateway.
   *   The transport must be non-actuating and side-effect free.
   */
  constructor(transport) {
    this.transport = transport;
  }

  /**
   * Ask the backend to run safestep for a given node and corridor flag.
   * The backend is expected to:
   * - fetch the latest CyboNodeEcosafetyEnvelope for nodeId,
   * - call safestep(envelope, corridorPresent, cfg),
   * - return FogGuardVerdict::Allow|Stop as a JSON string or enum.
   */
  async evaluateFogGuard(options) {
    const req = new FogGuardEvaluateRequest(options).build();
    return this.transport(req);
  }

  /**
   * Ask the backend FOG router guard to compute a FogRouteDecision
   * for a given node and (optional) windowId.
   *
   * The backend is expected to:
   * - build a FogNodeSnapshot from ecosafety shards,
   * - call decide_route(snapshot, cfg),
   * - return FogRouteDecision::AllowRoute|BlockRoute as JSON.
   */
  async decideFogRoute(options) {
    const req = new FogRouteDecisionRequest(options).build();
    return this.transport(req);
  }
}
