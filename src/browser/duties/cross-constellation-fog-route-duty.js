// src/browser/duties/cross-constellation-fog-route-duty.js
// Javasphere duty for cross-constellation FOG route evaluations.

export class CrossConstellationFogRouteRequestBuilder {
  constructor(options) {
    this.nodeId = options.nodeId;
    this.originConstellation = options.originConstellation;
    this.targetConstellation = options.targetConstellation;
    this.workflowId = options.workflowId;
    this.familyId =
      options.familyId ?? "CyboquaticEcosafetyEnvelopePhoenix2026v1";
    this.windowId = options.windowId ?? null;
    this.guardConfig = options.guardConfig ?? null;
  }

  build() {
    if (!this.nodeId) {
      throw new Error("CrossConstellationFogRouteRequest requires nodeId");
    }
    if (!this.originConstellation) {
      throw new Error(
        "CrossConstellationFogRouteRequest requires originConstellation"
      );
    }
    if (!this.targetConstellation) {
      throw new Error(
        "CrossConstellationFogRouteRequest requires targetConstellation"
      );
    }
    if (!this.workflowId) {
      throw new Error("CrossConstellationFogRouteRequest requires workflowId");
    }

    return {
      type: "CrossConstellationFogRouteRequest",
      nodeId: this.nodeId,
      originConstellation: this.originConstellation,
      targetConstellation: this.targetConstellation,
      workflowId: this.workflowId,
      familyId: this.familyId,
      windowId: this.windowId,
      guardConfig: this.guardConfig,
    };
  }
}

export class CrossConstellationFogRouteDuty {
  /**
   * @param {(request: any) => Promise<any>} transport
   *   Non-actuating transport to Rust/MCP gateway.
   */
  constructor(transport) {
    this.transport = transport;
  }

  /**
   * Evaluate a cross-constellation FOG route for a workflow:
   * - computes r_W via cross-constellation-index,
   * - applies EcoHamiltonian gate,
   * - if accepted, runs local fog-router-guard/FOG guard,
   * - returns CrossConstellationFogRouteResponse.
   */
  async evaluate(options) {
    const req = new CrossConstellationFogRouteRequestBuilder(options).build();
    return this.transport(req);
  }
}
