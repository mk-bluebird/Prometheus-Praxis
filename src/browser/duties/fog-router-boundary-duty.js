// src/browser/duties/fog-router-boundary-duty.js
// High-level Javasphere facade combining ecosafety envelopes and FOG guard decisions.

import {
  FogGuardQueryDuty,
} from "./fog-guard-query-duty.js";

import {
  CyboquaticEcosafetyEnvelopeDuty,
} from "./cyboquatic-ecosafety-envelope-duty.js";

export class FogRouterBoundaryDuty {
  /**
   * @param {(request: any) => Promise<any>} transport
   *   Shared transport used by underlying duties.
   */
  constructor(transport) {
    this.transport = transport;
    this.fogGuardDuty = new FogGuardQueryDuty(transport);
    this.ecosafetyDuty = new CyboquaticEcosafetyEnvelopeDuty(transport);
  }

  /**
   * Fetch the latest ecosafety envelope for a node and compute a fog guard verdict.
   *
   * This is a convenience method for AI browsers:
   * - getEnvelopes(limit=1),
   * - call evaluateFogGuard with the corridor flag provided by the caller,
   * - return both the envelope and verdict in one response.
   */
  async getEnvelopeAndVerdict(options) {
    const { nodeId, corridorPresent, familyId, guardConfig } = options;

    const envelopes = await this.ecosafetyDuty.getEnvelopes({
      nodeId,
      familyId,
      limit: 1,
    });

    const envelope = Array.isArray(envelopes) && envelopes.length > 0
      ? envelopes[0]
      : null;

    if (!envelope) {
      return {
        envelope: null,
        verdict: null,
        error: "No envelope available for nodeId",
      };
    }

    const verdict = await this.fogGuardDuty.evaluateFogGuard({
      nodeId,
      familyId,
      corridorPresent,
      guardConfig,
    });

    return {
      envelope,
      verdict,
    };
  }

  /**
   * Ask for a full route decision and ecosafety context.
   *
   * The backend is expected to:
   * - compute FogRouteDecision,
   * - include KER and residual summary in the response.
   */
  async getRouteDecision(options) {
    const { nodeId, familyId, windowId, guardConfig } = options;

    const decision = await this.fogGuardDuty.decideFogRoute({
      nodeId,
      familyId,
      windowId,
      guardConfig,
    });

    return decision;
  }
}
