// src/browser/duties/cyboquatic-ecosafety-envelope-duty.js
// Javasphere duty for querying ecosafety envelopes and KER windows.

export class EcosafetyEnvelopeRequest {
  constructor(options) {
    this.nodeId = options.nodeId ?? null;
    this.familyId =
      options.familyId ?? "CyboquaticEcosafetyEnvelopePhoenix2026v1";
    this.timeRange = options.timeRange ?? null; // { start: iso, end: iso }
    this.limit = options.limit ?? 1;
  }

  build() {
    if (!this.nodeId) {
      throw new Error("EcosafetyEnvelopeRequest requires nodeId");
    }

    return {
      type: "EcosafetyEnvelopeRequest",
      nodeId: this.nodeId,
      familyId: this.familyId,
      timeRange: this.timeRange,
      limit: this.limit,
    };
  }
}

export class KerWindowRequest {
  constructor(options) {
    this.nodeId = options.nodeId ?? null;
    this.familyId =
      options.familyId ?? "CyboquaticEcosafetyEnvelopePhoenix2026v1";
    this.windowSize = options.windowSize ?? 48; // steps or hours, backend-defined
  }

  build() {
    if (!this.nodeId) {
      throw new Error("KerWindowRequest requires nodeId");
    }

    return {
      type: "KerWindowRequest",
      nodeId: this.nodeId,
      familyId: this.familyId,
      windowSize: this.windowSize,
    };
  }
}

export class CyboquaticEcosafetyEnvelopeDuty {
  /**
   * @param {(request: any) => Promise<any>} transport
   */
  constructor(transport) {
    this.transport = transport;
  }

  /**
   * Fetch one or more CyboNodeEcosafetyEnvelope rows for a node.
   * The backend should return:
   * - lane, risk vector, residual, KER window, evidencehex, DID.
   */
  async getEnvelopes(options) {
    const req = new EcosafetyEnvelopeRequest(options).build();
    return this.transport(req);
  }

  /**
   * Fetch a KER window summary for a node.
   * The backend should return K, E, R and kerdeployable flags.
   */
  async getKerWindow(options) {
    const req = new KerWindowRequest(options).build();
    return this.transport(req);
  }
}
