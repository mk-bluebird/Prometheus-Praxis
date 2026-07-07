// src/browser/duties/cyboquatic-ecosafety-query-duty.js
// Javasphere query layer for cyboquatic ecosafety shards and frames.
// Turns high-level ecosystem questions into structured queries that
// a backend (Rust+SQLite+ALN) or MCP tool can service.

export class ShardSchemaQuery {
  constructor(options) {
    this.familyId = options?.familyId ?? "CyboquaticEcosafetyEnvelopePhoenix2026v1";
  }

  /**
   * Build a query object asking the backend to return the ALN-derived
   * ShardSchema and any validation hints for UI or agents.
   */
  buildSchemaRequest() {
    return {
      type: "ShardSchemaRequest",
      familyId: this.familyId,
      expect: {
        fields: true,
        tags: true,
        riskAxes: true,
        allowedFrames: [
          "BiodiversityIntegrityFrame",
          "MesocosmRiskFrame",
          "LyapunovStabilityFrame"
        ]
      }
    };
  }

  /**
   * Build a validation request to check a candidate row/update
   * against the ALN ShardSchema before it ever hits SQLite.
   */
  buildValidateUpdateRequest(shardUpdate) {
    return {
      type: "ShardUpdateValidationRequest",
      familyId: this.familyId,
      payload: shardUpdate
    };
  }
}

export class WindowManagerQuery {
  constructor(options) {
    this.nodeId = options?.nodeId ?? null;
    this.windowSize = options?.windowSize ?? 24; // hours, for example
    this.windowStride = options?.windowStride ?? 6; // sliding stride
  }

  /**
   * Ask the backend WindowManager to buffer NodeRiskSample records
   * and emit fixed or sliding windows for a given node.
   */
  buildWindowRequest(mode = "sliding") {
    if (!this.nodeId) {
      throw new Error("WindowManagerQuery requires nodeId");
    }

    return {
      type: "NodeWindowRequest",
      nodeId: this.nodeId,
      mode,
      windowSize: this.windowSize,
      windowStride: this.windowStride,
      metrics: [
        "r_overall",
        "r_biodiv",
        "r_pfas",
        "r_cec",
        "r_trap_fish",
        "r_trap_amphib",
        "vt_lyap"
      ]
    };
  }
}

export class EcosafetyStatusHistoryQuery {
  constructor(options) {
    this.nodeId = options?.nodeId ?? null;
    this.historyDepth = options?.historyDepth ?? 72; // last 72 hours
  }

  /**
   * Request GREEN/WARN/RED ring-buffer plus trend tag
   * ("improving" | "stable" | "degrading") for a node.
   */
  buildStatusHistoryRequest() {
    if (!this.nodeId) {
      throw new Error("EcosafetyStatusHistoryQuery requires nodeId");
    }

    return {
      type: "EcosafetyStatusHistoryRequest",
      nodeId: this.nodeId,
      historyDepth: this.historyDepth
    };
  }
}

export class BiodiversityIntegrityFrameQuery {
  constructor(options) {
    this.nodeId = options?.nodeId ?? null;
    this.windowSize = options?.windowSize ?? 24;
  }

  /**
   * Ask backend to refine r_biodiv using PFAS, CEC, trap-fish, trap-amphib.
   * This is purely a query envelope; backend encodes the math.
   */
  buildFrameRequest() {
    if (!this.nodeId) {
      throw new Error("BiodiversityIntegrityFrameQuery requires nodeId");
    }

    return {
      type: "RiskFrameRequest",
      frame: "BiodiversityIntegrityFrame",
      nodeId: this.nodeId,
      windowSize: this.windowSize,
      inputs: [
        "r_biodiv_raw",
        "r_pfas",
        "r_cec",
        "r_trap_fish",
        "r_trap_amphib"
      ],
      outputMetric: "r_biodiv"
    };
  }
}

export class MesocosmRiskFrameQuery {
  constructor(options) {
    this.nodeId = options?.nodeId ?? null;
    this.mesocosmFamilyId = options?.mesocosmFamilyId ?? "CyboquaticMesocosm2027v1";
    this.lookaheadHours = options?.lookaheadHours ?? 48;
  }

  /**
   * Request mesocosm-driven updates to r_invasive without widening corridors.
   */
  buildFrameRequest() {
    if (!this.nodeId) {
      throw new Error("MesocosmRiskFrameQuery requires nodeId");
    }

    return {
      type: "RiskFrameRequest",
      frame: "MesocosmRiskFrame",
      nodeId: this.nodeId,
      mesocosmFamilyId: this.mesocosmFamilyId,
      lookaheadHours: this.lookaheadHours,
      inputs: [
        "r_invasive_raw",
        "mesocosm_risk_index"
      ],
      outputMetric: "r_invasive"
    };
  }
}

export class LyapunovStabilityFrameQuery {
  constructor(options) {
    this.nodeId = options?.nodeId ?? null;
    this.historyDepth = options?.historyDepth ?? 168; // hours of Vt history
  }

  /**
   * Request Lyapunov exponents and change-point scores from Vt history
   * to flag pre-failure regimes before ecosafety distance crosses thresholds.
   */
  buildFrameRequest() {
    if (!this.nodeId) {
      throw new Error("LyapunovStabilityFrameQuery requires nodeId");
    }

    return {
      type: "RiskFrameRequest",
      frame: "LyapunovStabilityFrame",
      nodeId: this.nodeId,
      historyDepth: this.historyDepth,
      inputs: [
        "vt_history"
      ],
      outputs: [
        "lyapunov_exponent",
        "change_point_score",
        "pre_failure_flag"
      ]
    };
  }
}

/**
 * High-level facade for AI browsers:
 * turns natural-language intentions into structured requests Javasphere
 * can send over MCP or HTTP to your Rust/SQLite gateway.
 */
export class CyboquaticEcosafetyQueryDuty {
  constructor(transport) {
    // `transport` is an injected function: async (request) => response
    this.transport = transport;
  }

  async getShardSchema(familyId) {
    const q = new ShardSchemaQuery({ familyId });
    return this.transport(q.buildSchemaRequest());
  }

  async validateShardUpdate(familyId, shardUpdate) {
    const q = new ShardSchemaQuery({ familyId });
    return this.transport(q.buildValidateUpdateRequest(shardUpdate));
  }

  async getNodeWindows(nodeId, options) {
    const q = new WindowManagerQuery({ nodeId, ...options });
    return this.transport(q.buildWindowRequest(options?.mode ?? "sliding"));
  }

  async getStatusHistory(nodeId, options) {
    const q = new EcosafetyStatusHistoryQuery({ nodeId, ...options });
    return this.transport(q.buildStatusHistoryRequest());
  }

  async getBiodiversityFrame(nodeId, options) {
    const q = new BiodiversityIntegrityFrameQuery({ nodeId, ...options });
    return this.transport(q.buildFrameRequest());
  }

  async getMesocosmFrame(nodeId, options) {
    const q = new MesocosmRiskFrameQuery({ nodeId, ...options });
    return this.transport(q.buildFrameRequest());
  }

  async getLyapunovFrame(nodeId, options) {
    const q = new LyapunovStabilityFrameQuery({ nodeId, ...options });
    return this.transport(q.buildFrameRequest());
  }
}
